#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "app_state.h"
#include "wifi_manager.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAX_RETRY     10

static const char *TAG = "wifi";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static esp_event_handler_instance_t s_any_id_inst;
static esp_event_handler_instance_t s_got_ip_inst;

static size_t copy_cstr_to_u8(uint8_t *dst, size_t dst_sz, const char *src) {
    if (dst_sz == 0) return 0;
    size_t n = strnlen(src, dst_sz - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
    return n;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_start_ap(void) {
    wifi_config_t ap_cfg = {0};
    size_t ssid_len = copy_cstr_to_u8(ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), g_cfg.ap_ssid);
    size_t pass_len = copy_cstr_to_u8(ap_cfg.ap.password, sizeof(ap_cfg.ap.password), g_cfg.ap_pass);
    ap_cfg.ap.authmode = pass_len >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.ssid_len = ssid_len;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "AP started: %s", g_cfg.ap_ssid);
}

static void wifi_start_sta_with_fallback(void) {
    wifi_config_t sta_cfg = {0};
    copy_cstr_to_u8(sta_cfg.sta.ssid, sizeof(sta_cfg.sta.ssid), g_cfg.sta_ssid);
    copy_cstr_to_u8(sta_cfg.sta.password, sizeof(sta_cfg.sta.password), g_cfg.sta_pass);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "STA connected to %s", g_cfg.sta_ssid);
    } else {
        ESP_LOGW(TAG, "STA failed, fallback to AP");
        esp_wifi_stop();
        wifi_start_ap();
    }
}

void wifi_manager_start(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_any_id_inst));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_got_ip_inst));

    if (g_cfg.wifi_mode == WIFI_MODE_AP_ONLY || strlen(g_cfg.sta_ssid) == 0) {
        wifi_start_ap();
    } else {
        wifi_start_sta_with_fallback();
    }
}
