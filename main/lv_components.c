#include "lvgl.h"
#if LV_USE_SCALE && LV_BUILD_EXAMPLES

static lv_obj_t * scale;
static lv_obj_t * label;
static lv_obj_t * tacho;
static lv_obj_t * hoehe;
static lv_obj_t * breiteDegMinSec;
static lv_obj_t * laengeDegMinSec;
static lv_obj_t * breiteDeg;
static lv_obj_t * laengeDeg;
static lv_obj_t * uhrzeit;
static lv_obj_t * datum;
static lv_obj_t * anzahlSatelliten;

static  lv_obj_t * btn;

static const char * heading_to_cardinal(int32_t heading)
{
    /* Normalize heading to range [0, 360) */
    while(heading < 0) heading += 360;
    while(heading >= 360) heading -= 360;

    if(heading < 23) return "N";
    else if(heading < 68) return "NE";
    else if(heading < 113) return "E";
    else if(heading < 158) return "SE";
    else if(heading < 203) return "S";
    else if(heading < 248) return "SW";
    else if(heading < 293) return "W";
    else if(heading < 338) return "NW";

    return "N";
}

static void set_heading_value(void * obj, int32_t v)
{
    LV_UNUSED(obj);
    lv_scale_set_rotation(scale, 270 - v);
    lv_label_set_text_fmt(label, "%d°\n%s", (int)v, heading_to_cardinal(v));
}

void lv_gnss_display_set_current_values(int angle, int speed, int altitude, const char *latitudeDegMinSec, const char *latitudeDeg, const char *longitudeDegMinSec, const char *longitudeDeg, const char *date, const char *time, int nrOfSats)
{
    if (speed >= 2) {
        lv_scale_set_rotation(scale, 270 - angle);
        lv_label_set_text_fmt(label, "%d°\n%s", angle, heading_to_cardinal(angle));
    }
    lv_label_set_text_fmt(tacho, "%3d km/h", speed);
    lv_label_set_text_fmt(hoehe, "%3d m asl", altitude);
    lv_label_set_text_fmt(breiteDegMinSec, "%s", latitudeDegMinSec);
    lv_label_set_text_fmt(breiteDeg, "%s", latitudeDeg);
    lv_label_set_text_fmt(laengeDegMinSec, "%s", longitudeDegMinSec);
    lv_label_set_text_fmt(laengeDeg, "%s", longitudeDeg);
    lv_label_set_text_fmt(datum, "Date: %s", date);
    lv_label_set_text_fmt(uhrzeit, "Time: %s UTC", time);
    lv_label_set_text_fmt(anzahlSatelliten, "Nr of Sats: %d", nrOfSats);
}

static void draw_event_cb(lv_event_t * e)
{
    lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t * base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
    lv_draw_line_dsc_t * line_draw_dsc = lv_draw_task_get_line_dsc(draw_task);
    if(base_dsc->part == LV_PART_INDICATOR) {
        if(label_draw_dsc) {
            if(base_dsc->id1 == 0) {
                label_draw_dsc->color = lv_palette_main(LV_PALETTE_RED);
            }
        }
        if(line_draw_dsc) {
            if(base_dsc->id1 == 60) {
                line_draw_dsc->color = lv_palette_main(LV_PALETTE_RED);
            }
        }
    }
}

