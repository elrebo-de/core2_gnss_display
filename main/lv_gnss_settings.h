#ifndef LV_GNSS_SETTINGS_H
#define LV_GNSS_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lv_gnss_settings_init(lv_obj_t *parent, lv_event_cb_t powerOffCb);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_GNSS_SETTINGS_H*/
