#include <string>
#include <vector>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

#include "lv_gnss_cockpit.h" // cockpit components
#include "lv_gnss_settings.h" // settings components
#include "basic_map_display.h" // map components

// Global Mutex for Timezone switch
static SemaphoreHandle_t tz_mutex = NULL;

#include "generic_button.hpp"
#include "generic_uart.hpp"
GenericUart *gnssUart; // pointer to GenericUart class

#include "i2c_master.hpp"
I2cMaster* i2c = NULL;

#define LOG_MEM_INFO (0)

std::string tag("CORE2 GNSS Display");

bool sd_card = false; // flag to indicate a mounted SD card

time_t my_timegm_safe(struct tm *tm) {
    // initialize Mutex lazily at first call
    if (tz_mutex == NULL) {
        static StaticSemaphore_t mutex_buffer;
        tz_mutex = xSemaphoreCreateMutexStatic(&mutex_buffer);
    }

    time_t ret = -1;

    // lock Mutex (warts forever until Mutex is free
    if (xSemaphoreTake(tz_mutex, portMAX_DELAY) == pdTRUE) {
        char *tz = getenv("TZ");

        // switch to UTC temporarily
        unsetenv("TZ");
        tzset();

        // UTC has no DST
        tm->tm_isdst = 0;
        ret = mktime(tm);

        // reset original timezone
        if (tz) {
            setenv("TZ", tz, 1);
        } else {
            unsetenv("TZ");
        }
        tzset();

        // unlock Mutex
        xSemaphoreGive(tz_mutex);
    }
    return ret;
}

std::string convert_gnss_date_and_time_to_local(std::string gnssDate, std::string gnssTime)
{
    if(gnssDate.length() < 8 || gnssTime.length() < 8) {
        return std::string("");
    }

    // get UTC time from GNSS
    struct tm utc_tm = {0};
    utc_tm.tm_year = 2000 - 1900 + std::stoi(gnssDate.substr(6,2)); // years since 1900
    utc_tm.tm_mon  = std::stoi(gnssDate.substr(3,2)) - 1;           // Month (0 = Januar, 7 = August)
    utc_tm.tm_mday = std::stoi(gnssDate.substr(0,2));
    utc_tm.tm_hour = std::stoi(gnssTime.substr(0,2));
    utc_tm.tm_min  = std::stoi(gnssTime.substr(3,2));
    utc_tm.tm_sec  = std::stoi(gnssTime.substr(6,2));

    char utc_buf[64];
    strftime(utc_buf, sizeof(utc_buf), "%d.%m.%Y %H:%M:%S %Z", &utc_tm);

    // 3. transform UTC structure to a Unix timestamp
    // my_timegm_safe() uses UTC
    time_t utc_timestamp = my_timegm_safe(&utc_tm);

    // define Unix timestamp
    struct timeval tv;
    tv.tv_sec = utc_timestamp;  // Seconds since 1.1.1970
    tv.tv_usec = 0;             // Microseconds

    // 2. set system time directly
    settimeofday(&tv, NULL);

    // translate Timestamp to local time
    time_t now;
    struct tm local_tm;
    if(NULL == localtime_r(&utc_timestamp, &local_tm)) {
        return std::string("");
    };

    // format result
    char buf[64];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S %Z", &local_tm);

    return std::string(buf);
}

