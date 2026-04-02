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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
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
static bool                s_current_dirty = false;
static bool                s_daily_dirty   = false;
static bool                s_fetch_error   = false;

static SemaphoreHandle_t   s_data_mutex = nullptr;
static unsigned long       s_last_touch_ms = 0;

#define DAILY_POLL_INTERVAL_MS (5UL * 60UL * 1000UL)  // 5 minutes

// ─────────────────────────────────────────────────────────────────────────────
// Networking Task
// ─────────────────────────────────────────────────────────────────────────────
void network_task(void *pvParameters) {
    unsigned long last_current = 0;
    unsigned long last_daily = 0;

    while (true) {
        unsigned long now = millis();
        bool wifi_ok = network_wifi_is_connected();

        if (wifi_ok) {
            // Fetch Current Data
            if (now - last_current >= API_POLL_INTERVAL_MS || last_current == 0) {
                SolintegCurrentData tmp_current = {};
                bool ok = solinteg_fetch_current(&tmp_current);
                
                if (xSemaphoreTake(s_data_mutex, portMAX_DELAY)) {
                    if (ok) s_current_data = tmp_current;
                    s_current_dirty = ok;
                    s_fetch_error = !ok;
                    xSemaphoreGive(s_data_mutex);
                }
                last_current = now;
            }

            // Fetch Daily Data
            if (now - last_daily >= DAILY_POLL_INTERVAL_MS || last_daily == 0) {
                SolintegDailyData tmp_daily = {};
                bool ok = solinteg_fetch_daily(&tmp_daily);
                
                if (xSemaphoreTake(s_data_mutex, portMAX_DELAY)) {
                    if (ok) s_daily_data = tmp_daily;
                    s_daily_dirty = ok;
                    xSemaphoreGive(s_data_mutex);
                }
                last_daily = now;
            }
        } else {
             if (xSemaphoreTake(s_data_mutex, portMAX_DELAY)) {
                s_fetch_error = true;
                xSemaphoreGive(s_data_mutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

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

    // Setup Mutex and Task
    s_data_mutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(network_task, "network_task", 8192, NULL, 1, NULL, 0);

    Serial.println("[main] Setup complete. Entering loop.");
}

void loop() {
    // Touch detection drives the sleep timer
    handle_touch_and_sleep();

    // Drive LVGL
    display_tick();

    // Check for new data from network task
    if (xSemaphoreTake(s_data_mutex, 0) == pdTRUE) {
        if (s_current_dirty) {
            ui_update_current(&s_current_data);
            s_current_dirty = false;
        }
        if (s_daily_dirty) {
            ui_update_daily(&s_daily_data);
            s_daily_dirty = false;
        }
        ui_show_error(s_fetch_error);
        xSemaphoreGive(s_data_mutex);
    }

    delay(5); // 5ms tick aligns with lv_tick_inc(5)
}
