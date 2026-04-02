/**
 * display_hal.cpp
 *
 * Arduino framework version.
 * Binds LovyanGFX (GC9A01) and CST816S into LVGL.
 */

#include "display_hal.h"
#include "config.h"

#include <Arduino.h>
#include <lvgl.h>
#include <CST816S.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// LovyanGFX custom class for GC9A01
// ─────────────────────────────────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;

public:
    LGFX(void) {
        { // Bus
            auto cfg           = _bus_instance.config();
            cfg.spi_host       = SPI2_HOST;
            cfg.spi_mode       = 0;
            cfg.freq_write     = TFT_FREQ_WRITE;
            cfg.freq_read      = TFT_FREQ_READ;
            cfg.spi_3wire      = true;
            cfg.use_lock       = true;
            cfg.dma_channel    = SPI_DMA_CH_AUTO;
            cfg.pin_sclk       = TFT_SCLK;
            cfg.pin_mosi       = TFT_MOSI;
            cfg.pin_miso       = TFT_MISO;
            cfg.pin_dc         = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        { // Panel
            auto cfg              = _panel_instance.config();
            cfg.pin_cs            = TFT_CS;
            cfg.pin_rst           = TFT_RST;
            cfg.pin_busy          = -1;
            cfg.memory_width      = 240;
            cfg.panel_width       = 240;
            cfg.panel_height      = 240;
            cfg.offset_x          = 0;
            cfg.offset_y          = 0;
            cfg.offset_rotation   = 0;
            cfg.dummy_read_pixel  = 8;
            cfg.dummy_read_bits   = 1;
            cfg.readable          = false;
            cfg.invert            = true;
            cfg.rgb_order         = false;
            cfg.dlen_16bit        = false;
            cfg.bus_shared        = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

static LGFX tft;
static CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);
static bool s_display_awake = true;

// ─────────────────────────────────────────────────────────────────────────────
// LVGL draw buffers + driver callbacks
// ─────────────────────────────────────────────────────────────────────────────
#define LV_DRAW_BUF_SIZE (240 * 20)
static lv_color_t lv_buf1[LV_DRAW_BUF_SIZE];
static lv_color_t lv_buf2[LV_DRAW_BUF_SIZE];
static lv_disp_draw_buf_t lv_disp_buf;
static lv_disp_drv_t      lv_disp_drv;
static lv_indev_drv_t     lv_indev_drv;

static void lv_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px_map) {
    if (tft.getStartCount() == 0) {
        tft.endWrite();
    }
    tft.pushImageDMA(area->x1, area->y1,
                     area->x2 - area->x1 + 1,
                     area->y2 - area->y1 + 1,
                     (uint16_t *)&px_map->full);
    lv_disp_flush_ready(drv);
}

static void lv_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    if (touch.available()) {
        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
bool display_init(void) {
    if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);
    }
    
    tft.init();
    tft.initDMA();
    tft.startWrite();
    tft.setRotation(0);
    tft.setBrightness(255);
    tft.fillScreen(TFT_BLACK);

    touch.begin();

    lv_init();
    lv_disp_draw_buf_init(&lv_disp_buf, lv_buf1, lv_buf2, LV_DRAW_BUF_SIZE);

    lv_disp_drv_init(&lv_disp_drv);
    lv_disp_drv.hor_res  = 240;
    lv_disp_drv.ver_res  = 240;
    lv_disp_drv.flush_cb = lv_flush_cb;
    lv_disp_drv.draw_buf = &lv_disp_buf;
    lv_disp_drv_register(&lv_disp_drv);

    lv_indev_drv_init(&lv_indev_drv);
    lv_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    lv_indev_drv.read_cb = lv_touch_cb;
    lv_indev_drv_register(&lv_indev_drv);

    s_display_awake = true;
    Serial.println("[display] LVGL + GC9A01 + CST816S ready");
    return true;
}

void display_wake(void) {
    if (!s_display_awake) {
        tft.wakeup();
        tft.setBrightness(255);
        if (TFT_BL >= 0) {
            digitalWrite(TFT_BL, HIGH);
        }
        s_display_awake = true;
        Serial.println("[display] Woke up");
    }
}

void display_sleep(void) {
    if (s_display_awake) {
        if (TFT_BL >= 0) {
            digitalWrite(TFT_BL, LOW);
        }
        tft.setBrightness(0);
        tft.sleep();
        s_display_awake = false;
        Serial.println("[display] Sleeping");
    }
}

bool display_is_awake(void) {
    return s_display_awake;
}

bool display_touch_available(void) {
    return touch.available();
}

void display_tick(void) {
    lv_tick_inc(5);
    lv_timer_handler();
}