// Callback function for PPS signal from LC76G GNSS Module
extern "C" void ppsSignalCb(void *arg, void *data)
{
    ESP_LOGD(tag.c_str(), "Callback for PPS signal called!");

    int i;

    int angle = 0;
    int speed = 0;
    int altitude = 0;
    std::string latitudeDeg;
    std::string longitudeDeg;
    std::string latitudeDegMin;
    std::string longitudeDegMin;
    std::string latitudeDegMinSec;
    std::string longitudeDegMinSec;
    std::string xdate;
    std::string xtime;
    int nrOfSats = 0;

    // reads data (max. 2048 - 1 bytes) from UART with a timeout 0f 100 ms
    // int len = uart_read_bytes(*uart_num, data, 2048 - 1, 100 / portTICK_PERIOD_MS);
    //int len = uart_read_bytes(gnssUart->getUartNum(), data, 2048 - 1, 100 / portTICK_PERIOD_MS);

    // every second the LC76G sends data (less than 2k)
    std::string allMessages = gnssUart->readString(100);

    if (allMessages.length() > 0) {
        // parse message $GNVTG
        std::size_t position = allMessages.find("$GNVTG");
        if(position != std::string::npos) {
            std::stringstream gnvtg(allMessages.substr(position));

            std::string segment;
            std::vector<std::string> seglist;

            i = 0;
            while(std::getline(gnvtg, segment, ','))
            {
               i++;
               seglist.push_back(segment);

               if(i>7) break;
            }

            std::string trueTrackAngle(seglist[1]);
            std::string magneticTrackAngle(seglist[3]);
            std::string speedOverGroundKnots(seglist[5]);
            std::string speedOverGroundKilometers(seglist[7]);

            // print data
            ESP_LOGD(tag.c_str(),
                     "$GNVTG\n TrueTrackAngle: %s,\n MagneticTrackAngle: %s,\n SpeedOverGround(Knots): %s,\n SpeedOverGround(km/h): %s",
                     trueTrackAngle.c_str(),
                     magneticTrackAngle.c_str(),
                     speedOverGroundKnots.c_str(),
                     speedOverGroundKilometers.c_str());

            // verwende trueTrackAngle im scale widget (Compass)
            if(trueTrackAngle.length() > 0) {
                angle = std::stoi(trueTrackAngle);

            }
            // verwende speedOverGroundKilometers im label widget (tacho)
            if(speedOverGroundKilometers.length() > 0) {
                speed = std::stoi(speedOverGroundKilometers);
            }
        }

        // parse message $GNRMC
        position = allMessages.find("$GNRMC");
        if(position != std::string::npos) {
            std::stringstream gnrmc(allMessages.substr(position));

            std::string segment;
            std::vector<std::string> seglist;

            i = 0;
            while(std::getline(gnrmc, segment, ','))
            {
               i++;
               seglist.push_back(segment);

               if(i>11) break;
            }

            std::string time(seglist[1]);
            std::string validity(seglist[2]);
            std::string latitude(seglist[3].append(",").append(seglist[4]));
            std::string longitude(seglist[5].append(",").append(seglist[6]));
            std::string speedOverGroundKnots(seglist[7]);
            std::string trueTrackAngle(seglist[8]);
            std::string date(seglist[9]);
            std::string variation(seglist[10].append(",").append(seglist[11]));

            // print data
            ESP_LOGD(tag.c_str(),
                     "$GNRMC\n Time: %s,\n Validity: %s,\n Latitude: %s,\n Longitude: %s,\n SpeedOverGround(Knots): %s,\n TrueTrackAngle: %s,\n Date: %s,\n Variation: %s",
                     time.c_str(),
                     validity.c_str(),
                     latitude.c_str(),
                     longitude.c_str(),
                     speedOverGroundKnots.c_str(),
                     trueTrackAngle.c_str(),
                     date.c_str(),
                     variation.c_str());
            // verwende latitude im label widget breite (latitude)
            // format is: ddmm.ffffff,{N|S}
            if(latitude.length() >= 13) {
                latitudeDegMin = latitude.substr(0,2).append("°").append(latitude.substr(2,9)).append("'").append(latitude.substr(12,1));
                float floatDeg = std::stof(latitude.substr(0,2)) + std::stof(latitude.substr(2,9)) / 60;
                char deg[20];
                sprintf(deg, "%011.8f", floatDeg);
                if(0 == latitude.substr(12,1).compare("S")) strcpy(deg, std::string("-").append(deg).c_str());
                latitudeDeg = std::string(deg);
                float sec = std::stof(latitude.substr(4,7)) * 60;
                char degMinSec[20];
                sprintf(degMinSec, "%s°%s'%7.4f\"%s", latitude.substr(0,2).c_str(), latitude.substr(2,2).c_str(), sec, latitude.substr(12,1).c_str());
                latitudeDegMinSec = std::string(degMinSec);
                ESP_LOGD(tag.c_str(), "Deg: %s, Min: %s, Sec: %7.4f, LatitudeDegMinSec: %s, LatitudeDegMin: %s, LatitudeDeg: %s", latitude.substr(0,2).c_str(), latitude.substr(2,2).c_str(), sec, latitudeDegMinSec.c_str(), latitudeDegMin.c_str(), latitudeDeg.c_str());
            }
            // verwende longitude im label widget laenge (longitude)
            // format is: dddmm.ffffff,{W|E}
            if(longitude.length() >= 14) {
                longitudeDegMin = longitude.substr(0,3).append("°").append(longitude.substr(3,9)).append("'").append(longitude.substr(13,1));
                float floatDeg = std::stof(longitude.substr(0,3)) + std::stof(longitude.substr(3,9)) / 60;
                char deg[20];
                sprintf(deg, "%012.8f", floatDeg);
                if(0 == longitude.substr(13,1).compare("W")) strcpy(deg, std::string("-").append(deg).c_str());
                longitudeDeg = std::string(deg);
                float sec = std::stof(longitude.substr(5,7)) * 60;
                char degMinSec[20];
                sprintf(degMinSec,"%s°%s'%7.4f\"%s", longitude.substr(0,3).c_str(), longitude.substr(3,2).c_str(), sec, longitude.substr(13,1).c_str());
                longitudeDegMinSec = std::string(degMinSec);
                ESP_LOGD(tag.c_str(), "Deg: %s, Min: %s, Sec: %7.4f, LongitudeDegMinSec: %s, LongitudeDegMin: %s, LongitudeDeg: %s", longitude.substr(0,3).c_str(), longitude.substr(3,2).c_str(), sec, longitudeDegMinSec.c_str(), longitudeDegMin.c_str(), longitudeDeg.c_str());
            }
            // verwende time im label widget uhrzeit (time)
            if(time.length() >= 6) {
                xtime = time.substr(0,2).append(":").append(time.substr(2,2)).append(":").append(time.substr(4,2));
            }
            // verwende date im label widget datum (date)
            if(time.length() >= 6) {
                xdate = date.substr(0,2).append(".").append(date.substr(2,2)).append(".").append(date.substr(4,2));
            }
        }

        // parse message $GNGGA
        position = allMessages.find("$GNGGA");
        if(position != std::string::npos) {
            std::stringstream gngga(allMessages.substr(position));

            std::string segment;
            std::vector<std::string> seglist;

            i = 0;
            while(std::getline(gngga, segment, ','))
            {
               i++;
               seglist.push_back(segment);

               if(i>11) break;
            }

            std::string time(seglist[1]);
            std::string latitude(seglist[2].append(",").append(seglist[3]));
            std::string longitude(seglist[4].append(",").append(seglist[5]));
            std::string quality(seglist[6]);
            std::string numberOfSatellites(seglist[7]);
            std::string hdop(seglist[8]);
            std::string orthometricHeight(seglist[9]);
            std::string geoidSeparation(seglist[11]);

            // print data
            ESP_LOGD(tag.c_str(),
                     "$GNGGA\n Time: %s,\n Latitude: %s,\n Longitude: %s,\n Quality: %s,\n NrOfSatellites: %s,\n HDOP: %s,\n OrthometricHeight: %s,\n GeoidSeparation: %s",
                     time.c_str(),
                     latitude.c_str(),
                     longitude.c_str(),
                     quality.c_str(),
                     numberOfSatellites.c_str(),
                     hdop.c_str(),
                     orthometricHeight.c_str(),
                     geoidSeparation.c_str());

            // verwende orthometricHeight im label widget hoehe (altitude)
            if(orthometricHeight.length() > 0) {
                altitude = std::stoi(orthometricHeight);
            }
            // verwende nrOfSatellites im label widget anzSat (nrOfSats)
            if(numberOfSatellites.length() > 0) {
                nrOfSats = std::stoi(numberOfSatellites);
            }
        }
        ESP_LOGD(tag.c_str(), "speed: %d, angle: %d, alt: %d, latDegMinSec: %s, latDeg: %s, lonDegMinSec: %s, lonDeg: %s, date: %s, time: %s, nrOfSats: %d", speed, angle, altitude, latitudeDegMinSec.c_str(), latitudeDeg.c_str(), longitudeDegMinSec.c_str(), longitudeDeg.c_str(), xdate.c_str(), xtime.c_str(), nrOfSats);

        // set current timestamp
        if(xdate.length() == 8 && xtime.length() == 8) {
            std::string localTimestamp = convert_gnss_date_and_time_to_local(xdate, xtime);

            ESP_LOGD(tag.c_str(), "Local time: %s", localTimestamp.c_str());

            if(localTimestamp.length() >= 19) {
                xdate = localTimestamp.substr(0,6).append(localTimestamp.substr(8,2));
                xtime = localTimestamp.substr(11);
            }
        }
        // set current cockpit values
        lv_gnss_cockpit_set_current_values(angle, speed, altitude, latitudeDegMinSec.c_str(), latitudeDeg.c_str(), longitudeDegMinSec.c_str(), longitudeDeg.c_str(), xdate.c_str(), xtime.c_str(), nrOfSats);

        // set marker position in map
        if(sd_card) {
            if(latitudeDeg.length() > 7 && longitudeDeg.length() > 7) {
                map_display_add_marker(std::stof(latitudeDeg), std::stof(longitudeDeg));
            }
        }
    }
}

