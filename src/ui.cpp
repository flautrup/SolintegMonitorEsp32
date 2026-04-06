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

static lv_obj_t *s_lbl_node_solar = nullptr;
static lv_obj_t *s_lbl_node_grid = nullptr;
static lv_obj_t *s_lbl_node_battery = nullptr;
static lv_obj_t *s_lbl_node_home = nullptr;

// Screen 2 – daily summary labels
static lv_obj_t *s_lbl_daily_pv_total  = nullptr;
static lv_obj_t *s_lbl_daily_pv_peak   = nullptr;
static lv_obj_t *s_lbl_daily_load_total = nullptr;
static lv_obj_t *s_lbl_daily_load_peak  = nullptr;

typedef struct {
    lv_obj_t * dot;
    int16_t start_x;
    int16_t start_y;
    int16_t end_x;
    int16_t end_y;
} anim_path_t;

static anim_path_t s_path_solar;
static anim_path_t s_path_grid;
static anim_path_t s_path_battery;
static anim_path_t s_path_home;

static lv_anim_t s_anim_solar;
static lv_anim_t s_anim_grid;
static lv_anim_t s_anim_battery;
static lv_anim_t s_anim_home;

static lv_obj_t *s_line_solar = nullptr;
static lv_obj_t *s_line_grid = nullptr;
static lv_obj_t *s_line_battery = nullptr;
static lv_obj_t *s_line_home = nullptr;

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
                              lv_color_t color, const char *label, lv_obj_t **out_lbl) {
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
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0); // Smaller font for symbols
    lv_obj_center(lbl);

    if (out_lbl) *out_lbl = lbl;
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

