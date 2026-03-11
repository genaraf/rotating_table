#include <stdint.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#include "board_config.h"
#include "app_state.h"
#include "motor_control.h"

#define HOME_RPM            2.0f
#define HOME_TILT_SPEED_DPS 30.0f

static float s_step_phase = 0.0f;
static int s_step_idx = 0;
static int s_tilt_dir = 1;
static int32_t s_total_steps = 0;

static const uint8_t s_stepper_seq[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1},
};

static void update_table_angle(void) {
    float a = ((float)s_total_steps * 360.0f) / STEPPER_STEPS_PER_REV;
    a = fmodf(a, 360.0f);
    if (a < 0.0f) a += 360.0f;
    g_table_angle_deg = a;
}

static void stepper_write_phase(int idx) {
    gpio_set_level(PIN_STEPPER_IN1, s_stepper_seq[idx][0]);
    gpio_set_level(PIN_STEPPER_IN2, s_stepper_seq[idx][1]);
    gpio_set_level(PIN_STEPPER_IN3, s_stepper_seq[idx][2]);
    gpio_set_level(PIN_STEPPER_IN4, s_stepper_seq[idx][3]);
}

static void stepper_release(void) {
    gpio_set_level(PIN_STEPPER_IN1, 0);
    gpio_set_level(PIN_STEPPER_IN2, 0);
    gpio_set_level(PIN_STEPPER_IN3, 0);
    gpio_set_level(PIN_STEPPER_IN4, 0);
}

static void servo_setup(void) {
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_PERIOD_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));

    ledc_channel_config_t c = {
        .gpio_num = PIN_SERVO_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

static void servo_set_angle(float angle_deg) {
    float span = g_cfg.max_angle - g_cfg.min_angle;
    float t = span > 0.0f ? (angle_deg - g_cfg.min_angle) / span : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float pulse_us = SERVO_MIN_US + t * (SERVO_MAX_US - SERVO_MIN_US);
    uint32_t max_duty = (1u << 14) - 1;
    uint32_t duty = (uint32_t)((pulse_us * SERVO_PERIOD_HZ * max_duty) / 1000000.0f);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void run_home_step(float dt_s) {
    float steps_per_sec = (HOME_RPM * STEPPER_STEPS_PER_REV) / 60.0f;
    s_step_phase += steps_per_sec * dt_s;

    while (s_step_phase >= 1.0f) {
        s_step_phase -= 1.0f;

        if (s_total_steps == 0) {
            break;
        }

        int dir = (s_total_steps > 0) ? -1 : 1;
        s_step_idx = (s_step_idx + (dir > 0 ? 1 : 7)) & 0x7;
        stepper_write_phase(s_step_idx);
        s_total_steps += dir;
    }

    update_table_angle();

    float d = -g_current_angle;
    float max_step = HOME_TILT_SPEED_DPS * dt_s;
    if (d > max_step) d = max_step;
    if (d < -max_step) d = -max_step;
    g_current_angle += d;
    servo_set_angle(g_current_angle);

    if (s_total_steps == 0 && fabsf(g_current_angle) < 0.2f) {
        g_current_angle = 0.0f;
        g_table_angle_deg = 0.0f;
        servo_set_angle(g_current_angle);
        g_home_requested = false;
        g_cfg.power_on = false;
        cfg_save();
    }
}

static void run_normal_step(float dt_s) {
    float steps_per_sec = (g_cfg.rpm * STEPPER_STEPS_PER_REV) / 60.0f;
    s_step_phase += steps_per_sec * dt_s;
    int stepped = 0;

    while (s_step_phase >= 1.0f) {
        s_step_phase -= 1.0f;
        int dir = (STEPPER_DIR > 0) ? 1 : -1;
        s_step_idx = (s_step_idx + (dir > 0 ? 1 : 7)) & 0x7;
        stepper_write_phase(s_step_idx);
        s_total_steps += dir;
        stepped++;
    }

    if (stepped > 0) {
        float rev_delta = (float)stepped / STEPPER_STEPS_PER_REV;
        float angle_delta = rev_delta * g_cfg.turn_grad * (float)s_tilt_dir;
        g_current_angle += angle_delta;

        if (g_current_angle >= g_cfg.max_angle) {
            g_current_angle = g_cfg.max_angle;
            s_tilt_dir = -1;
        } else if (g_current_angle <= g_cfg.min_angle) {
            g_current_angle = g_cfg.min_angle;
            s_tilt_dir = 1;
        }

        servo_set_angle(g_current_angle);
    }

    update_table_angle();
}

static void motion_task(void *arg) {
    const TickType_t dt_ticks = pdMS_TO_TICKS(20);
    const float dt_s = 0.02f;

    while (1) {
        if (g_home_requested) {
            run_home_step(dt_s);
        } else if (g_cfg.power_on) {
            run_normal_step(dt_s);
        } else {
            stepper_release();
        }

        vTaskDelay(dt_ticks);
    }
}

void motor_control_init(void) {
    gpio_config_t out_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << PIN_STEPPER_IN1) | (1ULL << PIN_STEPPER_IN2) |
                        (1ULL << PIN_STEPPER_IN3) | (1ULL << PIN_STEPPER_IN4),
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));

    g_current_angle = 0.0f;
    if (g_current_angle < g_cfg.min_angle) g_current_angle = g_cfg.min_angle;
    if (g_current_angle > g_cfg.max_angle) g_current_angle = g_cfg.max_angle;
    g_table_angle_deg = 0.0f;

    servo_setup();
    servo_set_angle(g_current_angle);
}

void motor_control_start_task(void) {
    xTaskCreate(motion_task, "motion_task", 4096, NULL, 8, NULL);
}
