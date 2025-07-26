idf_component_register(
    SRC_DIRS 
        "."
        "APP"
    INCLUDE_DIRS 
        "."
        "APP"
    REQUIRES
        esp_wifi
        esp_http_client
        nvs_flash
        esp_event
        esp32-camera
        esp-dl
        BSP
        fb_gfx
        modules)

