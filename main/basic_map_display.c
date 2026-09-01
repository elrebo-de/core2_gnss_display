#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "basic_map_display.h"

#include "bsp/esp-bsp.h"

static char *TAG = "basic_map_display";

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

// LVGL objects for displaying tiles
static lv_obj_t* map_container = NULL;
static lv_obj_t** tile_images = NULL;  // Dynamic array for configurable grid
static lv_obj_t** tile_borders = NULL;  // Dynamic array for configurable grid
static int grid_cols = 0, grid_rows = 0, tile_count = 0;
static lv_obj_t * copyright = NULL;

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

/**
 * @brief Initialize the map display
 */
void map_display_init(lv_obj_t * parent)
{
    // Configure map tiles with multiple tile types and custom grid size
    const char* tile_folders[] = {"esp_sd_tiles"};
    map_tiles_config_t config = {
        .base_path = "/sdcard",
        .tile_folders = {tile_folders[0]},
        .tile_type_count = 1,
        .grid_cols = 3,          // 5x5 grid (configurable)
        .grid_rows = 3,
        .default_zoom = 16,
        .use_spiram = true,
        .default_tile_type = 0,  // Start with street map
    };

    bsp_display_lock(0);

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
    tile_images = malloc(tile_count * sizeof(lv_obj_t*));
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
    tile_borders = malloc((tile_count+1) * sizeof(lv_obj_t*));
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


    bsp_display_unlock();

    ESP_LOGI(TAG, "Map display initialized");
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
    
    bsp_display_lock(0);

    ESP_LOGI(TAG, "Loading map for GPS: %.6f, %.6f", lat, lon);
    
    // Set center from GPS coordinates
    map_tiles_set_center_from_gps(map_handle, lat, lon);
    
    // Get current tile position
    int base_tile_x, base_tile_y;
    map_tiles_get_position(map_handle, &base_tile_x, &base_tile_y);

    // pause all LVGL timers globally
    lv_timer_enable(false);

    ///////    // pause display refresh timer
    ///////    // Get the current active display
    ///////    lv_display_t * disp = lv_display_get_default();
    ///////    // GET the refresh timer pointer using the official API function
    ///////    lv_timer_t * refr_timer = lv_display_get_refr_timer(disp);
    ///////    // Pause the display's internal refreshing timer safely
    ///////    lv_timer_pause(refr_timer);

    bsp_display_unlock();

    // Load tiles in a configurable grid
    for (int row = 0; row < grid_rows; row++) {
        for (int col = 0; col < grid_cols; col++) {
            int index = row * grid_cols + col;
            int tile_x = base_tile_x + col;
            int tile_y = base_tile_y + row;
            
            // Load the tile
            bool loaded = map_tiles_load_tile(map_handle, index, tile_x, tile_y);
            if (loaded) {
                bsp_display_lock(0);
                // Update the image widget
                lv_image_dsc_t* img_dsc = map_tiles_get_image(map_handle, index);
                if (img_dsc) {
                    lv_image_set_src(tile_images[index], img_dsc);
                    ESP_LOGI(TAG, "Loaded tile %d (%d, %d)", index, tile_x, tile_y);
                }
                bsp_display_unlock();
            } else {
                ESP_LOGW(TAG, "Failed to load tile %d (%d, %d)", index, tile_x, tile_y);
                // Set a placeholder or clear the image
                bsp_display_lock(0);
                lv_image_set_src(tile_images[index], NULL);
                bsp_display_unlock();
            }
        }
    }
    
    bsp_display_lock(0);

    // resume all LVGL timers globally
    lv_timer_enable(true);

    ///////      // resume display refresh timer
    ///////      // Force coordinates to calculate before making it visible
    ///////      lv_obj_update_layout(lv_screen_active());
    ///////      // Resume refreshing and trigger an immediate redraw
    ///////      lv_timer_resume(refr_timer);
    ///////      lv_obj_invalidate(lv_screen_active());

    bsp_display_unlock();

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
    bsp_display_lock(0);

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
        bsp_display_lock(0);
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
    bsp_display_lock(0);
    if (!map_handle) {
        ESP_LOGE(TAG, "Map not initialized");
        bsp_display_unlock();
        return;
    }
    
    ESP_LOGI(TAG, "Setting zoom to %d", zoom);
    
    // Update zoom level
    map_tiles_set_zoom(map_handle, zoom);
    
    bsp_display_unlock();
     // Reload tiles for the new zoom level
    map_display_load_location(lat, lon);
}

