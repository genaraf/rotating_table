#pragma once

#include "driver/gpio.h"

// ---------------- Pins (adjust to your exact wiring) ----------------
#define PIN_STEPPER_IN1 GPIO_NUM_4
#define PIN_STEPPER_IN2 GPIO_NUM_5
#define PIN_STEPPER_IN3 GPIO_NUM_6
#define PIN_STEPPER_IN4 GPIO_NUM_7

#define PIN_SERVO_PWM   GPIO_NUM_8

// Buttons are active-low, connected to GND (internal pull-up is enabled).
// With this pin set both actions are on one button:
// short press -> power toggle, hold >5s -> factory reset.
#define PIN_BTN_POWER   GPIO_NUM_10
#define PIN_BTN_RESET   GPIO_NUM_10

// ---------------- Motion calibration ----------------
#define STEPPER_STEPS_PER_REV 4096.0f
#define STEPPER_DIR           1

#define SERVO_PERIOD_HZ 50
#define SERVO_MIN_US    500
#define SERVO_MAX_US    2500

// UI/runtime clamp range for tilt in degrees
#define TILT_MIN_DEG_LIMIT -45.0f
#define TILT_MAX_DEG_LIMIT 45.0f
