#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include "lv_gnss_settings.h"

// all objects in settings
static  lv_obj_t * btn;

static battery_indicator_t battery_ui;

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

    // battery indicator
    create_battery_indicator(parent);

    bsp_display_unlock();
}

void update_battery(battery_indicator_t * battery, int32_t percentage, bool is_charging)
{
    // 1. Update the bar fill level smoothly
    lv_bar_set_value(battery->bar, percentage, LV_ANIM_ON);

    // 2. Dynamic indicator coloring based on battery state
    if (is_charging) {
        lv_obj_set_style_bg_color(battery->bar, lv_palette_main(LV_PALETTE_LIGHT_GREEN), LV_PART_INDICATOR);
    } else if (percentage <= 20) {
        lv_obj_set_style_bg_color(battery->bar, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    } else if (percentage <= 50) {
        lv_obj_set_style_bg_color(battery->bar, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR);
    } else {
        lv_obj_set_style_bg_color(battery->bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    }

    // 3. Update the text label string
    char buf[16];
    if (is_charging) {
        snprintf(buf, sizeof(buf), "%"   LV_PRId32 "%% >>", percentage);
    } else {
        snprintf(buf, sizeof(buf), "%" LV_PRId32 "%%", percentage);
    }
    lv_label_set_text(battery->label, buf);
}

void create_battery_indicator(lv_obj_t * parent)
{
    /* 1. Create a container for the battery layout */
    lv_obj_t * battery_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(battery_cont);
    lv_obj_set_size(battery_cont, 91, 40);
    lv_obj_center(battery_cont);

    /* 2. Create the positive terminal tip on the right edge */
    lv_obj_t * tip = lv_obj_create(battery_cont);
    lv_obj_remove_style_all(tip);
    lv_obj_set_size(tip, 6, 16);
    lv_obj_align(tip, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(tip, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tip, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(tip, 3, LV_PART_MAIN);

    /* 3. Create the battery body using lv_bar */
    battery_ui.bar = lv_bar_create(battery_cont);
    lv_obj_set_size(battery_ui.bar, 85, 40);
    lv_obj_align(battery_ui.bar, LV_ALIGN_LEFT_MID, 0, 0);

    /* Style the outer shell background */
    lv_obj_set_style_bg_color(battery_ui.bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_color(battery_ui.bar, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_border_width(battery_ui.bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(battery_ui.bar, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(battery_ui.bar, 4, LV_PART_MAIN); // Margin for inner fluid layer

    /* Style the inner moving fluid indicator */
    lv_obj_set_style_radius(battery_ui.bar, 3, LV_PART_INDICATOR);

    /* 4. Create the text percentage label centered inside the bar */
    battery_ui.label = lv_label_create(battery_ui.bar);
    lv_obj_center(battery_ui.label);

    /* Style the text layout */
    lv_obj_set_style_text_color(battery_ui.label, lv_color_white(), LV_PART_MAIN);
    // Use standard micro fonts (like montserrat_12 or 14) depending on your lv_conf.h
    lv_obj_set_style_text_font(battery_ui.label, &lv_font_montserrat_14, LV_PART_MAIN);

    /* Set initial runtime test values (65% power, not charging) */
    update_battery(&battery_ui, 65, false);
}

void lv_gnss_settings_set_battery_values(int32_t percentage, bool is_charging) {
    update_battery(&battery_ui, percentage, is_charging);
}

