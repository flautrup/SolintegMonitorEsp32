/**
 * network_api.h
 * Wi-Fi, NTP, and Solinteg Cloud API client declarations.
 */
#pragma once

#include "data_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise Wi-Fi station mode and connect using credentials from secrets.h.
 *        Blocks until connected or 30 seconds elapse.
 * @return true if connected successfully, false on timeout.
 */
bool network_wifi_connect(void);

/**
 * @brief Returns true if currently connected to Wi-Fi.
 */
bool network_wifi_is_connected(void);

/**
 * @brief Synchronise the system clock via SNTP. Call after Wi-Fi is connected.
 *        Blocks up to 10 seconds waiting for sync.
 * @return true if time was synced successfully.
 */
bool network_ntp_sync(void);

/**
 * @brief Authenticate with the Solinteg Cloud API and store the JWT token.
 *        Token is valid for 60 minutes, re-login happens automatically when expired.
 * @return true if authentication succeeded.
 */
bool solinteg_login(void);

/**
 * @brief Fetch current live system data (solar, grid, battery, home load).
 *        Populates the provided SolintegCurrentData struct.
 * @return true if data was fetched and parsed successfully.
 */
bool solinteg_fetch_current(SolintegCurrentData *out);

/**
 * @brief Fetch today's daily statistics (peak production, peak consumption).
 *        Populates the provided SolintegDailyData struct.
 * @return true if data was fetched and parsed successfully.
 */
bool solinteg_fetch_daily(SolintegDailyData *out);

#ifdef __cplusplus
}
#endif
