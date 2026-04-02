/**
 * main.cpp – Arduino framework entry point
 *
 * Startup sequence:
 *   1. Init Serial
 *   2. Init display + LVGL
 *   3. Show "Connecting…" splash
 *   4. Connect Wi-Fi
 *   5. Sync NTP clock
 *   6. Login to Solinteg Cloud
 *   7. Build UI screens
 *   8. Enter loop: poll data, drive LVGL, manage sleep
 */

#include <Arduino.h>
#include <lvgl.h>
#include "display_hal.h"
#include "network_api.h"
#include "ui.h"
#include "config.h"
#include "data_types.h"

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────
static SolintegCurrentData s_current_data = {};
static SolintegDailyData   s_daily_data   = {};

static unsigned long s_last_touch_ms         = 0;
static unsigned long s_last_current_fetch_ms = 0;
static unsigned long s_last_daily_fetch_ms   = 0;

#define DAILY_POLL_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 minutes

// ─────────────────────────────────────────────────────────────────────────────
// Splash helper (renders a message before the main UI is built)
// ─────────────────────────────────────────────────────────────────────────────
static void show_splash(const char *msg) {
    static lv_obj_t *splash_label = nullptr;
    if (!splash_label) {
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x0D0D0D), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_100, 0);
        splash_label = lv_label_create(scr);
        lv_obj_set_width(splash_label, 200);
        lv_label_set_long_mode(splash_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(splash_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(splash_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(splash_label, &lv_font_montserrat_24, 0);
        lv_obj_center(splash_label);
    }
    lv_label_set_text(splash_label, msg);
    lv_timer_handler();
    delay(50);
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch wake / sleep timer
// ─────────────────────────────────────────────────────────────────────────────
static void handle_touch_and_sleep(void) {
    if (display_touch_available()) {
        unsigned long now = millis();
        s_last_touch_ms = now;
        if (!display_is_awake()) {
            display_wake();
        }
    }

    if (display_is_awake()) {
        if ((millis() - s_last_touch_ms) > DISPLAY_SLEEP_TIMEOUT_MS) {
            display_sleep();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Arduino entry points
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[main] Solinteg Monitor starting...");

    // Init display + LVGL
    display_init();
    s_last_touch_ms = millis();

    // Splash
    show_splash(LV_SYMBOL_WIFI "\nConnecting...");

    // Wi-Fi
    bool wifi_ok = network_wifi_connect();
    if (!wifi_ok) {
        show_splash(LV_SYMBOL_WIFI "\nNo network!\nCheck secrets.h");
    }

    // NTP
    if (wifi_ok) {
        show_splash("Syncing clock...");
        network_ntp_sync();
    }

    // Login
    if (wifi_ok) {
        show_splash("Logging in...");
        solinteg_login();
    }

    // Build main UI
    ui_init();

    // Initial data fetch
    if (wifi_ok && network_wifi_is_connected()) {
        if (solinteg_fetch_current(&s_current_data)) {
            ui_update_current(&s_current_data);
        }
        if (solinteg_fetch_daily(&s_daily_data)) {
            ui_update_daily(&s_daily_data);
        }
    }
    s_last_current_fetch_ms = millis();
    s_last_daily_fetch_ms   = millis();

    Serial.println("[main] Setup complete. Entering loop.");
}

void loop() {
    unsigned long now = millis();

    // Touch detection drives the sleep timer
    handle_touch_and_sleep();

    // Drive LVGL
    display_tick();

    if (display_is_awake() && network_wifi_is_connected()) {
        // Poll current data at API_POLL_INTERVAL_MS
        if ((now - s_last_current_fetch_ms) >= API_POLL_INTERVAL_MS) {
            s_last_current_fetch_ms = now;
            bool ok = solinteg_fetch_current(&s_current_data);
            ui_update_current(&s_current_data);
            ui_show_error(!ok);
        }

        // Poll daily data every 5 minutes
        if ((now - s_last_daily_fetch_ms) >= DAILY_POLL_INTERVAL_MS) {
            s_last_daily_fetch_ms = now;
            if (solinteg_fetch_daily(&s_daily_data)) {
                ui_update_daily(&s_daily_data);
            }
        }
    } else if (!network_wifi_is_connected()) {
        ui_show_error(true);
    }

    delay(5); // 5ms tick aligns with lv_tick_inc(5)
}
