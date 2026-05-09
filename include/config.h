#pragma once

// ============================================
// SYSTEM CONFIGURATION (Safe to commit to Git)
// ============================================

// Solinteg Station
// Note: SOLINTEG_STATION_ID is now defined in secrets.h
#define SOLINTEG_API_BASE_URL   "https://lb-eu.solinteg-cloud.com"

// Polling & Timeouts
#define API_POLL_INTERVAL_MS    1000    // Fetch live data every 1 second when awake
#define DISPLAY_SLEEP_TIMEOUT_MS 60000 // Screen off after 1 minute of inactivity

// NTP
#define NTP_SERVER              "pool.ntp.org"
#define NTP_TZ                  "CET-1CEST,M3.5.0,M10.5.0/3" // Central European Time

// Hardware Pin Definitions
// Update these to match your specific ESP32-C3 board wiring
// GC9A01 Display (SPI)
#define TFT_MOSI    7
#define TFT_MISO    8    // Required to avoid spiAttachMISO exception
#define TFT_SCLK    6
#define TFT_CS      10
#define TFT_DC      2
#define TFT_RST     -1   // Not wired
#define TFT_BL      3
#define TFT_FREQ_WRITE 80000000
#define TFT_FREQ_READ  20000000

// CST816S Touch (I2C) - from CST816S(4, 5, 1, 0): sda, scl, rst, int
#define TOUCH_SDA   4
#define TOUCH_SCL   5
#define TOUCH_RST   1
#define TOUCH_INT   0