void lv_gnss_display(lv_event_cb_t powerOffCb)
{
    // Compass
    scale = lv_scale_create(lv_screen_active());

    lv_obj_set_size(scale, 150, 150);
    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
    // linke obere Ecke des Widgets setzen
    // lv_obj_set_pos(scale, 320 - 150 - 10, 10); // V1.0.0
    lv_obj_set_pos(scale, 320 - 150 - 10, 10 + 70); // V1.0.1
    //lv_obj_set_align(scale, LV_ALIGN_TOP_RIGHT);

    lv_scale_set_total_tick_count(scale, 61);
    lv_scale_set_major_tick_every(scale, 5);

    lv_obj_set_style_length(scale, 5, LV_PART_ITEMS);
    lv_obj_set_style_length(scale, 10, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(scale, 3, LV_PART_INDICATOR);
    lv_scale_set_range(scale, 0, 360);

    static const char * custom_labels[] = {"N", "30", "60", "E", "120", "150", "S", "210", "240", "W", "300", "330", NULL};
    lv_scale_set_text_src(scale, custom_labels);

    lv_scale_set_angle_range(scale, 360);
    lv_scale_set_rotation(scale, 270);

    lv_obj_add_event_cb(scale, draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_flag(scale, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

    // Winkel und Himmelsrichtung im Compass
    // Style für etwas größere Schrift erstellen
    static lv_style_t style_little_larger;
    lv_style_init(&style_little_larger);

    // Eingebaute 20px-Schriftart zuweisen (Standard ist meist 14px)
    lv_style_set_text_font(&style_little_larger, &lv_font_montserrat_20);

    label = lv_label_create(lv_screen_active());
    lv_obj_add_style(label, &style_little_larger, LV_PART_MAIN);
    lv_obj_set_width(label, 100);
    lv_obj_set_height(label, 100);
    // x: Breite -halbe Kompassbreite - Rand
    // y: halbe Kompasshöhe + Rand - Höhe der oberen Zeile
    // lv_obj_set_pos(label, 320 - 150/2 - 10 - 100/2, 150/2 + 10 - 20); // V1.0.0
    lv_obj_set_pos(label, 320 - 150/2 - 10 - 100/2, 150/2 + 10 - 20 + 70); // V1.0.1
    lv_label_set_text(label, "0°\nN");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    set_heading_value(NULL, 0);

    lv_obj_t * symbol = lv_label_create(scale);
    lv_obj_set_align(symbol, LV_ALIGN_TOP_MID);
    lv_obj_set_y(symbol, 5);
    lv_label_set_text(symbol, LV_SYMBOL_UP);
    lv_obj_set_style_text_align(symbol, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(symbol, lv_palette_main(LV_PALETTE_RED), 0);

    // Tacho mit Geschwindigkeit
    // Style für ganz große Schrift erstellen
    static lv_style_t style_very_large;
    lv_style_init(&style_very_large);

    // Eingebaute 48px-Schriftart zuweisen (Standard ist meist 14px)
    lv_style_set_text_font(&style_very_large, &lv_font_montserrat_48);

    tacho = lv_label_create(lv_screen_active());
    lv_obj_add_style(tacho, &style_very_large, LV_PART_MAIN);
    lv_obj_set_width(tacho, 300);
    lv_obj_set_height(tacho, 50);
    // x: 0 + Rand
    // y: Bildschirmhöhe - Schrifthöhe - Rand
    //lv_obj_set_pos(tacho, 10, 240-48-10); // V1.0.0
    lv_obj_set_pos(tacho, 10, 20); // V1.0.1
    lv_label_set_text_fmt(tacho, "%3d km/h", 0);
    lv_obj_set_style_text_align(tacho, LV_TEXT_ALIGN_RIGHT, 0);

    // Meereshöhe
    // Style für große Schrift erstellen
    static lv_style_t style_large;
    lv_style_init(&style_large);

    // Eingebaute 24px-Schriftart zuweisen (Standard ist meist 14px)
    lv_style_set_text_font(&style_large, &lv_font_montserrat_24);

    hoehe = lv_label_create(lv_screen_active());
    lv_obj_add_style(hoehe, &style_large, LV_PART_MAIN);
    lv_obj_set_width(hoehe, 140);
    lv_obj_set_height(hoehe, 30);
    // x: 0 + Rand
    // y: 0 + Rand
    lv_obj_set_pos(hoehe, 10, 80);
    lv_label_set_text_fmt(hoehe, "%3d m asl", 0);
    // lv_obj_set_style_text_align(hoehe, LV_TEXT_ALIGN_RIGHT, 0); // V1.0.0
    lv_obj_set_style_text_align(hoehe, LV_TEXT_ALIGN_LEFT, 0); // V1.0.1

    // Breite
    breiteDegMinSec = lv_label_create(lv_screen_active());
    lv_obj_set_width(breiteDegMinSec, 140);
    lv_obj_set_height(breiteDegMinSec, 15);
    // x: 0 + Rand
    // y: 30 + Rand
    lv_obj_set_pos(breiteDegMinSec, 10, 110);
    lv_label_set_text(breiteDegMinSec, "");
    lv_obj_set_style_text_align(breiteDegMinSec, LV_TEXT_ALIGN_LEFT, 0);

    // Laenge
    laengeDegMinSec = lv_label_create(lv_screen_active());
    lv_obj_set_width(laengeDegMinSec, 140);
    lv_obj_set_height(laengeDegMinSec, 15);
    // x: 0 + Rand
    // y: 30 + Rand
    lv_obj_set_pos(laengeDegMinSec, 10, 125);
    lv_label_set_text(laengeDegMinSec, "");
    lv_obj_set_style_text_align(laengeDegMinSec, LV_TEXT_ALIGN_LEFT, 0);

    // Breite
    breiteDeg = lv_label_create(lv_screen_active());
    lv_obj_set_width(breiteDeg, 140);
    lv_obj_set_height(breiteDeg, 15);
    // x: 0 + Rand
    // y: 30 + Rand
    lv_obj_set_pos(breiteDeg, 10, 145);
    lv_label_set_text(breiteDeg, "");
    lv_obj_set_style_text_align(breiteDeg, LV_TEXT_ALIGN_LEFT, 0);

    // Laenge
    laengeDeg = lv_label_create(lv_screen_active());
    lv_obj_set_width(laengeDeg, 140);
    lv_obj_set_height(laengeDeg, 15);
    // x: 0 + Rand
    // y: 30 + Rand
    lv_obj_set_pos(laengeDeg, 10, 160);
    lv_label_set_text(laengeDeg, "");
    lv_obj_set_style_text_align(laengeDeg, LV_TEXT_ALIGN_LEFT, 0);

    // datum
    datum = lv_label_create(lv_screen_active());
    lv_obj_set_width(datum, 140);
    lv_obj_set_height(datum, 15);
    // x: 0 + Rand
    // y: 30 + Rand
    lv_obj_set_pos(datum, 10, 180);
    lv_label_set_text(datum, "");
    lv_obj_set_style_text_align(datum, LV_TEXT_ALIGN_LEFT, 0);

    // uhrzeit
    uhrzeit = lv_label_create(lv_screen_active());
    lv_obj_set_width(uhrzeit, 140);
    lv_obj_set_height(uhrzeit, 15);
    // x: 0 + Rand
    // y: 30 + Rand
    lv_obj_set_pos(uhrzeit, 10, 195);
    lv_label_set_text(uhrzeit, "");
    lv_obj_set_style_text_align(uhrzeit, LV_TEXT_ALIGN_LEFT, 0);

    // Anzahl Satelliten
    anzahlSatelliten = lv_label_create(lv_screen_active());
    lv_obj_set_width(anzahlSatelliten, 140);
    lv_obj_set_height(anzahlSatelliten, 15);
    // x: 0 + Rand
    // y: 30 + Rand
    lv_obj_set_pos(anzahlSatelliten, 10, 220);
    lv_label_set_text(anzahlSatelliten, "");
    lv_obj_set_style_text_align(anzahlSatelliten, LV_TEXT_ALIGN_LEFT, 0);

    // 3. Erstelle ein einfaches UI-Element (Button), um den Touch zu testen
    btn = lv_btn_create(lv_scr_act());
    lv_obj_set_pos(btn, 10, 20);
    lv_obj_set_size(btn, 70, 50);

    // Event-Callback an den Button hängen
    lv_obj_add_event_cb(btn, powerOffCb, LV_EVENT_ALL, NULL);

    // Text auf dem Button platzieren
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Off");
    lv_obj_center(label);

}

#endif
