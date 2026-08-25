#include "lvgl.h"
#include "bsp/esp-bsp.h"
#if LV_USE_SCALE && LV_BUILD_EXAMPLES

// all objects in settings
static  lv_obj_t * btn;

void lv_gnss_settings_init(lv_obj_t *parent, lv_event_cb_t powerOffCb)
{
    bsp_display_lock(0);

    // "Off" button
    btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, 10, 20);
    lv_obj_set_size(btn, 70, 50);

    // Event-Callback an den Button hängen
    lv_obj_add_event_cb(btn, powerOffCb, LV_EVENT_ALL, NULL);

    // Text auf dem Button platzieren
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Off");
    lv_obj_center(label);

    bsp_display_unlock();
}

#endif
