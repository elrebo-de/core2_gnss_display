#ifndef LV_COMPONENTS_H
#define LV_COMPONENTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lv_gnss_cockpit_set_current_values(int angle, int speed, int altitude, const char *latitudeDegMinSec, const char *latitudeDeg, const char *longitudeDegMinSec, const char *longitudeDeg, const char *date, const char *time, int nrOfSats);
void lv_gnss_cockpit_init(lv_obj_t *parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_COMPONENTS_H*/
