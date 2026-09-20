#include "basic_map_display.hpp"

#include "generic_nvsflash.hpp"

#include "bsp/esp-bsp.h"

static char *TAG = "basic_map_display";

LV_IMAGE_DECLARE(arrow_up_11_20_a);
LV_FONT_DECLARE(my_montserrat_14);

// Map tiles handle
// Internal structure for map tiles instance
struct map_tiles_t {
    // Configuration
    char* base_path;
    char* tile_folders[MAP_TILES_MAX_TYPES];
    int tile_type_count;
    int current_tile_type;
    int grid_cols;
    int grid_rows;
    int tile_count;
    int zoom;
    bool use_spiram;
    bool initialized;

    // Tile management
    int tile_x;
    int tile_y;
    int marker_offset_x;
    int marker_offset_y;
    bool tile_loading_error;

    // Tile data - arrays will be allocated dynamically based on actual grid size
    uint8_t** tile_bufs;
    lv_image_dsc_t* tile_imgs;
};

static map_tiles_handle_t map_handle = NULL;

static double currentLatPosition = 0.;
static double currentLonPosition = 0.;

// LVGL objects for displaying tiles
static lv_obj_t* map_container = NULL;
static lv_obj_t** tile_images = NULL;  // Dynamic array for configurable grid
static lv_obj_t** tile_borders = NULL;  // Dynamic array for configurable grid
static int grid_cols = 0, grid_rows = 0, tile_count = 0;

static lv_obj_t* marker = NULL;

static lv_obj_t * copyright = NULL;
static lv_obj_t * plusButton = NULL;
static lv_obj_t * minusButton = NULL;

static lv_obj_t * tacho = NULL;
static lv_obj_t * hoehe = NULL;
static lv_obj_t * arrow = NULL;
static lv_obj_t * time = NULL;
static lv_obj_t * zoomlevel = NULL;

static lv_style_t style_plusActive;
static lv_style_t style_minusActive;
// Style für ganz große Schrift erstellen
static lv_style_t style_very_large;
// Style für große Schrift erstellen
static lv_style_t style_large;

// additional function for map_tiles
bool map_tiles_is_gps_within_inner_half_of_outer_tiles(map_tiles_handle_t handle, double lat, double lon)
{
    if (!handle || !handle->initialized) {
        return false;
    }

    double x, y;
    map_tiles_gps_to_tile_xy(handle, lat, lon, &x, &y);

    int gps_tile_x = (int)x;
    int gps_tile_y = (int)y;

    // Calculate pixel offset within the tile
    int offset_x = (int)((x - (int)x) * MAP_TILES_TILE_SIZE);
    int offset_y = (int)((y - (int)y) * MAP_TILES_TILE_SIZE);

    bool within_x = (gps_tile_x >= handle->tile_x && gps_tile_x < handle->tile_x + handle->grid_cols);
    if(gps_tile_x == handle->tile_x) {
        if(offset_x <= MAP_TILES_TILE_SIZE / 2) within_x = false;
    }
    else if(gps_tile_x == handle->tile_x + handle->grid_cols - 1) {
        if(offset_x >= MAP_TILES_TILE_SIZE / 2) within_x = false;
    }

    bool within_y = (gps_tile_y >= handle->tile_y && gps_tile_y < handle->tile_y + handle->grid_rows);
        if(gps_tile_y == handle->tile_y) {
        if(offset_y <= MAP_TILES_TILE_SIZE / 2) within_y = false;
    }
    else if(gps_tile_y == handle->tile_y + handle->grid_cols - 1) {
        if(offset_y >= MAP_TILES_TILE_SIZE / 2) within_y = false;
    }

    return within_x && within_y;
}

