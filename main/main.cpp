#include <string>
#include <vector>
#include <sstream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

#include "lv_components.h" // my lvgl components

#include "generic_button.hpp"
#include "generic_uart.hpp"
GenericUart *gnssUart; // pointer to GenericUart class

#include "driver/i2c_master.h"

#define LOG_MEM_INFO (0)

/*******/
std::string tag("CORE2 GNSS Display");

// Callback function for PPS signal from LC76G GNSS Module
extern "C" void ppsSignalCb(void *arg, void *data)
{
    ESP_LOGI(tag.c_str(), "Callback for PPS signal called!");

    int i;

    int angle = 0;
    int speed = 0;
    int altitude = 0;
    std::string xlatitude;
    std::string xlongitude;
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
            if(latitude.length() >= 13) {
                xlatitude = latitude.substr(0,2).append("° ").append(latitude.substr(2,9)).append("' ").append(latitude.substr(12,1));
            }
            // verwende longitude im label widget laenge (longitude)
            if(longitude.length() >= 14) {
                xlongitude = longitude.substr(0,3).append("° ").append(longitude.substr(3,9)).append("' ").append(longitude.substr(13,1));
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
        ESP_LOGI(tag.c_str(), "speed: %d, angle: %d, altitude: %d, latitude: %s, longitude: %s, date: %s, time: %s, nrOfSats: %d", speed, angle, altitude, xlatitude.c_str(), xlongitude.c_str(), xdate.c_str(), xtime.c_str(), nrOfSats);

        // set current display values
        bsp_display_lock(0);
        lv_gnss_display_set_current_values(angle, speed, altitude, xlatitude.c_str(), xlongitude.c_str(), xdate.c_str(), xtime.c_str(), nrOfSats);
        bsp_display_unlock();
    }
}

// The AXP2101 default I2C slave address
#define AXP2101_I2C_ADDR   0x34

// Callback-Funktion für den PowerOff Button click
extern "C" void powerOffCb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        ESP_LOGI(tag.c_str(), "PowerOff button clicked!");

    // 1. Initialize the board's I2C bus via the BSP wrapper
    esp_err_t ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag.c_str(), "Failed to initialize standard BSP I2C bus");
        return;
    }

    // 2. Fetch the native ESP-IDF driver master bus handle
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();

    // 3. Register the AXP2101 device wrapper context onto the active bus
    i2c_master_dev_handle_t axp_dev_handle;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDR,
        .scl_speed_hz = 400000, // Standard 400kHz I2C speed
    };

    ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &axp_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(tag.c_str(), "Failed to add AXP2101 device to the I2C master bus");
        return;
    }

    ESP_LOGI(tag.c_str(), "AXP2101 I2C interface successfully established via esp_bsp!");

    ESP_LOGI(tag.c_str(), "Power Off!");
    // wait a moment ...
    vTaskDelay(500 / portTICK_PERIOD_MS); // delay 0.5 seconds

    // AXP2101 PowerOff
    uint8_t powerOff[2] = {0x10, 0x01};

    ESP_ERROR_CHECK(i2c_master_transmit(axp_dev_handle, powerOff, 2, -1));

    }
}

extern "C" void app_main(void)
{
/*******/
    vTaskDelay(500 / portTICK_PERIOD_MS); // delay 0.5 seconds

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

    ppsSignal.RegisterCallbackForEvent(BUTTON_SINGLE_CLICK, ppsSignalCb);

    ESP_LOGI(tag.c_str(), "Configure Display");

    /* Initialize display and LVGL */
    bsp_display_start();
    // Set display brightness to 100%
    //bsp_display_backlight_on();

    // Set display brightness to 50%
    bsp_display_brightness_set(50);

    bsp_display_lock(0);
    lv_gnss_display(powerOffCb);
    bsp_display_unlock();

/*****/
    // do nothing
    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS); // delay 1 seconds
    }
/*****/
}
