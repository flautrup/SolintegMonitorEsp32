/**
 * network_api.cpp
 *
 * Arduino-framework implementation using:
 *  - WiFi.h     for Wi-Fi connection
 *  - HTTPClient for HTTP/HTTPS requests
 *  - configTime  for NTP synchronisation
 *  - ArduinoJson for JSON parsing
 */

#include "network_api.h"
#include "config.h"
#include "secrets.h"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static const char *TAG = "network_api";

// JWT auth token (valid 60 min)
static char  s_auth_token[512] = {0};
static time_t s_token_expiry   = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Wi-Fi
// ─────────────────────────────────────────────────────────────────────────────

bool network_wifi_connect(void) {
    Serial.printf("[%s] Connecting to SSID: %s\n", TAG, WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 30000) {
            Serial.printf("[%s] Wi-Fi connection timed out\n", TAG);
            return false;
        }
        delay(250);
    }
    Serial.printf("[%s] Connected. IP: %s\n", TAG, WiFi.localIP().toString().c_str());
    return true;
}

bool network_wifi_is_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}

// ─────────────────────────────────────────────────────────────────────────────
// NTP Time Sync
// ─────────────────────────────────────────────────────────────────────────────

bool network_ntp_sync(void) {
    configTime(0, 0, NTP_SERVER); // UTC first, then apply TZ
    setenv("TZ", NTP_TZ, 1);
    tzset();

    Serial.printf("[%s] Waiting for NTP sync...\n", TAG);
    unsigned long start = millis();
    time_t now = 0;
    while (now < 1000000000L) { // Wait for a valid epoch
        if (millis() - start > 10000) {
            Serial.printf("[%s] NTP sync timed out\n", TAG);
            return false;
        }
        delay(200);
        time(&now);
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.printf("[%s] Time synced: %s\n", TAG, buf);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Solinteg Cloud HTTP Client
// ─────────────────────────────────────────────────────────────────────────────

bool solinteg_login(void) {
    time_t now;
    time(&now);

    // Reuse existing token if still valid (with 5-min buffer)
    if (s_auth_token[0] != '\0' && now < (s_token_expiry - 300)) {
        return true;
    }

    Serial.printf("[%s] Logging in to Solinteg Cloud...\n", TAG);

    HTTPClient http;
    http.begin(SOLINTEG_API_BASE_URL "/gen2api/pc/user/login");
    http.addHeader("Content-Type", "application/json");

    char body[256];
    snprintf(body, sizeof(body),
             "{\"account\":\"%s\",\"pwd\":\"%s\",\"isReadAgreement\":true}",
             SOLINTEG_USERNAME, SOLINTEG_PASSWORD);

    int status = http.POST(body);
    if (status != 200) {
        Serial.printf("[%s] Login failed, HTTP %d\n", TAG, status);
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    // Parse: {"errorCode":0,"body":"<jwt>","successful":true}
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, response);
    if (err || !doc["successful"].as<bool>()) {
        Serial.printf("[%s] Login parse error: %s\n", TAG, err.c_str());
        return false;
    }

    const char *token = doc["body"][0];
    if (!token) {
        Serial.printf("[%s] No token in login response\n", TAG);
        return false;
    }

    strncpy(s_auth_token, token, sizeof(s_auth_token) - 1);
    s_token_expiry = now + 3600;
    Serial.printf("[%s] Login successful\n", TAG);
    return true;
}

bool solinteg_fetch_current(SolintegCurrentData *out) {
    if (!out) return false;
    out->data_valid = false;

    if (!solinteg_login()) return false;

    char url[256];
    snprintf(url, sizeof(url),
             "%s/gen2api/pc/owner/station/stationCurrentInfo/%s/system",
             SOLINTEG_API_BASE_URL, SOLINTEG_STATION_ID);

    HTTPClient http;
    http.begin(url);
    http.addHeader("token", s_auth_token);
    http.addHeader("Content-Type", "application/json");

    int status = http.GET();
    if (status != 200) {
        Serial.printf("[%s] Current data fetch failed, HTTP %d\n", TAG, status);
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    // Increase to 16KB to prevent NoMemory errors with large payloads
    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.printf("[%s] Current JSON parse error: %s\n", TAG, err.c_str());
        return false;
    }

    JsonObject body = doc["body"];
    out->solar_power_kw      = body["pvPower"]      | 0.0f;
    out->grid_power_kw       = body["meterPower"]   | 0.0f;
    out->battery_power_kw    = body["batteryPower"] | 0.0f;
    out->home_consumption_kw = body["loadPower"]    | 0.0f;
    out->battery_soc_pct     = body["soc"]          | 0.0f;
    out->data_valid = true;

    // Track daily values internally
    static float s_max_solar = 0;
    static float s_max_load = 0;
    static float s_total_solar = 0;
    s_max_solar = std::max(s_max_solar, out->solar_power_kw);
    s_max_load = std::max(s_max_load, out->home_consumption_kw);
    s_total_solar = body["powerGenerationToday"] | s_total_solar;
    
    // Store for fetch_daily
    extern SolintegDailyData g_tracked_daily;
    g_tracked_daily.total_solar_kwh = s_total_solar;
    g_tracked_daily.total_consumption_kwh = 0.0f; // Not available in current endpoint
    g_tracked_daily.max_solar_kw = s_max_solar;
    g_tracked_daily.max_consumption_kw = s_max_load;
    g_tracked_daily.data_valid = true;

    Serial.printf("[%s] Current: Solar=%.2fkW Grid=%.2fkW Bat=%.2fkW Load=%.2fkW SoC=%.0f%%\n",
                  TAG, out->solar_power_kw, out->grid_power_kw,
                  out->battery_power_kw, out->home_consumption_kw, out->battery_soc_pct);
    return true;
}

SolintegDailyData g_tracked_daily = {0, 0, 0, 0, false};

bool solinteg_fetch_daily(SolintegDailyData *out) {
    if (!out) return false;
    
    // Using locally tracked daily maximums and totals collected from fetch_current()
    // instead of generating a 300KB history array payload which caused NoMemory.
    *out = g_tracked_daily;
    
    if (out->data_valid) {
        Serial.printf("[%s] Daily Tracker: SolarToday=%.1fkWh MaxSolar=%.2fkW MaxLoad=%.2fkW\n",
                      TAG, out->total_solar_kwh, out->max_solar_kw, out->max_consumption_kw);
        return true;
    }
    
    return false;
}
