#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

#include "board_config.h"
#include "app_state.h"
#include "button_control.h"

static const char *TAG = "button";

static void button_task(void *arg) {
#if PIN_BTN_POWER == PIN_BTN_RESET
    int hold_ms = 0;
    int prev = 1;
    bool reset_done = false;

    while (1) {
        int p = gpio_get_level(PIN_BTN_POWER);
        if (p == 0) {
            hold_ms += 50;
            if (!reset_done && hold_ms > 5000) {
                reset_done = true;
                ESP_LOGW(TAG, "Factory reset by long press");
                nvs_flash_erase();
                esp_restart();
            }
        } else {
            if (prev == 0 && hold_ms > 50 && hold_ms <= 5000 && !reset_done) {
                g_cfg.power_on = !g_cfg.power_on;
                cfg_save();
                ESP_LOGI(TAG, "Power toggled: %d", g_cfg.power_on);
            }
            hold_ms = 0;
            reset_done = false;
        }

        prev = p;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
#else
    int reset_hold_ms = 0;
    int prev_power = 1;
    int prev_reset = 1;

    while (1) {
        int p = gpio_get_level(PIN_BTN_POWER);
        int r = gpio_get_level(PIN_BTN_RESET);

        if (prev_power == 1 && p == 0) {
            g_cfg.power_on = !g_cfg.power_on;
            cfg_save();
            ESP_LOGI(TAG, "Power toggled: %d", g_cfg.power_on);
        }

        if (r == 0) {
            reset_hold_ms += 50;
            if (reset_hold_ms > 5000) {
                ESP_LOGW(TAG, "Factory reset by button hold");
                nvs_flash_erase();
                esp_restart();
            }
        } else {
            if (prev_reset == 0 && reset_hold_ms > 100) {
                ESP_LOGI(TAG, "Reset button short press ignored");
            }
            reset_hold_ms = 0;
        }

        prev_power = p;
        prev_reset = r;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
#endif
}

void button_control_init(void) {
    gpio_config_t in_cfg = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pin_bit_mask = (1ULL << PIN_BTN_POWER) | (1ULL << PIN_BTN_RESET),
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));
}

void button_control_start_task(void) {
    xTaskCreate(button_task, "button_task", 3072, NULL, 6, NULL);
}
