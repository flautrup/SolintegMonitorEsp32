 Solinteg Monitor ESP32

## Goal
Create a ESP32 that can fetch data from Solinteg Smart Energy Cloud and display it on a GC9A01 round 240x240 IPS display.
The focus whould be on showing the current power production, consumption and battery status as well as the max for the day.
It can also have some type of animation showing how the energy is flowing in the system.

## User flow
1. The ESP32 is connected to wifi (credentials and Solinteg API details are hardcoded in a configuration file for now).
2. There are tree different screens that can be displayed. 
3. The user can swipe between the screens to view the data.
4. The first screen shows an animation with four parts (Grid, Solar, House, Battery) depicting how energy flows between them.
5. The Second screen shows the current production, consumption and battery status 
6. The third show the max production during the day and max consumption during the day.

## Implemented Features
- The days data can be found here https://lb-eu.solinteg-cloud.com/gen2api/pc/owner/station/statistics/station/new?date=2026-03-13&dateType=DAY&stationId=[STATION_ID]
- Current  information https://lb-eu.solinteg-cloud.com/gen2api/pc/owner/station/stationCurrentInfo/[STATION_ID]/system
- Code on this page return the information we need https://www.solinteg-cloud.com/power-station/detail?id=[STATION_ID]&name=[STATION_NAME]
- Anläggningsnamn is [STATION_NAME]

# Authentication
  curl 'https://lb-eu.solinteg-cloud.com/gen2api/pc/user/login' \
  -H 'accept: application/json, text/plain, */*' \
  -H 'accept-language: en-US,en;q=0.9,sv;q=0.8,da;q=0.7' \
  -H 'cache-control: no-cache' \
  -H 'content-type: application/json;charset=UTF-8' \
  --data-raw '{"account":"[EMAIL_ADDRESS]","pwd":"[PASSWORD]"}'

## Platformio.ini example file of working project
[env:esp32-2424S012C]
platform = espressif32
board = esp32-2424S012C
framework = arduino
monitor_speed = 115200
board_build.partitions = huge_app.csv

lib_deps =
    lvgl/lvgl@^8.3.9
    lovyan03/LovyanGFX@^1.1.12
    fbiego/CST816S@^1.1.1
    h2zero/NimBLE-Arduino@^1.4.1

build_flags =
    -D LV_CONF_SKIP=1
    -D LV_COLOR_DEPTH=16
    -D LV_COLOR_16_SWAP=1
    -D LV_TICK_CUSTOM=1
    -D LV_USE_THEME_MONO=1
    -D LV_USE_IMG_TRANSFORM=1
    -D LV_MEM_SIZE=32768
    -D LV_FONT_MONTSERRAT_24=1
    -D LV_FONT_MONTSERRAT_32=1
    -D LV_FONT_MONTSERRAT_48=1
    -I src

## Hardware
- ESP32-C3
- GC9A01 round 240x240 IPS display
- CST816S touch controller

## Technical Stack
- ~~**Framework:** PlatformIO with Arduino~~
- **Graphics Library:** LVGL
- **Credentials:** Hardcoded in a dedicated configuration file.

## System Behavior & Edge Cases
- **Time Synchronization (NTP):** The ESP32 will use NTP over Wi-Fi to sync its internal clock upon boot. This is required to construct the dynamic date string for the daily statistics API.
- **API Polling Rate:** The system will poll the Solinteg Cloud API once every second *while the display is awake*. (Note: Real-world rate limits from the Solinteg API may reject aggressive 1-second polling, so rate-limit fallback and error handling will be implemented).
- **Display Power Management:** The GC9A01 display will automatically turn off (sleep) after 1 minute of inactivity to save power and prevent screen burn-in. It will wake up instantly when the CST816S touch controller detects a touch.
- **Error States:** The UI must provide visual feedback (e.g., an icon or fallback screen) if the Wi-Fi disconnects or if the Solinteg API becomes unreachable.
