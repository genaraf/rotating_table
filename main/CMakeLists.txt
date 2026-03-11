idf_component_register(
    SRCS
        "main.c"
        "app_state.c"
        "motor_control.c"
        "button_control.c"
        "wifi_manager.c"
        "web_server.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server nvs_flash esp_wifi esp_event esp_netif driver json
)
