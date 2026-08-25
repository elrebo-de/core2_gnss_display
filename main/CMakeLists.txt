idf_component_register( SRCS "main.cpp" "lv_gnss_cockpit.c" "basic_map_display.c" "lv_gnss_settings.c"
                        INCLUDE_DIRS "."
                        REQUIRES esp_psram
                      )