// Callback-Funktion für den PowerOff Button click
extern "C" void powerOffCb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        ESP_LOGI(tag.c_str(), "PowerOff button clicked!");

        ESP_LOGI(tag.c_str(), "Power Off!");
        // wait a moment ...
        vTaskDelay(500 / portTICK_PERIOD_MS); // delay 0.5 seconds

        // AXP2101: VBUS trennen und PowerOff
        I2cDevice *device = i2c->GetDevice("AXP2101");
        // disconnectVbus
        uint8_t disconnectVbus[2] = {0x18, 0x0f};
        ESP_ERROR_CHECK(device->Write(disconnectVbus, 2));
        vTaskDelay(100 / portTICK_PERIOD_MS); // delay 0.1 seconds
        // powerOff
        uint8_t powerOff[2] = {0x10, 0x31};
        ESP_ERROR_CHECK(device->Write(powerOff, 2));
    }
}

// SD card
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

// Configuration parameters for the SD Card Virtual File System
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,      // Do not erase map data if mount fails
    .max_files = 5,                       // Must allow at least 2-3 open files for map streaming
    .allocation_unit_size = 16 * 1024     // Large allocation units optimize binary streaming
};

extern "C" void app_main(void)
{
    vTaskDelay(500 / portTICK_PERIOD_MS); // delay 0.5 seconds

    ESP_LOGI(tag.c_str(), "Configure local timezone");
    // set timezone für Germany (CET/CEST incl. rules for switching)
    // "CET-1CEST,M3.5.0,M10.5.0" bedeutet:
    // - Normal time is CET (UTC+1)
    // - Daylight saving time is CEST (UTC+2)
    // - switch to CEST: March (3), last sunday (.5.0)
    // - switch to CET: October (10), last sunday (.5.0)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0", 1);
    tzset();

    ESP_LOGI(tag.c_str(), "Configure GenericUart gnssUart");

    // set UART configuration
    const uart_port_t uart_num = UART_NUM_2;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {
           .allow_pd = 0,
           .backup_before_sleep = 0,
        },
    };
    // we do not use the TX pin, because we are only reading from RX

    // M5STACK CORE2 V1.1
    const int uart_tx_pin = UART_PIN_NO_CHANGE; // pin 32 used for LC76G PPS signal
    const int uart_rx_pin = 33;

    gnssUart = new GenericUart(
                 "gnssUart",
                 uart_num,
                 &uart_config,
                 uart_tx_pin,
                 uart_rx_pin);

    ESP_LOGI(tag.c_str(), "Configure SD card");
    // Initialise BSP (incl. I2C/PMU)
    ESP_ERROR_CHECK(bsp_i2c_init());

    vTaskDelay(150 / portTICK_PERIOD_MS); // delay 150 ms before using i2c

    // integrate SD card via SPI
    bsp_sdcard_cfg_t sd_cfg = {
        .mount = &mount_config,
    };

    if (bsp_sdcard_sdspi_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGE(tag.c_str(), "Error mounting SD card -> Map display disabled!");
        sd_card = false;
    }
    else {
        ESP_LOGI(tag.c_str(), "SD card successfully mounted at %s!", BSP_SD_MOUNT_POINT);
        sd_card = true;
    }


    ESP_LOGI(tag.c_str(), "Configure AXP2101 PMU");

    // 1. Initialize the PMU hardware configuration
    // This auto-configures the AXP2101 ADC for battery current and voltage monitoring
    #if CONFIG_BSP_PMU_AXP2101
    ESP_LOGI(tag.c_str(), "AXP2101 PMU Detected (M5Stack Core2 v1.1)");
    #endif

    /* Configure the I2C Master Bus */
    ESP_LOGI(tag.c_str(), "I2cMaster");
    // set i2c_master_bus_handle from already initialized i2c_master_bus
    i2c = new I2cMaster(std::string("I2C Master Bus"), bsp_i2c_get_handle());

    // Add the AXP2101 PMU device
    ESP_LOGI(tag.c_str(), "I2cDevice AXP2101 PMU");
    i2c->AddDevice(new I2cDevice(
        std::string("AXP2101 PMU"), // tag
        std::string("AXP2101"), // deviceName
        (i2c_addr_bit_len_t) I2C_ADDR_BIT_LEN_7, // devAddrLength
        (uint16_t) 0x34, // deviceAddress
        (uint32_t) 400000 // sclSpeedHz
        )
    );

    ESP_LOGI(tag.c_str(), "AXP2101 I2C interface successfully established via esp_bsp!");

    ESP_LOGI(tag.c_str(), "Configure Display");

    // PSRAM Kapazität abfragen
    size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    if (psram_size > 0) {
        printf("PSRAM successfully activated! Free storage: %d Bytes\n", psram_size);
    } else {
        printf("Error: PSRAM could not be initialised.\n");
    }


    ESP_LOGI(tag.c_str(), "Configure GenericButton ppsSignal");

    // set GPIO PPS signal "Button" configuration

    /* M5STACK CORE2 V1.1 */
    button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = 32,
        .active_level = 1,
        .enable_power_save = false,
        .disable_pull = true,
    };

    GenericButton ppsSignal(
	   std::string("ppsSignal"),
	   &btn_gpio_cfg
	);

    bsp_display_start(); // Initialize display and LVGL

    bsp_display_lock(0);

    //********************
    // display the TabView
    //********************

    // all objects in tabview
    static lv_obj_t * tv;

    tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, 30);
    //lv_obj_add_event_cb(tv, tabview_delete_event_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t * t1 = NULL;
    lv_obj_t * t2 = NULL;
    lv_obj_t * t3 = NULL;

    t1 = lv_tabview_add_tab(tv, "Cockpit");
    if (sd_card) {
        t2 = lv_tabview_add_tab(tv, "Map");
    }
    t3 = lv_tabview_add_tab(tv, "Settings");

    // set padding to 0 in all tabs
    lv_obj_set_style_pad_top(t1, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(t1, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(t1, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(t1, 0, LV_PART_MAIN);

    if(sd_card) {
        lv_obj_set_style_pad_top(t2, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(t2, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(t2, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(t2, 0, LV_PART_MAIN);
    }

    lv_obj_set_style_pad_top(t3, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(t3, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(t3, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_right(t3, 0, LV_PART_MAIN);

    bsp_display_unlock();

    //***************************
    // display cockpit and/or map
    //***************************

    /*cockpit*/  lv_gnss_cockpit_init(t1);

    if(sd_card) {
        /*map*/      // Initialize map display
        /*map*/      map_display_init(t2);
        /*map*/
        /*map*/      // Load map for Wilhelmsfeld
        /*map*/      //double lat = 49.47023;
        /*map*/      //double lon = 8.75627;
                     // Load map for position without map tiles for testing
        /*map*/      double lat = 52.47023;
        /*map*/      double lon = 9.75627;
        /*map*/      map_display_load_location(lat, lon);
        /*map*/
        /*map*/      // Add GPS marker
        /*map*/      map_display_add_marker(lat, lon);
    }

    /*settings*/  lv_gnss_settings_init(t3, powerOffCb);

    /*cockpit*/  ppsSignal.RegisterCallbackForEvent(BUTTON_SINGLE_CLICK, ppsSignalCb);

/*****/
    // do nothing
    while(1) {
        I2cDevice *device = i2c->GetDevice("AXP2101");

        uint8_t msb = device->ReadRegister(0x34);
        uint8_t lsb = device->ReadRegister(0x35);

        uint16_t battery_voltage_mv = (msb << 8) | (lsb);

        uint8_t battery_percentage = device->ReadRegister(0xA4); // Value directly represents %

        uint8_t status_reg = device->ReadRegister(0x01);
        uint8_t charge_status = status_reg & 0x07;

        // It is actively charging if the status state is between 0 and 3
        bool is_charging = (charge_status <= 3);

        ESP_LOGI(tag.c_str(), "Battery Voltage: %d mV, Charge State: %s(%d) (%d %%)", battery_voltage_mv, is_charging ? "Charging" : "Discharging", charge_status, battery_percentage);

        lv_gnss_settings_set_battery_values(battery_percentage, is_charging);

        vTaskDelay(30000 / portTICK_PERIOD_MS); // delay 30 seconds
    }
/*****/
}
