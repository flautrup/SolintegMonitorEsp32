/**
 * display_hal.h
 * Display and LVGL hardware abstraction layer declarations.
 */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool display_init(void);
void display_wake(void);
void display_sleep(void);
bool display_is_awake(void);
bool display_touch_available(void);
void display_tick(void);

#ifdef __cplusplus
}
#endif
