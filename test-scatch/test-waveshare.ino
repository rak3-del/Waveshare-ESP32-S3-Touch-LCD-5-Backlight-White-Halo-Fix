#include <Arduino.h>
#include <esp_display_panel.hpp>

#include <lvgl.h>
#include "lvgl_v8_port.h"
#include <demos/lv_demos.h>

using namespace esp_panel::drivers;
using namespace esp_panel::board;

Board *board = NULL;
bool displayOff = false;
unsigned long lastTouchTime = 0;
lv_obj_t *statusLabel = NULL;
lv_obj_t *countLabel = NULL;
int wakeCount = 0;

#define TEST_VERSION       "T5.0"
#define SLEEP_TIMEOUT_SEC  15

void setup()
{
    Serial.begin(115200);

    Serial.println("Initializing board");
    board = new Board();
    board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif
    assert(board->begin());

    Serial.println("Initializing LVGL");
    lvgl_port_init(board->getLCD(), board->getTouch());

    Serial.println("Creating UI");
    lvgl_port_lock(-1);

    // Dunkler Hintergrund
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0b101e), 0);

    // Status Label
    statusLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(statusLabel, "Sleep/Wake Test " TEST_VERSION "\nBL via CH422G EXIO2\nSleep in 15 Sek...");
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(statusLabel, lv_color_white(), 0);
    lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, -30);

    // Wake Counter
    countLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(countLabel, "Wake: 0");
    lv_obj_set_style_text_font(countLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(countLabel, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(countLabel, LV_ALIGN_CENTER, 0, 50);

    lvgl_port_unlock();

    lastTouchTime = millis();
    Serial.printf("[OK] Sleep/Wake Test %s - CH422G EXIO2, LVGL bleibt aktiv\n", TEST_VERSION);
}

void loop()
{
    // --- WAKE ---
    if (displayOff) {
        // Touch pollen (LVGL-Task laeuft weiter, also mit Mutex!)
        lvgl_port_lock(-1);
        Touch *tp = board->getTouch();
        TouchPoint pt;
        int cnt = 0;
        if (tp) cnt = tp->readPoints(&pt, 1, 0);
        lvgl_port_unlock();

        if (cnt > 0) {
            Serial.println("[WAKE] Touch erkannt - BL ein");
            Backlight *bl = board->getBacklight();
            if (bl) bl->on();
            displayOff = false;
            lastTouchTime = millis();
            wakeCount++;
            // Wake-Counter updaten
            lvgl_port_lock(-1);
            char buf[32];
            snprintf(buf, sizeof(buf), "Wake: %d", wakeCount);
            lv_label_set_text(countLabel, buf);
            lvgl_port_unlock();
            delay(300);
            Serial.printf("[WAKE] BL ein (Wake #%d)\n", wakeCount);
        }
        delay(50);
        return;
    }

    // --- Touch tracken (mit Mutex weil LVGL-Task auch Touch pollt) ---
    lvgl_port_lock(-1);
    Touch *tp = board->getTouch();
    TouchPoint pt;
    if (tp && tp->readPoints(&pt, 1, 0) > 0) {
        lastTouchTime = millis();
    }
    lvgl_port_unlock();

    // --- SLEEP ---
    if (millis() - lastTouchTime > (SLEEP_TIMEOUT_SEC * 1000UL)) {
        Serial.println("[SLEEP] Timeout - BL aus");
        Backlight *bl = board->getBacklight();
        if (bl) bl->off();
        displayOff = true;
        // LVGL + DMA laufen weiter
        Serial.println("[SLEEP] BL aus - LVGL+DMA laufen weiter");
    }

    delay(10);
}
