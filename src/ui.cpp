/**
 * ui.cpp
 *
 * Three-screen LVGL UI for the Solinteg Monitor:
 *
 *  Screen 0 – Energy Flow Animation
 *    Four node icons (Grid, Solar, House, Battery) arranged in a circle
 *    on a 240x240 round display. Animated arcs drawn between nodes whose
 *    power flows are non-zero; arc colour and direction indicate flow.
 *
 *  Screen 1 – Current Status
 *    Real-time kW values for Solar, Grid, Battery, and Home load.
 *    Battery SoC shown as a radial arc meter.
 *
 *  Screen 2 – Daily Summary
 *    Peak kW and total kWh for solar and consumption.
 */

#include "ui.h"
#include <lvgl.h>
#include <stdio.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
// Colour palette
// ─────────────────────────────────────────────────────────────────────────────
#define COL_BG          lv_color_hex(0x0D0D0D)  // Near-black background
#define COL_SOLAR       lv_color_hex(0xFFD700)  // Gold – Solar
#define COL_GRID        lv_color_hex(0x4FC3F7)  // Light-blue – Grid
#define COL_BATTERY     lv_color_hex(0x66BB6A)  // Green – Battery
#define COL_HOME        lv_color_hex(0xEF9A9A)  // Salmon – Home load
#define COL_FLOW_IN     lv_color_hex(0x00E676)  // Bright green – power flowing in
#define COL_FLOW_OUT    lv_color_hex(0xFF5252)  // Red – power flowing out
#define COL_LABEL       lv_color_hex(0xFFFFFF)  // White text
#define COL_DIM         lv_color_hex(0x444444)  // Dim – inactive flow arc
#define COL_ERROR       lv_color_hex(0xFF1744)  // Error red

// ─────────────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t *s_scr_flow    = nullptr;
static lv_obj_t *s_scr_current = nullptr;
static lv_obj_t *s_scr_daily   = nullptr;
static lv_obj_t *s_error_label = nullptr;

// Screen 0 – flow arc objects (4 arcs between adjacent nodes)
static lv_obj_t *s_arc_solar_home    = nullptr;
static lv_obj_t *s_arc_battery_home  = nullptr;
static lv_obj_t *s_arc_grid_home     = nullptr;
static lv_obj_t *s_arc_solar_battery = nullptr;
// Node value labels on the flow screen
static lv_obj_t *s_lbl_flow_solar   = nullptr;
static lv_obj_t *s_lbl_flow_grid    = nullptr;
static lv_obj_t *s_lbl_flow_battery = nullptr;
static lv_obj_t *s_lbl_flow_home    = nullptr;

// Screen 1 – current status labels
static lv_obj_t *s_lbl_solar_kw   = nullptr;
static lv_obj_t *s_lbl_grid_kw    = nullptr;
static lv_obj_t *s_lbl_battery_kw = nullptr;
static lv_obj_t *s_lbl_home_kw    = nullptr;
static lv_obj_t *s_arc_soc        = nullptr;
static lv_obj_t *s_lbl_soc        = nullptr;

// Screen 2 – daily summary labels
static lv_obj_t *s_lbl_daily_pv_total  = nullptr;
static lv_obj_t *s_lbl_daily_pv_peak   = nullptr;
static lv_obj_t *s_lbl_daily_load_total = nullptr;
static lv_obj_t *s_lbl_daily_load_peak  = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static lv_style_t s_style_scr;
static lv_style_t s_style_val_big;
static lv_style_t s_style_label_small;
static lv_style_t s_style_node_label;

static void apply_screen_style(lv_obj_t *scr) {
    lv_obj_add_style(scr, &s_style_scr, 0);
}

/** Create a coloured node circle with a centred text label */
static lv_obj_t *create_node(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                              lv_color_t color, const char *label) {
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_set_size(circle, 44, 44);
    lv_obj_set_pos(circle, x - 22, y - 22);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, color, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_100, 0);
    lv_obj_set_style_border_width(circle, 2, 0);
    lv_obj_set_style_border_color(circle, COL_LABEL, 0);
    lv_obj_set_style_pad_all(circle, 0, 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(circle);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, COL_LABEL, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_center(lbl);

    return circle;
}

