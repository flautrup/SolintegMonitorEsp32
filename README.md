# Solinteg Monitor ESP32

An ESP32-based smart energy monitor that fetches live and daily data from the Solinteg Smart Energy Cloud and displays it on a beautiful GC9A01 round IPS display. It features touch-based swipe navigation, real-time power tracking, and dynamic energy flow animations.

## Hardware Requirements

- **Microcontroller:** ESP32-C3 (configured for the `esp32-2424S012C` board)
- **Display:** GC9A01 1.28" Round 240x240 IPS display
- **Touch Controller:** CST816S capacitive touch

## Features

- **Live Dashboard:** Displays current solar production, grid usage, home consumption, and battery status.
- **Daily Statistics:** Tracks maximum daily production and consumption.
- **Animated Energy Flow:** Visualizes how energy is currently moving between the Grid, Solar Panels, House, and Battery.
- **Power Management:** Automatically puts the display to sleep after 1 minute of inactivity to prevent screen burn-in and save power. Touch the screen to wake it up.

## Step-by-Step Setup Instructions

### 1. Prerequisites
- Download and install [Visual Studio Code (VSCode)](https://code.visualstudio.com/).
- Install the **PlatformIO IDE** extension within VSCode.

### 2. Clone the Repository
Clone this repository to your local machine and open the project folder in VSCode. PlatformIO will automatically detect the `platformio.ini` file and download the necessary dependencies (LVGL, LovyanGFX, ArduinoJson, etc.).

### 3. Configure Credentials (Secrets)
The project requires your Wi-Fi details and Solinteg API credentials. For security reasons, these are not tracked in the git repository.

1. Navigate to the `include/` folder.
2. Duplicate or rename `secrets.example.h` to `secrets.h`.
3. Open `secrets.h` and fill in your details:
   ```cpp
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   #define SOLINTEG_USERNAME "YOUR_SOLINTEG_USERNAME"
   #define SOLINTEG_PASSWORD "YOUR_SOLINTEG_PASSWORD"
   #define SOLINTEG_STATION_ID "YOUR_STATION_ID"
   ```

**How to find your Solinteg Password Hash:**
The Solinteg API does not accept plain-text passwords. You must provide the hashed password that the web portal generates.
1. Open Google Chrome (or any modern browser) and navigate to the [Solinteg Cloud login page](https://www.solinteg-cloud.com/).
2. Press `F12` (or Right-Click -> Inspect) to open **Developer Tools** and navigate to the **Network** tab.
3. Type in your email and password on the page, and click login.
4. In the Network tab, look for a network request named `login` (usually pointing to `/user/login`) and click on it.
5. In the details pane on the right, go to the **Payload** (or Request) tab.
6. Look for the `pwd` parameter in the JSON payload. Copy this long alphanumeric string and paste it as your `SOLINTEG_PASSWORD` in `secrets.h`.

**How to find your Station ID:**
1. Log in to the [Solinteg Cloud web portal](https://www.solinteg-cloud.com/).
2. Navigate to your power station details.
3. Look at the URL in your browser. It will look something like this: `.../detail?id=68e39beb453729541efe79e2&name=...`
4. The long alphanumeric string after `id=` is your Station ID.

*Note: To avoid getting logged out of the official Solinteg mobile app, we highly recommend creating a secondary "Viewer" account in the Solinteg web dashboard and using those credentials for the ESP32.*

### 4. Build and Upload
1. Connect your ESP32-C3 board to your computer via USB.
2. In VSCode, click the **PlatformIO: Upload** button (the right-pointing arrow icon in the bottom blue status bar).
3. PlatformIO will compile the C++ firmware and flash it to the ESP32.
4. Once flashed, click the **PlatformIO: Serial Monitor** button (the plug icon) to monitor the boot sequence, verify the Wi-Fi connection, and ensure it successfully fetches data from the Solinteg API.
