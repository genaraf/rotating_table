#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "app_state.h"
#include "motor_control.h"
#include "button_control.h"
#include "wifi_manager.h"
#include "web_server.h"

static const char *TAG = "rtable";

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    cfg_load();
    g_current_angle = 0.0f;
    if (g_current_angle < g_cfg.min_angle) g_current_angle = g_cfg.min_angle;
    if (g_current_angle > g_cfg.max_angle) g_current_angle = g_cfg.max_angle;

    motor_control_init();
    button_control_init();

    wifi_manager_start();
    web_server_start();

    motor_control_start_task();
    button_control_start_task();

    ESP_LOGI(TAG, "RTable started");
}
