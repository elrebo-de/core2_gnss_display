#ifndef LV_COMPONENTS_H
#define LV_COMPONENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lv_gnss_display_set_current_values(int angle, int speed, int altitude, const char *latitude, const char *longitude, const char *date, const char *time, int nrOfSats);
void lv_gnss_display(lv_event_cb_t powerOffCb);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_COMPONENTS_H*/
