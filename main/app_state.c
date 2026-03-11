#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "nvs.h"

#include "board_config.h"
#include "app_state.h"

#define DEFAULT_STA_SSID ""
#define DEFAULT_STA_PASS ""
#define DEFAULT_AP_SSID  "RTable-Setup"
#define DEFAULT_AP_PASS  "12345678"

#define DEFAULT_RPM       2.0f
#define DEFAULT_ROT_DIR   1
#define DEFAULT_TURN_GRAD 10.0f
#define DEFAULT_MIN_ANGLE -45.0f
#define DEFAULT_MAX_ANGLE 45.0f
#define DEFAULT_ANGLE     0.0f

static const char *TAG = "app_state";

app_config_t g_cfg;
float g_current_angle = DEFAULT_ANGLE;
float g_table_angle_deg = 0.0f;
bool g_home_requested = false;

static void cfg_set_defaults(app_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->wifi_mode = WIFI_MODE_STA_FALLBACK_AP;
    snprintf(cfg->sta_ssid, sizeof(cfg->sta_ssid), "%s", DEFAULT_STA_SSID);
    snprintf(cfg->sta_pass, sizeof(cfg->sta_pass), "%s", DEFAULT_STA_PASS);
    snprintf(cfg->ap_ssid, sizeof(cfg->ap_ssid), "%s", DEFAULT_AP_SSID);
    snprintf(cfg->ap_pass, sizeof(cfg->ap_pass), "%s", DEFAULT_AP_PASS);

    cfg->power_on = false;
    cfg->rpm = DEFAULT_RPM;
    cfg->rotation_dir = DEFAULT_ROT_DIR;
    cfg->turn_grad = DEFAULT_TURN_GRAD;
    cfg->min_angle = DEFAULT_MIN_ANGLE;
    cfg->max_angle = DEFAULT_MAX_ANGLE;

    cfg->profile_count = 1;
    cfg->active_profile = 0;
    snprintf(cfg->profiles[0].name, sizeof(cfg->profiles[0].name), "default");
    cfg->profiles[0].rpm = cfg->rpm;
    cfg->profiles[0].rotation_dir = cfg->rotation_dir;
    cfg->profiles[0].turn_grad = cfg->turn_grad;
    cfg->profiles[0].min_angle = cfg->min_angle;
    cfg->profiles[0].max_angle = cfg->max_angle;
}

void cfg_clamp(app_config_t *cfg) {
    if (cfg->rpm < 0.1f) cfg->rpm = 0.1f;
    if (cfg->rpm > 12.0f) cfg->rpm = 12.0f;
    if (cfg->rotation_dir >= 0) cfg->rotation_dir = 1;
    else cfg->rotation_dir = -1;

    if (cfg->turn_grad < 0.0f) cfg->turn_grad = -cfg->turn_grad;
    if (cfg->turn_grad > 360.0f) cfg->turn_grad = 360.0f;

    if (cfg->min_angle < TILT_MIN_DEG_LIMIT) cfg->min_angle = TILT_MIN_DEG_LIMIT;
    if (cfg->max_angle > TILT_MAX_DEG_LIMIT) cfg->max_angle = TILT_MAX_DEG_LIMIT;
    if (cfg->max_angle < cfg->min_angle + 1.0f) cfg->max_angle = cfg->min_angle + 1.0f;

    if (cfg->profile_count < 1) cfg->profile_count = 1;
    if (cfg->profile_count > MAX_PROFILES) cfg->profile_count = MAX_PROFILES;

    if (cfg->active_profile < 0) cfg->active_profile = 0;
    if (cfg->active_profile >= cfg->profile_count) cfg->active_profile = cfg->profile_count - 1;

    for (int i = 0; i < cfg->profile_count; i++) {
        if (cfg->profiles[i].rpm < 0.1f) cfg->profiles[i].rpm = 0.1f;
        if (cfg->profiles[i].rpm > 12.0f) cfg->profiles[i].rpm = 12.0f;
        if (cfg->profiles[i].rotation_dir >= 0) cfg->profiles[i].rotation_dir = 1;
        else cfg->profiles[i].rotation_dir = -1;
        if (cfg->profiles[i].turn_grad < 0.0f) cfg->profiles[i].turn_grad = -cfg->profiles[i].turn_grad;
        if (cfg->profiles[i].turn_grad > 360.0f) cfg->profiles[i].turn_grad = 360.0f;
        if (cfg->profiles[i].min_angle < TILT_MIN_DEG_LIMIT) cfg->profiles[i].min_angle = TILT_MIN_DEG_LIMIT;
        if (cfg->profiles[i].max_angle > TILT_MAX_DEG_LIMIT) cfg->profiles[i].max_angle = TILT_MAX_DEG_LIMIT;
        if (cfg->profiles[i].max_angle < cfg->profiles[i].min_angle + 1.0f) {
            cfg->profiles[i].max_angle = cfg->profiles[i].min_angle + 1.0f;
        }
    }
}

esp_err_t cfg_save(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("rtable", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs, "cfg", &g_cfg, sizeof(g_cfg));
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

void cfg_load(void) {
    cfg_set_defaults(&g_cfg);

    nvs_handle_t nvs;
    size_t sz = sizeof(g_cfg);
    esp_err_t err = nvs_open("rtable", NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_get_blob(nvs, "cfg", &g_cfg, &sz);
    if (err != ESP_OK || sz != sizeof(g_cfg)) {
        ESP_LOGI(TAG, "No saved cfg, using defaults");
        cfg_set_defaults(&g_cfg);
    }

    nvs_close(nvs);
    cfg_clamp(&g_cfg);
}

void apply_profile(int idx) {
    if (idx < 0 || idx >= g_cfg.profile_count) return;

    profile_t *p = &g_cfg.profiles[idx];
    g_cfg.rpm = p->rpm;
    g_cfg.rotation_dir = p->rotation_dir;
    g_cfg.turn_grad = p->turn_grad;
    g_cfg.min_angle = p->min_angle;
    g_cfg.max_angle = p->max_angle;

    g_cfg.active_profile = idx;
    cfg_clamp(&g_cfg);
}
