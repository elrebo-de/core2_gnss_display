#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include "lv_gnss_settings.h"

// all objects in settings
static lv_obj_t * btn;
static battery_indicator_t battery_ui;
static lv_obj_t * brightness;
static lv_obj_t * label_1;

static lv_subject_t * brightness_subject_value;

void lv_gnss_settings_init(lv_obj_t *parent, lv_event_cb_t powerOffCb, lv_event_cb_t brightnessSliderCb, uint32_t brightness_value)
{
    bsp_display_lock(-1);

    // "Off" button
    btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, 10, 10);
    lv_obj_set_size(btn, 60, 40);

    // Event-Callback an den Button hängen
    lv_obj_add_event_cb(btn, powerOffCb, LV_EVENT_ALL, NULL);

    // Text auf dem Button platzieren
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Off");
    lv_obj_center(label);

    bsp_display_unlock();

    // battery indicator
    create_battery_indicator(parent);

    // display brightness
    create_brightness_slider(parent, brightnessSliderCb, brightness_value);
}

void set_battery_indicator(battery_indicator_t * battery, int32_t percentage, bool is_charging)
{
    bsp_display_lock(-1);

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
        snprintf(buf, sizeof(buf), "%ld%% >>", percentage);
    } else {
        snprintf(buf, sizeof(buf), "%ld%%", percentage);
    }
    lv_label_set_text(battery->label, buf);

    bsp_display_unlock();
}

void create_battery_indicator(lv_obj_t * parent)
{
    bsp_display_lock(-1);
    /* 1. Create a container for the battery layout */
    lv_obj_t * battery_cont = lv_obj_create(parent);
    lv_obj_remove_style_all(battery_cont);
    lv_obj_set_size(battery_cont, 91, 40);
    lv_obj_align(battery_cont, LV_ALIGN_TOP_RIGHT, -10 , 10);

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

    bsp_display_unlock();

    /* Set initial runtime test values (65% power, not charging) */
    set_battery_indicator(&battery_ui, 65, false);
}

void lv_gnss_settings_set_battery_values(int32_t percentage, bool is_charging) {
    set_battery_indicator(&battery_ui, percentage, is_charging);
}

void set_brightness(int32_t brightness_value)
{
    bsp_display_lock(-1);
    lv_slider_set_value(brightness, brightness_value, LV_ANIM_OFF);
    bsp_display_unlock();
}

void create_brightness_slider(lv_obj_t * parent, lv_event_cb_t brightnessSliderCb, uint32_t brightness_value)
{
    static bool inited = false;
    if(!inited) {
        brightness_subject_value = lv_subject_create(LV_SUBJECT_TYPE_INT);
        lv_subject_set_min_value_int(brightness_subject_value, 15);
        lv_subject_set_max_value_int(brightness_subject_value, 100);
        lv_subject_set_int(brightness_subject_value, brightness_value);
        inited = true;
    }
    bsp_display_lock(-1);
    /* 1. Create a container for the battery layout */
    brightness = lv_slider_create(parent);
    lv_obj_set_size(brightness, lv_pct(90), 20);
    lv_slider_set_min_value(brightness, 15);
    lv_slider_set_max_value(brightness, 100);
    lv_slider_set_orientation(brightness, LV_SLIDER_ORIENTATION_HORIZONTAL);
    lv_obj_align(brightness, LV_ALIGN_CENTER, 0 , 0);
    lv_slider_bind_value(brightness, brightness_subject_value);
    lv_obj_add_event_cb(brightness, brightnessSliderCb, LV_EVENT_RELEASED, NULL);

    label_1 = lv_label_create(brightness);
    lv_obj_set_align(label_1, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(label_1, lv_color_black(), 0);
    lv_label_bind_text(label_1, brightness_subject_value, "Brightness %d%%");

    bsp_display_unlock();
}