// Callback-Funktion für den PowerOff Button click
void plusButtonCb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "PlusButton clicked!");

        int new_zoom = map_tiles_get_zoom(map_handle) + 1;
        lv_label_set_text_fmt(zoomlevel, "%2d", new_zoom);
        lv_display_t *disp = lv_display_get_default();
        lv_refr_now(disp);

        {
            // set current value of zoomlevel in nvsFlash
            GenericNvsFlash nvsGnss(std::string("nvsGnss"), std::string("gnss"), NVS_READWRITE);
            esp_err_t ret;
            ret = nvsGnss.SetU8("zoomlevel", new_zoom);
        }
        // 2. Das automatische Vormerken von Änderungen stoppen (Freeze)
        lv_display_enable_invalidation(disp, false);

        ESP_LOGI(TAG, "Zoom in!");
        map_display_set_zoom(new_zoom, currentLatPosition, currentLonPosition);

        // 3. Invalidation wieder erlauben
        lv_display_enable_invalidation(disp, true);
        // 4. Einmalig manuell das Neuzeichnen aller geänderten Bereiche erzwingen
        lv_refr_now(disp);
    }
}

void minusButtonCb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "MinusButton clicked!");

        int new_zoom = map_tiles_get_zoom(map_handle) - 1;
        lv_label_set_text_fmt(zoomlevel, "%2d", new_zoom);
        lv_display_t *disp = lv_display_get_default();
        lv_refr_now(disp);

        {
            // set current value of zoomlevel in nvsFlash
            GenericNvsFlash nvsGnss(std::string("nvsGnss"), std::string("gnss"), NVS_READWRITE);
            esp_err_t ret;
            ret = nvsGnss.SetU8("zoomlevel", new_zoom);
        }
        // 2. Das automatische Vormerken von Änderungen stoppen (Freeze)
        lv_display_enable_invalidation(disp, false);

        ESP_LOGI(TAG, "Zoom out!");
        map_display_set_zoom(new_zoom, currentLatPosition, currentLonPosition);

        // 3. Invalidation wieder erlauben
        lv_display_enable_invalidation(disp, true);
        // 4. Einmalig manuell das Neuzeichnen aller geänderten Bereiche erzwingen
        lv_refr_now(disp);
    }
}

/**
 * @brief Initialize the map display
 */
