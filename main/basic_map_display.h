#ifndef BASIC_MAP_DISPLAY_H
#define BASIC_MAP_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "map_tiles.h"
#include "lvgl.h"
#include "esp_log.h"

void map_display_init(lv_obj_t * parent);
void map_display_load_location(double lat, double lon);
void map_display_set_tile_type(int tile_type, double lat, double lon);
void map_display_set_zoom(int zoom, double lat, double lon);
void map_display_add_marker(double lat, double lon);
void map_display_cleanup(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*BASIC_MAP_DISPLAY_H*/
