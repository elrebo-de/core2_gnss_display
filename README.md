# CORE2 GNSS Display

This project uses an M5STACK CORE2 V1.1 SoC together with a Waveshare LC76G GNSS Module to display Satellite data.

![picture](IMG_5866.png "CORE2 GNSS Display Cockpit View")

On the left side of the picture you see the Waveshare LC76G GNSS Module and on the right side the CORE2 V1.1 SoC, which displays the satellite data in the Cockpit tabview.

The Cockpit view shows 

on the right from top to bottom:

* the velocity in km/h
* a compass showing the direction of movement

on the left from top to bottom:

* the altitude im meters above sea level: 166 m asl
* the latitude and longitude in degrees, minutes and seconds: 49°28'12.0284"N and 008°45'21.0121"E
* the latitude and longitude in degrees: 49.47000885 and 008.75583649
* the local date: 26.08.26
* the local time: 00:17:40 CEST
* the nr of satellites: 21

The Map view can be activated by pressing the tab "Map".

![picture](IMG_5867.png "CORE2 GNSS Display Map View")

The Map view shows an OpenStreetMap with a marker at the current location. The map data must be available on the SD card in the CORE2.

The "Settings" view can be activated by pressing the tab "Settings".

![picture](IMG_5868.png "CORE2 GNSS Display Settings View")

Currently no settings can be set, but the system can be powered off with the "Off" button.

The Waveshare LC76G GNSS Module receives the satellite signals and as soon as it has enough information it signals the availability on a PPS (puls per second) pin and transmits the data through a UART connection to the SoC.
 
The standard setting of the LC76G is to send one puls per second. The standard baud rate of the UART is 115200 baud.

In this project we do not need to transmit commands to the LC76G GNSS module, because we use the standard configuration. Therefore we only have to connect four lines and we can use the Grove port on the CORE2 SoC:

| LC76G GNSS Module | Port A of CORE2 | Connection information                    |
|:------------------|:----------------|:------------------------------------------|
| VCC               | 5V              | 5V                                        |
| GND               | G               | Ground                                    |
| PPS               | G32             | connected to a GPIO button for PPS signal |
| TX                | G33             | connected to RX line of UART              |

The project uses the component `elrebo-de/generic_button` to listen for the PPS signal.

For every PPS signal a BUTTON_SINGLE_CLICK event is triggered and the function `ppsSignalCb`
 is called.

To receive the GNSS data it uses the component `elrebo-de/generic_uart`. 

To power off the system it uses the I2C functionality of ESP-IDF.