/**
 * @brief Add a GPS marker to the map
 * 
 * @param lat Latitude in degrees
 * @param lon Longitude in degrees
 */
void map_display_add_marker(double lat, double lon)
{
    bsp_display_lock(0);
    if (!map_handle) {
        ESP_LOGE(TAG, "Map not initialized");
        bsp_display_unlock();
        return;
    }
    
    // Check if GPS position is within inner half of outer tiles
    if (!map_tiles_is_gps_within_inner_half_of_outer_tiles(map_handle, lat, lon)) {
        bsp_display_unlock();
        ESP_LOGW(TAG, "GPS position outside inner half of outer tiles, reloading map");
        // if not, load tiles for location
        map_display_load_location(lat, lon);
        bsp_display_lock(0);
        // update coordinates
        //lv_obj_update_layout(lv_screen_active());
    }

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
    
    // update coordinates
    //lv_obj_update_layout(lv_screen_active());

    // Calculate marker position relative to current view
    int marker_x = abs_px - top_left_px_x /*- scroll_x*/ - 5;  // -5 to center the 10px marker
    int marker_y = abs_py - top_left_px_y /*- scroll_y*/ - 5;
    
    ESP_LOGD(TAG, "Marker calculation: tile_xy=(%.3f,%.3f) base=(%d,%d) abs_px=(%d,%d) scroll=(%d,%d) pixel=(%d,%d)",
             tile_x, tile_y, base_tile_x, base_tile_y, abs_px, abs_py, scroll_x, scroll_y, marker_x, marker_y);
    
    // Check if marker is within visible bounds
    int container_width = grid_cols * MAP_TILES_TILE_SIZE;
    int container_height = grid_rows * MAP_TILES_TILE_SIZE;
    if (marker_x < -10 || marker_x > container_width || marker_y < -10 || marker_y > container_height) {
        ESP_LOGW(TAG, "Marker at (%d, %d) is outside visible bounds (0,0) to (%d,%d)",
                 marker_x, marker_y, container_width, container_height);
    }
    
    // Create or update marker object
    static lv_obj_t* marker = NULL;
    if (!marker) {
        marker = lv_obj_create(map_container);
        lv_obj_set_size(marker, 10, 10);
        lv_obj_set_style_bg_color(marker, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_radius(marker, 5, 0);
        lv_obj_set_style_border_width(marker, 1, 0);
        lv_obj_set_style_border_color(marker, lv_color_hex(0xFFFFFF), 0);
    }
    
    lv_obj_set_pos(marker, marker_x, marker_y);

    ESP_LOGD(TAG, "GPS marker at (%.6f, %.6f) positioned at pixel (%d, %d)",
             lat, lon, marker_x, marker_y);

    // update coordinates
    //lv_obj_update_layout(lv_screen_active());
    uint32_t width = lv_obj_get_width(map_container);
    uint32_t height = lv_obj_get_height(map_container);

    // Scroll the window panel itself to the center the marker position
    // Use LV_ANIM_ON if you want a smooth sliding transition on load
    lv_obj_scroll_to(map_container, marker_x - width/2, marker_y - height/2, LV_ANIM_ON);

    LV_FONT_DECLARE(my_montserrat_14);
    // Copyright notice
    if(copyright == NULL) {
        copyright = lv_label_create(lv_obj_get_parent(map_container));
        lv_obj_set_width(copyright, width);
        lv_obj_set_height(copyright, 15);
        lv_obj_set_style_text_font(copyright, &my_montserrat_14, 0);
        lv_label_set_text(copyright, "© OpenStreetMap Contributors ");
        lv_obj_set_style_text_align(copyright, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(copyright, 0, height - 15);
    }
    bsp_display_unlock();
 }

/**
 * @brief Clean up map display resources
 */
void map_display_cleanup(void)
{
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
    
    ESP_LOGI(TAG, "Map display cleaned up");
}