/** Clamp a float kW value and format "±X.X kW" */
static void fmt_kw(char *buf, size_t len, float kw) {
    if (fabsf(kw) < 0.01f) {
        snprintf(buf, len, "0.0 kW");
    } else {
        snprintf(buf, len, "%+.1f kW", kw);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Swipe / tab gesture handler
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t *s_screens[3];
static int s_current_screen = 0;

static void on_swipe(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    if (code == LV_EVENT_GESTURE) {
        if (dir == LV_DIR_LEFT && s_current_screen < 2) {
            s_current_screen++;
            lv_scr_load_anim(s_screens[s_current_screen],
                             LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        } else if (dir == LV_DIR_RIGHT && s_current_screen > 0) {
            s_current_screen--;
            lv_scr_load_anim(s_screens[s_current_screen],
                             LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen 0 – Energy Flow Animation
// ─────────────────────────────────────────────────────────────────────────────
// Node positions on a 240x240 circle (r=80 from centre 120,120):
//   Solar   top     120, 40
//   Home    right   200, 120
//   Battery bottom  120, 200
//   Grid    left     40, 120

#define CX 120
#define CY 120
#define NODE_R 80

#define NX_SOLAR    120
#define NY_SOLAR    40
#define NX_HOME     200
#define NY_HOME     120
#define NX_BATTERY  120
#define NY_BATTERY  200
#define NX_GRID     40
#define NY_GRID     120

static void build_screen_flow(void) {
    s_scr_flow = lv_obj_create(nullptr);
    apply_screen_style(s_scr_flow);
    lv_obj_add_event_cb(s_scr_flow, on_swipe, LV_EVENT_GESTURE, nullptr);

    // ── Flow arcs (drawn behind nodes) ──────────────────────────────────────
    // Each arc represents a power flow path between two nodes.
    // We use LVGL arcs positioned at centre of the screen, width chosen to
    // reach between nodes. Angles are approximate cardinal directions.
    // Arc: solar → home (top → right, quarter arc clockwise)
    s_arc_solar_home = lv_arc_create(s_scr_flow);
    lv_obj_set_size(s_arc_solar_home, 160, 160);
    lv_obj_center(s_arc_solar_home);
    lv_arc_set_bg_angles(s_arc_solar_home, 270, 0); // top to right
    lv_arc_set_angles(s_arc_solar_home, 270, 0);
    lv_arc_set_value(s_arc_solar_home, 50);
    lv_obj_set_style_arc_width(s_arc_solar_home, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc_solar_home, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_solar_home, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_solar_home, COL_SOLAR, LV_PART_INDICATOR);
    lv_arc_set_mode(s_arc_solar_home, LV_ARC_MODE_NORMAL);
    lv_obj_remove_style(s_arc_solar_home, nullptr, LV_PART_KNOB);

    // Arc: solar → battery (top → bottom, half arc left side)
    s_arc_solar_battery = lv_arc_create(s_scr_flow);
    lv_obj_set_size(s_arc_solar_battery, 130, 130);
    lv_obj_center(s_arc_solar_battery);
    lv_arc_set_bg_angles(s_arc_solar_battery, 180, 270);
    lv_arc_set_angles(s_arc_solar_battery, 180, 270);
    lv_obj_set_style_arc_width(s_arc_solar_battery, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc_solar_battery, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_solar_battery, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_solar_battery, COL_BATTERY, LV_PART_INDICATOR);
    lv_obj_remove_style(s_arc_solar_battery, nullptr, LV_PART_KNOB);

    // Arc: battery → home (bottom → right, quarter arc)
    s_arc_battery_home = lv_arc_create(s_scr_flow);
    lv_obj_set_size(s_arc_battery_home, 160, 160);
    lv_obj_center(s_arc_battery_home);
    lv_arc_set_bg_angles(s_arc_battery_home, 0, 90);
    lv_arc_set_angles(s_arc_battery_home, 0, 90);
    lv_obj_set_style_arc_width(s_arc_battery_home, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc_battery_home, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_battery_home, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_battery_home, COL_HOME, LV_PART_INDICATOR);
    lv_obj_remove_style(s_arc_battery_home, nullptr, LV_PART_KNOB);

    // Arc: grid → home (left → right, horizontal through centre, top half arc)
    s_arc_grid_home = lv_arc_create(s_scr_flow);
    lv_obj_set_size(s_arc_grid_home, 160, 160);
    lv_obj_center(s_arc_grid_home);
    lv_arc_set_bg_angles(s_arc_grid_home, 90, 180);
    lv_arc_set_angles(s_arc_grid_home, 90, 180);
    lv_obj_set_style_arc_width(s_arc_grid_home, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc_grid_home, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_grid_home, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc_grid_home, COL_GRID, LV_PART_INDICATOR);
    lv_obj_remove_style(s_arc_grid_home, nullptr, LV_PART_KNOB);

    // ── Node circles ─────────────────────────────────────────────────────────
    create_node(s_scr_flow, NX_SOLAR,   NY_SOLAR,   COL_SOLAR,   "☀");
    create_node(s_scr_flow, NX_HOME,    NY_HOME,    COL_HOME,    "⌂");
    create_node(s_scr_flow, NX_BATTERY, NY_BATTERY, COL_BATTERY, "▮");
    create_node(s_scr_flow, NX_GRID,    NY_GRID,    COL_GRID,    "⚡");

    // ── Node kW sub-labels ────────────────────────────────────────────────────
    s_lbl_flow_solar = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_solar, NX_SOLAR - 30, NY_SOLAR + 24);
    lv_obj_set_width(s_lbl_flow_solar, 60);
    lv_obj_set_style_text_align(s_lbl_flow_solar, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_lbl_flow_solar, &s_style_label_small, 0);
    lv_label_set_text(s_lbl_flow_solar, "0.0kW");

    s_lbl_flow_grid = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_grid, NX_GRID - 36, NY_GRID + 24);
    lv_obj_set_width(s_lbl_flow_grid, 72);
    lv_obj_set_style_text_align(s_lbl_flow_grid, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_lbl_flow_grid, &s_style_label_small, 0);
    lv_label_set_text(s_lbl_flow_grid, "0.0kW");

    s_lbl_flow_battery = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_battery, NX_BATTERY - 30, NY_BATTERY + 24);
    lv_obj_set_width(s_lbl_flow_battery, 60);
    lv_obj_set_style_text_align(s_lbl_flow_battery, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_lbl_flow_battery, &s_style_label_small, 0);
    lv_label_set_text(s_lbl_flow_battery, "0.0kW");

    s_lbl_flow_home = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_home, NX_HOME - 60, NY_HOME + 24);
    lv_obj_set_width(s_lbl_flow_home, 56);
    lv_obj_set_style_text_align(s_lbl_flow_home, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_lbl_flow_home, &s_style_label_small, 0);
    lv_label_set_text(s_lbl_flow_home, "0.0kW");
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen 1 – Current Status
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t *make_metric_row(lv_obj_t *parent, lv_coord_t y,
                                  const char *icon, lv_color_t icon_color,
                                  lv_obj_t **val_label_out) {
    // Icon
    lv_obj_t *icon_lbl = lv_label_create(parent);
    lv_label_set_text(icon_lbl, icon);
    lv_obj_set_pos(icon_lbl, 28, y);
    lv_obj_set_style_text_color(icon_lbl, icon_color, 0);
    lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_24, 0);

    // Value
    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "--.-kW");
    lv_obj_set_pos(val, 70, y);
    lv_obj_add_style(val, &s_style_val_big, 0);
    *val_label_out = val;
    return val;
}

static void build_screen_current(void) {
    s_scr_current = lv_obj_create(nullptr);
    apply_screen_style(s_scr_current);
    lv_obj_add_event_cb(s_scr_current, on_swipe, LV_EVENT_GESTURE, nullptr);

    // Title
    lv_obj_t *title = lv_label_create(s_scr_current);
    lv_label_set_text(title, "Live");
    lv_obj_set_pos(title, 0, 14);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, COL_LABEL, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    make_metric_row(s_scr_current, 46,  "☀", COL_SOLAR,   &s_lbl_solar_kw);
    make_metric_row(s_scr_current, 78,  "⚡", COL_GRID,    &s_lbl_grid_kw);
    make_metric_row(s_scr_current, 110, "▮", COL_BATTERY, &s_lbl_battery_kw);
    make_metric_row(s_scr_current, 142, "⌂", COL_HOME,    &s_lbl_home_kw);

    // Battery SoC arc
    s_arc_soc = lv_arc_create(s_scr_current);
    lv_obj_set_size(s_arc_soc, 70, 70);
    lv_obj_set_pos(s_arc_soc, 85, 162);
    lv_arc_set_rotation(s_arc_soc, 135);
    lv_arc_set_bg_angles(s_arc_soc, 0, 270);
    lv_arc_set_value(s_arc_soc, 0);
    lv_obj_set_style_arc_color(s_arc_soc, COL_BATTERY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc_soc, COL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc_soc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc_soc, 8, LV_PART_MAIN);
    lv_obj_remove_style(s_arc_soc, nullptr, LV_PART_KNOB);

    s_lbl_soc = lv_label_create(s_scr_current);
    lv_label_set_text(s_lbl_soc, "-%");
    lv_obj_align_to(s_lbl_soc, s_arc_soc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(s_lbl_soc, COL_BATTERY, 0);
    lv_obj_set_style_text_font(s_lbl_soc, &lv_font_montserrat_24, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen 2 – Daily Summary
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t *make_daily_row(lv_obj_t *parent, lv_coord_t y,
                                 const char *label_text, lv_color_t col,
                                 lv_obj_t **out) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_pos(lbl, 12, y);
    lv_obj_set_style_text_color(lbl, col, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);

    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "---");
    lv_obj_set_pos(val, 12, y + 24);
    lv_obj_set_style_text_color(val, COL_LABEL, 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_24, 0);
    *out = val;
    return val;
}

static void build_screen_daily(void) {
    s_scr_daily = lv_obj_create(nullptr);
    apply_screen_style(s_scr_daily);
    lv_obj_add_event_cb(s_scr_daily, on_swipe, LV_EVENT_GESTURE, nullptr);

    lv_obj_t *title = lv_label_create(s_scr_daily);
    lv_label_set_text(title, "Today");
    lv_obj_set_pos(title, 0, 14);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, COL_LABEL, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    make_daily_row(s_scr_daily, 50,  "☀ Solar Total",   COL_SOLAR,   &s_lbl_daily_pv_total);
    make_daily_row(s_scr_daily, 104, "☀ Solar Peak",    COL_SOLAR,   &s_lbl_daily_pv_peak);
    make_daily_row(s_scr_daily, 158, "⌂ Load Total",    COL_HOME,    &s_lbl_daily_load_total);
    make_daily_row(s_scr_daily, 180, "⌂ Load Peak",     COL_HOME,    &s_lbl_daily_load_peak);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void ui_init(void) {
    // ── Shared styles ─────────────────────────────────────────────────────────
    lv_style_init(&s_style_scr);
    lv_style_set_bg_color(&s_style_scr, COL_BG);
    lv_style_set_bg_opa(&s_style_scr, LV_OPA_100);
    lv_style_set_border_width(&s_style_scr, 0);
    lv_style_set_radius(&s_style_scr, 0);
    lv_style_set_pad_all(&s_style_scr, 0);

    lv_style_init(&s_style_val_big);
    lv_style_set_text_color(&s_style_val_big, COL_LABEL);
    lv_style_set_text_font(&s_style_val_big, &lv_font_montserrat_24);

    lv_style_init(&s_style_label_small);
    lv_style_set_text_color(&s_style_label_small, COL_LABEL);
    lv_style_set_text_font(&s_style_label_small, &lv_font_montserrat_24);

    // ── Build screens ─────────────────────────────────────────────────────────
    build_screen_flow();
    build_screen_current();
    build_screen_daily();

    s_screens[0] = s_scr_flow;
    s_screens[1] = s_scr_current;
    s_screens[2] = s_scr_daily;

    // Error label (shown on whichever screen is active)
    s_error_label = lv_label_create(lv_layer_top());
    lv_label_set_text(s_error_label, LV_SYMBOL_WIFI " Offline");
    lv_obj_set_style_text_color(s_error_label, COL_ERROR, 0);
    lv_obj_set_style_text_font(s_error_label, &lv_font_montserrat_24, 0);
    lv_obj_align(s_error_label, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);

    // Show the flow animation screen first
    lv_scr_load(s_scr_flow);
    s_current_screen = 0;
}

void ui_update_current(const SolintegCurrentData *data) {
    if (!data) return;
    char buf[32];

    // ── Flow screen node labels ───────────────────────────────────────────────
    snprintf(buf, sizeof(buf), "%.1fkW", data->solar_power_kw);
    lv_label_set_text(s_lbl_flow_solar, buf);
    snprintf(buf, sizeof(buf), "%.1fkW", data->home_consumption_kw);
    lv_label_set_text(s_lbl_flow_home, buf);
    snprintf(buf, sizeof(buf), "%.1fkW", fabsf(data->battery_power_kw));
    lv_label_set_text(s_lbl_flow_battery, buf);
    snprintf(buf, sizeof(buf), "%.1fkW", fabsf(data->grid_power_kw));
    lv_label_set_text(s_lbl_flow_grid, buf);

    // ── Flow arc colours: bright = active, dim = inactive ────────────────────
    lv_obj_set_style_arc_color(s_arc_solar_home,
        data->solar_power_kw > 0.05f ? COL_SOLAR : COL_DIM, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc_solar_battery,
        data->battery_power_kw > 0.05f ? COL_BATTERY : COL_DIM, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc_battery_home,
        data->battery_power_kw < -0.05f ? COL_FLOW_IN : COL_DIM, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc_grid_home,
        data->grid_power_kw > 0.05f ? COL_GRID : COL_DIM, LV_PART_INDICATOR);

    // ── Current status screen ─────────────────────────────────────────────────
    snprintf(buf, sizeof(buf), "%.1f kW", data->solar_power_kw);
    lv_label_set_text(s_lbl_solar_kw, buf);

    fmt_kw(buf, sizeof(buf), data->grid_power_kw);
    lv_label_set_text(s_lbl_grid_kw, buf);

    fmt_kw(buf, sizeof(buf), data->battery_power_kw);
    lv_label_set_text(s_lbl_battery_kw, buf);

    snprintf(buf, sizeof(buf), "%.1f kW", data->home_consumption_kw);
    lv_label_set_text(s_lbl_home_kw, buf);

    // SoC arc and label
    int soc = (int)data->battery_soc_pct;
    lv_arc_set_value(s_arc_soc, soc);
    snprintf(buf, sizeof(buf), "%d%%", soc);
    lv_label_set_text(s_lbl_soc, buf);
    lv_obj_align_to(s_lbl_soc, s_arc_soc, LV_ALIGN_CENTER, 0, 0);
}

void ui_update_daily(const SolintegDailyData *data) {
    if (!data) return;
    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f kWh", data->total_solar_kwh);
    lv_label_set_text(s_lbl_daily_pv_total, buf);

    snprintf(buf, sizeof(buf), "%.2f kW", data->max_solar_kw);
    lv_label_set_text(s_lbl_daily_pv_peak, buf);

    snprintf(buf, sizeof(buf), "%.1f kWh", data->total_consumption_kwh);
    lv_label_set_text(s_lbl_daily_load_total, buf);

    snprintf(buf, sizeof(buf), "%.2f kW", data->max_consumption_kw);
    lv_label_set_text(s_lbl_daily_load_peak, buf);
}

void ui_show_error(bool show) {
    if (show) {
        lv_obj_clear_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    }
}