``` log
I (1543) main_task: Calling app_main()
I (2043) CORE2 GNSS Display: Configure local timezone
I (2043) CORE2 GNSS Display: Configure GenericUart gnssUart
I (2043) gnssUart: constructor
I (2043) gnssUart: UART_HW_FIFO_LEN(2): 128
I (2043) CORE2 GNSS Display: Configure SD card
W (2053) i2c.master: Please check pull-up resistances whether be connected properly. Otherwise unexpected behavior would happen. For more detailed information, please read docs
W (2063) M5Stack: Warning: Long filenames on SD card are disabled in menuconfig!
I (2113) sdspi_transaction: cmd=52, R1 response: command not supported
I (2153) sdspi_transaction: cmd=5, R1 response: command not supported
I (2183) CORE2 GNSS Display: SD card successfully mounted at /sdcard!
I (2183) CORE2 GNSS Display: Configure Display
PSRAM successfully activated! Free storage: 4155324 Bytes
I (2193) CORE2 GNSS Display: Configure GenericButton ppsSignal
I (2193) ppsSignal: Button Type GPIO
I (2193) button: IoT Button Version: 4.1.7
I (2203) LVGL: Starting LVGL task
I (2233) M5Stack: Install panel IO
I (2233) M5Stack: Install LCD driver
I (2233) ili9341: LCD panel create success, version: 2.0.2
I (2843) map_tiles: Map tiles initialized with base path: /sdcard, 1 tile types, current type: esp_sd_tiles, zoom: 17, grid: 3x3
I (2863) basic_map_display: Map display initialized
I (2873) basic_map_display: Loading map for GPS: 49.470230, 8.756270
I (2873) map_tiles: GPS to tile: tile_x=68723, tile_y=44749, offset_x=15, offset_y=209
I (3183) basic_map_display: Loaded tile 0 (68723, 44749)
I (3463) basic_map_display: Loaded tile 1 (68724, 44749)
I (3733) basic_map_display: Loaded tile 2 (68725, 44749)
I (4013) basic_map_display: Loaded tile 3 (68723, 44750)
I (4283) basic_map_display: Loaded tile 4 (68724, 44750)
I (4553) basic_map_display: Loaded tile 5 (68725, 44750)
I (4833) basic_map_display: Loaded tile 6 (68723, 44751)
I (5103) basic_map_display: Loaded tile 7 (68724, 44751)
I (5373) basic_map_display: Loaded tile 8 (68725, 44751)
I (5383) basic_map_display: Map tiles loaded for location
I (5673) ppsSignal: RegisterCallbackForEvent called
I (6623) CORE2 GNSS Display: Callback for PPS signal called!
I (6633) CORE2 GNSS Display: speed: 0, angle: 214, alt: 410, latDegMinSec: 49°28'11.9078"N, latDeg: 49.46997452, lonDegMinSec: 008°45'21.2331"E, lonDeg: 008.75589848, date: 25.08.26, time: 22:10:23, nrOfSats: 10                                                                                                                                               
I (6643) CORE2 GNSS Display: Local time: 26.08.2026 00:10:23 CEST
I (7623) CORE2 GNSS Display: Callback for PPS signal called!
I (7723) CORE2 GNSS Display: speed: 0, angle: 214, alt: 410, latDegMinSec: 49°28'11.8211"N, latDeg: 49.46995163, lonDegMinSec: 008°45'21.2367"E, lonDeg: 008.75589943, date: 25.08.26, time: 22:10:28, nrOfSats: 10                                                                                                                                               
I (7733) CORE2 GNSS Display: Local time: 26.08.2026 00:10:28 CEST
I (8623) CORE2 GNSS Display: Callback for PPS signal called!
I (8723) CORE2 GNSS Display: speed: 1, angle: 179, alt: 410, latDegMinSec: 49°28'11.8333"N, latDeg: 49.46995544, lonDegMinSec: 008°45'21.2511"E, lonDeg: 008.75590324, date: 25.08.26, time: 22:10:29, nrOfSats: 10                                                                                                                                               
I (8733) CORE2 GNSS Display: Local time: 26.08.2026 00:10:29 CEST
I (9623) CORE2 GNSS Display: Callback for PPS signal called!
I (9723) CORE2 GNSS Display: speed: 1, angle: 181, alt: 410, latDegMinSec: 49°28'11.8452"N, latDeg: 49.46995544, lonDegMinSec: 008°45'21.2702"E, lonDeg: 008.75590801, date: 25.08.26, time: 22:10:30, nrOfSats: 10                                                                                                                                               
I (9733) CORE2 GNSS Display: Local time: 26.08.2026 00:10:30 CEST
I (10623) CORE2 GNSS Display: Callback for PPS signal called!
I (10723) CORE2 GNSS Display: speed: 1, angle: 200, alt: 410, latDegMinSec: 49°28'11.8570"N, latDeg: 49.46995926, lonDegMinSec: 008°45'21.2777"E, lonDeg: 008.75591087, date: 25.08.26, time: 22:10:31, nrOfSats: 11                                                   
```

# Known problems

* currently, the zoom for the map display and the initial coordinates have to be set in the program code
* system crashes when position moves out of range of available map tiles

# Next steps

* improve storage model

## Cockpit view

## Map view

* add + and - Buttons to switch zoom level

## Settings view

* set Timezone
* set initial zoom level
