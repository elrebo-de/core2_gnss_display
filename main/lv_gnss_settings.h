#ifndef LV_GNSS_SETTINGS_H
#define LV_GNSS_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Extended structure to pass both the bar and label to updates easily
typedef struct {
    lv_obj_t * bar;
    lv_obj_t * label;
} battery_indicator_t;

void lv_gnss_settings_set_battery_values(int32_t percentage, bool is_charging);
void lv_gnss_settings_init(lv_obj_t *parent, lv_event_cb_t powerOffCb);

void update_battery(battery_indicator_t * battery, int32_t percentage, bool is_charging);
void create_battery_indicator(lv_obj_t * parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GNSS_SETTINGS_H*/
