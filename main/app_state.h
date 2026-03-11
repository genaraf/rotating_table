#pragma once

#include <stdbool.h>

#include "esp_err.h"

#define MAX_PROFILES 8

typedef enum {
    WIFI_MODE_STA_FALLBACK_AP = 0,
    WIFI_MODE_AP_ONLY = 1,
} wifi_mode_sel_t;

typedef struct {
    char name[24];
    float rpm;
    int rotation_dir;
    float turn_grad;
    float min_angle;
    float max_angle;
} profile_t;

typedef struct {
    wifi_mode_sel_t wifi_mode;
    char sta_ssid[33];
    char sta_pass[65];
    char ap_ssid[33];
    char ap_pass[65];

    bool power_on;
    float rpm;
    int rotation_dir;
    float turn_grad;
    float min_angle;
    float max_angle;

    int profile_count;
    int active_profile;
    profile_t profiles[MAX_PROFILES];
} app_config_t;

extern app_config_t g_cfg;
extern float g_current_angle;
extern float g_table_angle_deg;
extern bool g_home_requested;

void cfg_clamp(app_config_t *cfg);
esp_err_t cfg_save(void);
void cfg_load(void);
void apply_profile(int idx);
