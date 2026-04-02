/**
 * ui.h
 * LVGL UI screen declarations.
 */
#pragma once
#include "data_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build all three LVGL screens. Call once after display_init().
 */
void ui_init(void);

/**
 * @brief Update the live data displayed on Screen 2 and the animation on Screen 1.
 * @param data Pointer to latest current data struct.
 */
void ui_update_current(const SolintegCurrentData *data);

/**
 * @brief Update the daily stats displayed on Screen 3.
 * @param data Pointer to latest daily data struct.
 */
void ui_update_daily(const SolintegDailyData *data);

/**
 * @brief Show or hide the "offline / error" indicator overlay.
 * @param show true = show error, false = hide.
 */
void ui_show_error(bool show);

#ifdef __cplusplus
}
#endif