void map_display_init(lv_obj_t * parent, uint8_t current_zoomlevel)
{
    // Configure map tiles with multiple tile types and custom grid size
    const char* tile_folders[] = {"esp_sd_tiles"};
    map_tiles_config_t config = {
        .base_path = "/sdcard",
        .tile_folders = {tile_folders[0]},
        .tile_type_count = 1,
        .grid_cols = 3,          // 5x5 grid (configurable)
        .grid_rows = 3,
        .default_zoom = current_zoomlevel,
        .use_spiram = true,
        .default_tile_type = 0,  // Start with street map
    };

    bsp_display_lock(-1);

    // Initialize map tiles
    map_handle = map_tiles_init(&config);
    if (!map_handle) {
        ESP_LOGE(TAG, "Failed to initialize map tiles");
        bsp_display_unlock();
        return;
    }
    
    // Get grid dimensions
    map_tiles_get_grid_size(map_handle, &grid_cols, &grid_rows);
    tile_count = map_tiles_get_tile_count(map_handle);
    
    // Allocate tile images array
    tile_images = (lv_obj_t**) malloc(tile_count * sizeof(lv_obj_t*));
    if (!tile_images) {
        ESP_LOGE(TAG, "Failed to allocate tile images array");
        map_tiles_cleanup(map_handle);
        bsp_display_unlock();
        return;
    }
    
    // Create map container
    map_container = lv_obj_create(parent);

    // Fit container exactly to the screen size
    lv_obj_set_size(map_container, LV_PCT(100), LV_PCT(100));
    // Enable scrolling inside the container for its children
    lv_obj_add_flag(map_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_center(map_container);
    lv_obj_set_style_pad_all(map_container, 0, 0);
    lv_obj_set_style_border_width(map_container, 0, 0);
    
    // Create image widgets for each tile
    for (int i = 0; i < tile_count; i++) {
        tile_images[i] = lv_image_create(map_container);
        
        // Position tile in grid
        int row = i / grid_cols;
        int col = i % grid_cols;
        lv_obj_set_pos(tile_images[i], 
                      col * MAP_TILES_TILE_SIZE,
                      row * MAP_TILES_TILE_SIZE);
        lv_obj_set_size(tile_images[i], MAP_TILES_TILE_SIZE, MAP_TILES_TILE_SIZE);
    }

    // Allocate tile borders array
    tile_borders = (lv_obj_t**) malloc((tile_count+1) * sizeof(lv_obj_t*));
    if (!tile_borders) {
        ESP_LOGE(TAG, "Failed to allocate tile borders array");
        map_tiles_cleanup(map_handle);
        bsp_display_unlock();
        return;
    }

    // Create border widgets for each tile
    for (int i = 0; i < tile_count; i++) {
        tile_borders[i] = lv_obj_create(map_container);

        // Position border in grid
        int row = i / grid_cols;
        int col = i % grid_cols;
        lv_obj_set_pos(tile_borders[i],
                      col * MAP_TILES_TILE_SIZE,
                      row * MAP_TILES_TILE_SIZE);
        lv_obj_set_size(tile_borders[i], MAP_TILES_TILE_SIZE, MAP_TILES_TILE_SIZE);
        lv_obj_set_style_bg_opa(tile_borders[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tile_borders[i], 1, 0); // 3-pixel thick border
        lv_obj_set_style_border_color(tile_borders[i], lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_pad_all(tile_borders[i], 0, 0);
    }
    // create border for half of outer tiles
    tile_borders[tile_count] = lv_obj_create(map_container);
    // Position border in grid
     lv_obj_set_pos(tile_borders[tile_count],
                  MAP_TILES_TILE_SIZE / 2,
                  MAP_TILES_TILE_SIZE / 2);
    lv_obj_set_size(tile_borders[tile_count], (grid_cols - 1) * MAP_TILES_TILE_SIZE, (grid_rows - 1) * MAP_TILES_TILE_SIZE);
    lv_obj_set_style_bg_opa(tile_borders[tile_count], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tile_borders[tile_count], 2, 0); // 3-pixel thick border
    lv_obj_set_style_border_color(tile_borders[tile_count], lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_pad_all(tile_borders[tile_count], 0, 0);

    // + button
    if(plusButton == NULL) {
        plusButton = lv_btn_create(parent);
        lv_obj_set_pos(plusButton, 270, 10);
        lv_obj_set_size(plusButton, 40, 40);

        // Event-Callback an den Button hängen
        lv_obj_add_event_cb(plusButton, plusButtonCb, LV_EVENT_ALL, NULL);

        // Text auf dem Button platzieren
        lv_obj_t * label = lv_label_create(plusButton);
        lv_label_set_text(label, "+");
        lv_obj_center(label);

        lv_style_init(&style_plusActive);
        // Grüne Hintergrundfarbe, wenn der Schalter "EIN" ist
        lv_style_set_bg_color(&style_plusActive, lv_palette_main(LV_PALETTE_GREEN));
        // Style explizit für den PRESSED-Zustand zuweisen
        lv_obj_add_style(plusButton, &style_plusActive, LV_STATE_PRESSED);

    }

    // - button
    if(minusButton == NULL) {
        minusButton = lv_btn_create(parent);
        lv_obj_set_pos(minusButton, 270, 150);
        lv_obj_set_size(minusButton, 40, 40);

        // Event-Callback an den Button hängen
        lv_obj_add_event_cb(minusButton, minusButtonCb, LV_EVENT_ALL, NULL);

        // Text auf dem Button platzieren
        lv_obj_t * label = lv_label_create(minusButton);
        lv_label_set_text(label, "-");
        lv_obj_center(label);

        lv_style_init(&style_minusActive);
        // Grüne Hintergrundfarbe, wenn der Schalter "EIN" ist
        lv_style_set_bg_color(&style_minusActive, lv_palette_main(LV_PALETTE_GREEN));
        // Style explizit für den PRESSED-Zustand zuweisen
        lv_obj_add_style(minusButton, &style_minusActive, LV_STATE_PRESSED);
    }

    // update coordinates
    lv_obj_update_layout(lv_screen_active());

    // Copyright notice
    uint32_t width = lv_obj_get_width(map_container);
    uint32_t height = lv_obj_get_height(map_container);
    if(copyright == NULL) {
        copyright = lv_label_create(parent);
        lv_obj_set_width(copyright, width);
        lv_obj_set_height(copyright, 15);
        lv_obj_set_style_text_font(copyright, &my_montserrat_14, 0);
        lv_label_set_text(copyright, "© OpenStreetMap Contributors ");
        lv_obj_set_style_text_align(copyright, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(copyright, 0, height - 15);
    }

    if (!marker) {
        marker = lv_obj_create(map_container);
        lv_obj_set_size(marker, 11, 11);
        lv_obj_set_style_bg_color(marker, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_radius(marker, 5, 0);
        lv_obj_set_style_border_width(marker, 1, 0);
        lv_obj_set_style_border_color(marker, lv_color_hex(0xFFFFFF), 0);

        // set marker object to center
        lv_obj_set_pos(marker, grid_cols * MAP_TILES_TILE_SIZE / 2 - 5, grid_rows * MAP_TILES_TILE_SIZE / 2 - 5);
    }

    if(!arrow) {
        arrow = lv_image_create(map_container);
        lv_image_set_src(arrow, &arrow_up_11_20_a);

        // 3. Define the widget dimension constraints explicitly
        lv_obj_set_size(arrow, 11, 20);

        // 5. Establish the center pivot point for rotation
        // For a 11x20 icon, the exact center point is (5, 19)
        lv_image_set_pivot(arrow, 5, 19);

        // 4. Set the exact coordinates
        // set marker object to center  minus (11/2, 20)
        lv_obj_set_pos(arrow, grid_cols * MAP_TILES_TILE_SIZE / 2 - 5, grid_rows * MAP_TILES_TILE_SIZE / 2 - 19);

        // 6. Set the angle (LVGL v9 uses 0.1-degree precision units)
        // Formula: angle_value = degrees * 10
        // Example: For 45°, pass 450. For 120.5°, pass 1205.
        lv_image_set_rotation(arrow, 0 * 10);
    }

    if(!tacho) {
        // Tacho mit Geschwindigkeit
        // Style für ganz große Schrift erstellen
        lv_style_init(&style_very_large);

        // Eingebaute 48px-Schriftart zuweisen (Standard ist meist 14px)
        lv_style_set_text_font(&style_very_large, &lv_font_montserrat_48);

        tacho = lv_label_create(parent);
        lv_obj_add_style(tacho, &style_very_large, LV_PART_MAIN);
        lv_obj_set_width(tacho, 250);
        lv_obj_set_height(tacho, 50);
        // x: 0 + Rand
        // y: Bildschirmhöhe - Schrifthöhe - Rand
        //lv_obj_set_pos(tacho, 10, 240-48-10); // V1.0.0
        lv_obj_set_pos(tacho, 10, 5); // V1.0.1
        lv_label_set_text_fmt(tacho, "%3d km/h", 0);
        lv_obj_set_style_text_align(tacho, LV_TEXT_ALIGN_RIGHT, 0);
    }

    if(!hoehe) {
        // Meereshöhe
        // Style für große Schrift erstellen
        lv_style_init(&style_large);

        // Eingebaute 24px-Schriftart zuweisen (Standard ist meist 14px)
        lv_style_set_text_font(&style_large, &lv_font_montserrat_24);

        hoehe = lv_label_create(parent);
        lv_obj_add_style(hoehe, &style_large, LV_PART_MAIN);
        lv_obj_set_width(hoehe, 250);
        lv_obj_set_height(hoehe, 30);
        // x: 0 + Rand
        // y: 0 + Rand
        lv_obj_set_pos(hoehe, 10, 165);
        lv_label_set_text_fmt(hoehe, "%3d m asl", 0);
        lv_obj_set_style_text_align(hoehe, LV_TEXT_ALIGN_RIGHT, 0);
    }

    if(!time) {
        // Uhrzeit
        // Style für große Schrift erstellen
        //lv_style_init(&style_large);

        // Eingebaute 24px-Schriftart zuweisen (Standard ist meist 14px)
        //lv_style_set_text_font(&style_large, &lv_font_montserrat_24);

        time = lv_label_create(parent);
        lv_obj_add_style(time, &style_large, LV_PART_MAIN);
        lv_obj_set_width(time, 300);
        lv_obj_set_height(time, 30);
        // x: 0 + Rand
        // y: 0 + Rand
        lv_obj_set_pos(time, 10, 165);
        lv_label_set_text_fmt(time, "%s", "11:55:00");
        lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_LEFT, 0);
    }

    if(!zoomlevel) {
        // zoom level
        // Style für große Schrift erstellen
        //lv_style_init(&style_large);

        // Eingebaute 24px-Schriftart zuweisen (Standard ist meist 14px)
        //lv_style_set_text_font(&style_large, &lv_font_montserrat_24);

        zoomlevel = lv_label_create(parent);
        lv_obj_add_style(zoomlevel, &style_large, LV_PART_MAIN);
        lv_obj_set_width(zoomlevel, 30);
        lv_obj_set_height(zoomlevel, 30);
        lv_obj_set_pos(zoomlevel, 10, 10);
        lv_label_set_text_fmt(zoomlevel, "%2d", 16);
        lv_obj_set_style_text_align(time, LV_TEXT_ALIGN_LEFT, 0);
    }

    bsp_display_unlock();

    ESP_LOGI(TAG, "Map display initialized");
}

void map_display_set_current_values(int angle, int speed, int altitude, double lat, double lon, const char* xtime) {
    bsp_display_lock(-1);
    lv_label_set_text_fmt(tacho, "%3d km/h", speed);
    lv_label_set_text_fmt(hoehe, "%3d m asl", altitude);
    if (speed > 2) {
        lv_image_set_rotation(arrow, angle * 10);
    }
    lv_label_set_text_fmt(time, "%s", xtime);
    lv_label_set_text_fmt(zoomlevel, "%2d", map_tiles_get_zoom(map_handle));

    lv_display_t *disp = lv_display_get_default();
    if(lat != 0 && lon != 0) {
        lv_refr_now(disp);
        // 2. Das automatische Vormerken von Änderungen stoppen (Freeze)
        lv_display_enable_invalidation(disp, false);
        bsp_display_unlock();
        map_display_set_center_from_gps(lat, lon);
        bsp_display_lock(-1);
        // 3. Invalidation wieder erlauben
        lv_display_enable_invalidation(disp, true);
    }
    // 4. Einmalig manuell das Neuzeichnen aller geänderten Bereiche erzwingen
    lv_refr_now(disp);
    bsp_display_unlock();
}

/**
 * @brief Load and display map tiles for a GPS location
 * 
 * @param lat Latitude in degrees
 * @param lon Longitude in degrees
 */
void map_display_load_location(double lat, double lon)
{
    if (!map_handle) {
        ESP_LOGE(TAG, "Map not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Loading map for GPS: %.6f, %.6f", lat, lon);
    
    // Set center from GPS coordinates
    map_tiles_set_center_from_gps(map_handle, lat, lon);
    
    // Get current tile position
    int base_tile_x, base_tile_y;
    map_tiles_get_position(map_handle, &base_tile_x, &base_tile_y);

    // Load tiles in a configurable grid
    for (int row = 0; row < grid_rows; row++) {
        for (int col = 0; col < grid_cols; col++) {
            int index = row * grid_cols + col;
            int tile_x = base_tile_x + col;
            int tile_y = base_tile_y + row;
            
            // Load the tile
            bool loaded = map_tiles_load_tile(map_handle, index, tile_x, tile_y);

            bsp_display_lock(-1);

            if (loaded) {
                // Update the image widget
                lv_image_dsc_t* img_dsc = map_tiles_get_image(map_handle, index);
                if (img_dsc) {
                    lv_image_set_src(tile_images[index], img_dsc);
                    ESP_LOGI(TAG, "Loaded tile %d (%d, %d)", index, tile_x, tile_y);
                }
            } else {
                ESP_LOGW(TAG, "Failed to load tile %d (%d, %d)", index, tile_x, tile_y);
                // Set a placeholder or clear the image
                lv_image_set_src(tile_images[index], NULL);
            }

            bsp_display_unlock();
        }
    }
    // update coordinates
    lv_obj_update_layout(lv_screen_active());

    ESP_LOGI(TAG, "Map tiles loaded for location");
}

/**
 * @brief Set the map tile type and reload tiles
 * 
 * @param tile_type Tile type index (0=street, 1=satellite, 2=terrain, 3=hybrid)
 * @param lat Current latitude
 * @param lon Current longitude
 */
void map_display_set_tile_type(int tile_type, double lat, double lon)
{
    bsp_display_lock(-1);

    if (!map_handle) {
        ESP_LOGE(TAG, "Map not initialized");
        bsp_display_unlock();
        return;
    }
    
    // Validate tile type
    int max_types = map_tiles_get_tile_type_count(map_handle);
    if (tile_type < 0 || tile_type >= max_types) {
        ESP_LOGW(TAG, "Invalid tile type %d (valid range: 0-%d)", tile_type, max_types - 1);
        bsp_display_unlock();
        return;
    }
    
    ESP_LOGI(TAG, "Setting tile type to %d (%s)", tile_type, 
             map_tiles_get_tile_type_folder(map_handle, tile_type));
    
    // Set tile type
    if (map_tiles_set_tile_type(map_handle, tile_type)) {
        bsp_display_unlock();
        // Reload tiles for the new type
        map_display_load_location(lat, lon);
        bsp_display_lock(-1);
    }
    bsp_display_unlock();
 }

/**
 * @brief Set the zoom level and reload tiles
 * 
 * @param zoom New zoom level
 * @param lat Current latitude
 * @param lon Current longitude
 */
void map_display_set_zoom(int zoom, double lat, double lon)
{
    if (!map_handle) {
        ESP_LOGE(TAG, "Map not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "Setting zoom to %d", zoom);
    // Update zoom level
    bsp_display_lock(-1);
    map_tiles_set_zoom(map_handle, zoom);
    bsp_display_unlock();

    // Show tiles for the new zoom level and position
    map_display_set_center_from_gps(lat, lon);
}

/**
 * @brief Add a GPS marker to the map
 * 
 * @param lat Latitude in degrees
 * @param lon Longitude in degrees
 */
void map_display_set_center_from_gps(double lat, double lon)
{
    if (!map_handle) {
        ESP_LOGE(TAG, "Map not initialized");
        return;
    }

    currentLatPosition = lat;
    currentLonPosition = lon;

    bsp_display_lock(-1);

    // update coordinates
    lv_obj_update_layout(lv_screen_active());

    // Check if GPS position is within inner half of outer tiles
    if (!map_tiles_is_gps_within_inner_half_of_outer_tiles(map_handle, lat, lon)) {
        bsp_display_unlock();
        ESP_LOGW(TAG, "GPS position outside inner half of outer tiles, reloading map");
        // if not, load tiles for location
        map_display_load_location(lat, lon);
        bsp_display_lock(-1);
    }

    // update coordinates
    lv_obj_update_layout(lv_screen_active());

    // Convert GPS to tile coordinates
    double tile_x, tile_y;
    map_tiles_gps_to_tile_xy(map_handle, lat, lon, &tile_x, &tile_y);
    
    // Get current grid position (top-left tile)
    int base_tile_x, base_tile_y;
    map_tiles_get_position(map_handle, &base_tile_x, &base_tile_y);
    
    // Calculate absolute pixel position of marker
    int abs_px = (int)(tile_x * MAP_TILES_TILE_SIZE);
    int abs_py = (int)(tile_y * MAP_TILES_TILE_SIZE);
    
    // Calculate top-left pixel position of current tile grid
    int top_left_px_x = base_tile_x * MAP_TILES_TILE_SIZE;
    int top_left_px_y = base_tile_y * MAP_TILES_TILE_SIZE;
    
    // Get scroll position if map is scrollable
    lv_coord_t scroll_x = lv_obj_get_scroll_x(map_container);
    lv_coord_t scroll_y = lv_obj_get_scroll_y(map_container);
    
    // Calculate marker position relative to current view
    int marker_x = abs_px - top_left_px_x /*- scroll_x*/ - 5;  // -5 to center the 11px marker
    int marker_y = abs_py - top_left_px_y /*- scroll_y*/ - 5;

    // Calculate arrow position relative to current view
    int arrow_x = abs_px - top_left_px_x /*- scroll_x*/ - 5;
    int arrow_y = abs_py - top_left_px_y /*- scroll_y*/ - 19;

    ESP_LOGD(TAG, "Marker calculation: tile_xy=(%.3f,%.3f) base=(%d,%d) abs_px=(%d,%d) scroll=(%d,%d) pixel=(%d,%d)",
             tile_x, tile_y, base_tile_x, base_tile_y, abs_px, abs_py, scroll_x, scroll_y, marker_x, marker_y);
    
    // Check if marker is within visible bounds
    int container_width = grid_cols * MAP_TILES_TILE_SIZE;
    int container_height = grid_rows * MAP_TILES_TILE_SIZE;
    if (marker_x < -10 || marker_x > container_width || marker_y < -10 || marker_y > container_height) {
        ESP_LOGW(TAG, "Marker at (%d, %d) is outside visible bounds (0,0) to (%d,%d)",
                 marker_x, marker_y, container_width, container_height);
    }
    
    ESP_LOGD(TAG, "GPS marker at (%.6f, %.6f) positioned at pixel (%d, %d)",
             lat, lon, marker_x, marker_y);

    // update coordinates
    lv_obj_update_layout(lv_screen_active());

    uint32_t width = lv_obj_get_width(map_container);
    uint32_t height = lv_obj_get_height(map_container);

    // first the window is scrolled to the new position, then the marker and arrow positions are updated
    // go get a smooth movement impression

    // Scroll the window panel itself to the center the marker position
    // Use LV_ANIM_ON if you want a smooth sliding transition on load
    lv_obj_scroll_to(map_container, marker_x - width/2, marker_y - height/2, LV_ANIM_ON);

    // update marker object
    lv_obj_set_pos(marker, marker_x, marker_y);

    // update arrow object
    lv_obj_set_pos(arrow, arrow_x, arrow_y);

    // update coordinates
    lv_obj_update_layout(lv_screen_active());

    bsp_display_unlock();
 }

/**
 * @brief Clean up map display resources
 */
void map_display_cleanup(void)
{
    bsp_display_lock(-1);

    if (tile_images) {
        free(tile_images);
        tile_images = NULL;
    }
    
    if (map_handle) {
        map_tiles_cleanup(map_handle);
        map_handle = NULL;
    }
    
    if (map_container) {
        lv_obj_delete(map_container);
        map_container = NULL;
    }
    
    grid_cols = grid_rows = tile_count = 0;

    bsp_display_unlock();

    ESP_LOGI(TAG, "Map display cleaned up");
}