static void cycle_screen(bool next) {
    if (next && s_current_screen < 2) {
        s_current_screen++;
        lv_scr_load_anim(s_screens[s_current_screen], LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    } else if (!next && s_current_screen > 0) {
        s_current_screen--;
        lv_scr_load_anim(s_screens[s_current_screen], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
    } else if (next && s_current_screen == 2) { // Loop back to start
        s_current_screen = 0;
        lv_scr_load_anim(s_screens[s_current_screen], LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
}

static void on_screen_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        if (dir == LV_DIR_LEFT) cycle_screen(true);
        else if (dir == LV_DIR_RIGHT) cycle_screen(false);
    } else if (code == LV_EVENT_CLICKED) {
        // Fallback: tap center to cycle forward
        cycle_screen(true);
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
#define NODE_R 75

#define NX_SOLAR    120
#define NY_SOLAR    45
#define NX_HOME     195
#define NY_HOME     120
#define NX_BATTERY  120
#define NY_BATTERY  195
#define NX_GRID     45
#define NY_GRID     120

static void dot_anim_cb(void * var, int32_t v) {
    anim_path_t * p = (anim_path_t *)var;
    lv_coord_t x = p->start_x + ((p->end_x - p->start_x) * v) / 1000;
    lv_coord_t y = p->start_y + ((p->end_y - p->start_y) * v) / 1000;
    lv_obj_set_pos(p->dot, x - 5, y - 5); // Center the 10x10 dot
}

static lv_obj_t * create_line(lv_obj_t * parent, int x1, int y1, int x2, int y2) {
    lv_point_t * pts = (lv_point_t *)malloc(sizeof(lv_point_t) * 2); // SEPARATE memory for each line
    pts[0].x = x1; pts[0].y = y1;
    pts[1].x = x2; pts[1].y = y2;
    lv_obj_t * line = lv_line_create(parent);
    lv_line_set_points(line, pts, 2);
    lv_obj_set_style_line_width(line, 8, 0); // Thicker lines
    lv_obj_set_style_line_color(line, lv_color_hex(0x222222), 0); // Very dim grey by default
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

static void init_anim(lv_anim_t * a, anim_path_t * path, lv_obj_t * dot) {
    path->dot = dot;
    lv_anim_init(a);
    lv_anim_set_var(a, path);
    lv_anim_set_exec_cb(a, dot_anim_cb);
    lv_anim_set_values(a, 0, 1000);
    lv_anim_set_time(a, 1200); // Slightly faster for more fluid feel
    lv_anim_set_repeat_count(a, LV_ANIM_REPEAT_INFINITE);
}

static lv_obj_t * create_dot(lv_obj_t * parent) {
    lv_obj_t * dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 12, 12); // Slightly larger
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_100, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_shadow_width(dot, 8, 0);
    lv_obj_set_style_shadow_spread(dot, 2, 0);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    return dot;
}

static void build_screen_flow(void) {
    s_scr_flow = lv_obj_create(nullptr);
    apply_screen_style(s_scr_flow);
    lv_obj_add_flag(s_scr_flow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_scr_flow, on_screen_event, LV_EVENT_ALL, nullptr);

    // ── Spoke Lines ──────────────────────────────────────────────────────────
    s_line_solar = create_line(s_scr_flow, CX, CY, NX_SOLAR, NY_SOLAR);
    s_line_home = create_line(s_scr_flow, CX, CY, NX_HOME, NY_HOME);
    s_line_battery = create_line(s_scr_flow, CX, CY, NX_BATTERY, NY_BATTERY);
    s_line_grid = create_line(s_scr_flow, CX, CY, NX_GRID, NY_GRID);

    // ── Center Hub ───────────────────────────────────────────────────────────
    create_node(s_scr_flow, CX, CY, COL_DIM, LV_SYMBOL_CHARGE, nullptr);

    // ── Node circles ─────────────────────────────────────────────────────────
    create_node(s_scr_flow, NX_SOLAR,   NY_SOLAR,   COL_SOLAR,   "PV", &s_lbl_node_solar);
    create_node(s_scr_flow, NX_HOME,    NY_HOME,    COL_HOME,    LV_SYMBOL_HOME, &s_lbl_node_home);
    create_node(s_scr_flow, NX_BATTERY, NY_BATTERY, COL_BATTERY, LV_SYMBOL_BATTERY_FULL, &s_lbl_node_battery);
    create_node(s_scr_flow, NX_GRID,    NY_GRID,    COL_GRID,    LV_SYMBOL_POWER, &s_lbl_node_grid);

    // ── Animated Dots ────────────────────────────────────────────────────────
    lv_obj_t * dot_solar = create_dot(s_scr_flow);
    lv_obj_t * dot_home = create_dot(s_scr_flow);
    lv_obj_t * dot_battery = create_dot(s_scr_flow);
    lv_obj_t * dot_grid = create_dot(s_scr_flow);

    init_anim(&s_anim_solar, &s_path_solar, dot_solar);
    init_anim(&s_anim_home, &s_path_home, dot_home);
    init_anim(&s_anim_battery, &s_path_battery, dot_battery);
    init_anim(&s_anim_grid, &s_path_grid, dot_grid);

    // ── Node kW sub-labels ────────────────────────────────────────────────────
    s_lbl_flow_solar = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_solar, NX_SOLAR - 30, NY_SOLAR + 20);
    lv_obj_set_width(s_lbl_flow_solar, 60);
    lv_obj_set_style_text_align(s_lbl_flow_solar, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_lbl_flow_solar, &s_style_label_small, 0);
    lv_label_set_text(s_lbl_flow_solar, "0.0kW");

    s_lbl_flow_grid = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_grid, NX_GRID - 25, NY_GRID + 20);
    lv_obj_set_width(s_lbl_flow_grid, 50);
    lv_obj_set_style_text_align(s_lbl_flow_grid, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_lbl_flow_grid, &s_style_label_small, 0);
    lv_label_set_text(s_lbl_flow_grid, "0.0kW");

    s_lbl_flow_battery = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_battery, NX_BATTERY - 30, NY_BATTERY + 20);
    lv_obj_set_width(s_lbl_flow_battery, 60);
    lv_obj_set_style_text_align(s_lbl_flow_battery, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(s_lbl_flow_battery, &s_style_label_small, 0);
    lv_label_set_text(s_lbl_flow_battery, "0.0kW");

    s_lbl_flow_home = lv_label_create(s_scr_flow);
    lv_obj_set_pos(s_lbl_flow_home, NX_HOME - 25, NY_HOME + 20);
    lv_obj_set_width(s_lbl_flow_home, 50);
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
    // Row container for alignment
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, 200, 32);
    lv_obj_set_pos(row, 20, y);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    // Icon
    lv_obj_t *icon_lbl = lv_label_create(row);
    lv_label_set_text(icon_lbl, icon);
    lv_obj_align(icon_lbl, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(icon_lbl, icon_color, 0);
    lv_obj_set_style_text_font(icon_lbl, &lv_font_montserrat_24, 0);

    // Value
    lv_obj_t *val = lv_label_create(row);
    lv_label_set_text(val, "0.00kW");
    lv_obj_align(val, LV_ALIGN_LEFT_MID, 50, 0);
    lv_obj_add_style(val, &s_style_val_big, 0);
    *val_label_out = val;
    return val;
}

static void build_screen_current(void) {
    s_scr_current = lv_obj_create(nullptr);
    apply_screen_style(s_scr_current);
    lv_obj_add_flag(s_scr_current, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_scr_current, on_screen_event, LV_EVENT_ALL, nullptr);

    // Title
    lv_obj_t *title = lv_label_create(s_scr_current);
    lv_label_set_text(title, "Live");
    lv_obj_set_pos(title, 0, 14);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, COL_LABEL, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    make_metric_row(s_scr_current, 44,  LV_SYMBOL_CHARGE, COL_SOLAR,   &s_lbl_solar_kw);
    make_metric_row(s_scr_current, 76,  LV_SYMBOL_POWER,  COL_GRID,    &s_lbl_grid_kw);
    make_metric_row(s_scr_current, 108, LV_SYMBOL_BATTERY_FULL, COL_BATTERY, &s_lbl_battery_kw);
    make_metric_row(s_scr_current, 140, LV_SYMBOL_HOME,    COL_HOME,    &s_lbl_home_kw);

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
    lv_label_set_text(s_lbl_soc, "0%");
    lv_obj_align_to(s_lbl_soc, s_arc_soc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(s_lbl_soc, COL_BATTERY, 0);
    lv_obj_set_style_text_font(s_lbl_soc, &lv_font_montserrat_24, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen 2 – Daily Summary
// ─────────────────────────────────────────────────────────────────────────────
static void build_screen_daily(void) {
    s_scr_daily = lv_obj_create(nullptr);
    apply_screen_style(s_scr_daily);
    lv_obj_add_flag(s_scr_daily, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_scr_daily, on_screen_event, LV_EVENT_ALL, nullptr);

    auto make_stat = [](lv_obj_t * parent, int y, const char * icon, const char * label, lv_color_t color, lv_obj_t ** out_val) {
        lv_obj_t * icon_l = lv_label_create(parent);
        lv_label_set_text(icon_l, icon);
        lv_obj_set_style_text_color(icon_l, color, 0);
        lv_obj_set_style_text_font(icon_l, &lv_font_montserrat_24, 0);
        lv_obj_set_pos(icon_l, 30, y);

        lv_obj_t * sub = lv_label_create(parent);
        lv_label_set_text(sub, label);
        lv_obj_set_style_text_color(sub, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
        lv_obj_set_pos(sub, 65, y);

        lv_obj_t * val = lv_label_create(parent);
        lv_label_set_text(val, "0.0");
        lv_obj_set_style_text_color(val, COL_LABEL, 0);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_24, 0);
        lv_obj_set_pos(val, 65, y + 14);
        *out_val = val;
    };

    lv_obj_t *title = lv_label_create(s_scr_daily);
    lv_label_set_text(title, "Daily Stats");
    lv_obj_set_pos(title, 0, 10);
    lv_obj_set_width(title, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

    make_stat(s_scr_daily, 45,  LV_SYMBOL_CHARGE, "Solar Yield", COL_SOLAR,   &s_lbl_daily_pv_total);
    make_stat(s_scr_daily, 85,  LV_SYMBOL_UP,     "Solar Peak",  COL_SOLAR,   &s_lbl_daily_pv_peak);
    make_stat(s_scr_daily, 135, LV_SYMBOL_HOME,   "Home Total",  COL_HOME,    &s_lbl_daily_load_total);
    make_stat(s_scr_daily, 175, LV_SYMBOL_UP,     "Home Peak",   COL_HOME,    &s_lbl_daily_load_peak);
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
    lv_style_set_text_font(&s_style_label_small, &lv_font_montserrat_14);

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

    // ── Update Animations and Lines ──────────────────────────────────────────
    auto manage_flow = [](float kw, lv_obj_t *line, lv_anim_t *anim, anim_path_t *path, 
                          lv_color_t base_col, int src_x, int src_y, int dst_x, int dst_y) {
        if (fabsf(kw) > 0.05f) {
            lv_color_t color = base_col;
            lv_obj_set_style_line_color(line, color, 0);
            lv_obj_set_style_bg_color(path->dot, color, 0);
            lv_obj_set_style_shadow_color(path->dot, color, 0);
            lv_obj_set_style_shadow_width(path->dot, 10, 0);
            lv_obj_clear_flag(path->dot, LV_OBJ_FLAG_HIDDEN);
            
            // Set flow direction
            if (kw > 0.0f) {
                path->start_x = src_x; path->start_y = src_y;
                path->end_x = dst_x; path->end_y = dst_y;
            } else {
                path->start_x = dst_x; path->start_y = dst_y;
                path->end_x = src_x; path->end_y = src_y;
            }
            
            // Start if not running
            if (!lv_anim_get(path, nullptr)) {
                lv_anim_start(anim);
            }
        } else {
            lv_obj_set_style_line_color(line, lv_color_hex(0x222222), 0);
            lv_obj_add_flag(path->dot, LV_OBJ_FLAG_HIDDEN);
            lv_anim_custom_del(anim, nullptr);
        }
    };

    // Solar always flows TO hub (positive)
    manage_flow(data->solar_power_kw, s_line_solar, &s_anim_solar, &s_path_solar, COL_SOLAR, NX_SOLAR, NY_SOLAR, CX, CY);
    
    // Grid: positive = exporting (Hub->Grid), negative = importing (Grid->Hub)
    manage_flow(data->grid_power_kw, s_line_grid, &s_anim_grid, &s_path_grid, COL_GRID, CX, CY, NX_GRID, NY_GRID);
    lv_label_set_text(s_lbl_node_grid, (data->grid_power_kw > 0.05f) ? LV_SYMBOL_UPLOAD : 
                                       (data->grid_power_kw < -0.05f) ? LV_SYMBOL_DOWNLOAD : LV_SYMBOL_POWER);
    
    // Battery: positive = discharging (Battery->Hub), negative = charging (Hub->Battery)
    manage_flow(data->battery_power_kw, s_line_battery, &s_anim_battery, &s_path_battery, COL_BATTERY, NX_BATTERY, NY_BATTERY, CX, CY);
    
    const char * bat_icon = LV_SYMBOL_BATTERY_FULL;
    if (data->battery_soc_pct < 20) bat_icon = LV_SYMBOL_BATTERY_EMPTY;
    else if (data->battery_soc_pct < 40) bat_icon = LV_SYMBOL_BATTERY_1;
    else if (data->battery_soc_pct < 60) bat_icon = LV_SYMBOL_BATTERY_2;
    else if (data->battery_soc_pct < 80) bat_icon = LV_SYMBOL_BATTERY_3;

    if (data->battery_power_kw < -0.05f) { // Charging
        lv_label_set_text(s_lbl_node_battery, LV_SYMBOL_CHARGE);
    } else {
        lv_label_set_text(s_lbl_node_battery, bat_icon);
    }
    
    // Home always draws FROM hub.
    manage_flow(data->home_consumption_kw, s_line_home, &s_anim_home, &s_path_home, COL_HOME, CX, CY, NX_HOME, NY_HOME);

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
