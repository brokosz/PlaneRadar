/*
 * PlaneRadar — overhead flight display for JC4827W543 (ESP32-S3 + 4.3" TFT)
 *
 * Hardware: same board as Halo-F1 (https://halof1.com)
 * Display:  480×272 capacitive touch TFT (GT911 touch controller)
 *
 * ── Required libraries (install via Arduino Library Manager) ─────────────────
 *   ArduinoJson       >= 7.x       (Benoit Blanchon)
 *   WiFiManager       >= 2.0.17    (tablatronix/tzapu)
 *   LVGL              >= 9.3.0     (lvgl)
 *   bb_spi_lcd        >= 2.7.1     (bitbank2)
 *   bb_captouch       latest       (bitbank2)
 *
 * ── Files to copy from the Halo-F1 repo ─────────────────────────────────────
 *   https://github.com/FabioRoss/Halo-F1
 *     lv_conf.h          → copy into this sketch folder
 *     lv_bb_spi_lcd.h    → copy into this sketch folder
 *     lv_bb_spi_lcd.cpp  → copy into this sketch folder
 *     touchscreen.h      → copy into this sketch folder
 *
 * ── Board settings in Arduino IDE ───────────────────────────────────────────
 *   Board:         ESP32S3 Dev Module
 *   Flash:         16MB  (128Mb)
 *   PSRAM:         OPI PSRAM
 *   Partition:     Huge APP (3MB No OTA / 1MB SPIFFS)
 *   USB Mode:      Hardware CDC and JTAG
 */

// ── All #defines must come before their respective #includes ─────────────────
// (Matches the pin config at the top of F1Halo.ino exactly)

// Display type for bb_spi_lcd
#define DISPLAY_TYPE     DISPLAY_CYD_543

// Touch — capacitive (GT911) pins
#define TOUCH_CAPACITIVE        // select GT911 path in touchscreen.h
#define TOUCH_SDA        8
#define TOUCH_SCL        4
#define TOUCH_INT        3
#define TOUCH_RST        (-1)
// Resistive touch SPI pins (unused when TOUCH_CAPACITIVE is set, but
// touchscreen.h may reference them at compile time)
#define TOUCH_MOSI       11
#define TOUCH_MISO       13
#define TOUCH_CLK        12
#define TOUCH_CS         38
// Touch calibration bounds
#define TOUCH_MIN_X      1
#define TOUCH_MAX_X      480
#define TOUCH_MIN_Y      1
#define TOUCH_MAX_Y      272

// Native display resolution (portrait). LVGL rotates to landscape at runtime.
#define SCREEN_WIDTH   272
#define SCREEN_HEIGHT  480
#define SCR_W          480    // logical landscape width  after rotation
#define SCR_H          272    // logical landscape height after rotation

// ── Includes ─────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <lvgl.h>
#include "lv_bb_spi_lcd.h"
#include "touchscreen.h"
#include "settings.h"
#include "location.h"
#include "flight_data.h"
#include "ui.h"

// ────────────────────────────────────────────────────────────────────────────
// Globals
// ────────────────────────────────────────────────────────────────────────────
static unsigned long last_fetch_at = 0;

// Deferred location re-resolve flag (set when ZIP changes in settings)
// The main loop checks this and re-runs location_resolve() + flights_fetch()
static bool need_relocate = false;

// ────────────────────────────────────────────────────────────────────────────
// WiFiManager setup — dark-themed captive portal with ZIP code field
// ────────────────────────────────────────────────────────────────────────────
static void wifi_setup() {
    WiFiManager wm;
    wm.setClass("invert");          // dark theme
    wm.setTitle("PlaneRadar");
    wm.setConfigPortalTimeout(180); // 3 min, then try saved creds

    // Custom ZIP code field shown in the captive portal
    WiFiManagerParameter zip_param(
        "zip", "ZIP Code (optional — leave blank for auto-location)",
        settings.zip_code, 10);
    wm.addParameter(&zip_param);

    // Called when user saves config through the portal
    wm.setSaveParamsCallback([&]() {
        const char *zip = zip_param.getValue();
        if (zip && strlen(zip) >= 5) {
            strncpy(settings.zip_code, zip, sizeof(settings.zip_code) - 1);
            settings.location_mode = LOC_ZIP;
        } else {
            settings.location_mode = LOC_AUTO;
            settings.zip_code[0] = '\0';
        }
        settings_save();
    });

    ui_show_wifi_screen();

    bool connected = wm.autoConnect("PlaneRadar");
    if (!connected) {
        Serial.println("[WIFI] Could not connect — restarting");
        delay(3000);
        ESP.restart();
    }

    Serial.printf("[WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
}

// ────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== PlaneRadar booting ===");

    // ── Load saved settings FIRST — orientation must be known before display ─
    settings_load();

    // ── LVGL init ──────────────────────────────────────────────────────────
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // ── Display init ───────────────────────────────────────────────────────
    lv_bb_spi_lcd_create(DISPLAY_TYPE);

    // Physical panel is 272×480 portrait-native.
    // Landscape mode rotates 90° so LVGL logical becomes 480×272.
    // Portrait mode uses no rotation — logical stays 272×480.
    if (settings.orientation == ORI_LANDSCAPE) {
        lv_display_set_rotation(lv_display_get_default(), LV_DISPLAY_ROTATION_90);
    }

    // ── Touch init ─────────────────────────────────────────────────────────
    // touchscreen.h declares a global BBCapTouch bbct — init it directly
    bbct.init(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read);

    // ── Build UI (shows WiFi screen) ───────────────────────────────────────
    ui_create();
    lv_timer_handler(); // render once before blocking in wifi_setup

    // ── WiFi ───────────────────────────────────────────────────────────────
    wifi_setup();

    // ── Location ───────────────────────────────────────────────────────────
    ui_show_status("Locating...");
    lv_timer_handler();
    location_resolve();

    // ── Switch to main screen ──────────────────────────────────────────────
    ui_show_main_screen();
    lv_timer_handler();

    // ── First flight fetch ─────────────────────────────────────────────────
    ui_show_status("Fetching flights...");
    lv_timer_handler();
    flights_fetch();
    ui_update_flights();
    ui_show_status(nullptr);

    last_fetch_at = millis();
    Serial.println("[BOOT] Ready.");
}

// ────────────────────────────────────────────────────────────────────────────
void loop() {
    lv_timer_handler();
    delay(5);

    // ── Handle deferred re-locate (triggered from settings ZIP change) ──────
    if (need_relocate) {
        need_relocate = false;
        ui_show_status("Updating location...");
        lv_timer_handler();
        location_resolve();
        ui_show_status("Fetching flights...");
        lv_timer_handler();
        flights_fetch();
        ui_update_flights();
        ui_show_status(nullptr);
        last_fetch_at = millis();
        return;
    }

    // ── Periodic flight refresh ─────────────────────────────────────────────
    unsigned long interval_ms = (unsigned long)settings.update_interval_s * 1000UL;
    if (millis() - last_fetch_at >= interval_ms) {
        // Re-resolve location if using auto-IP (cheap: just update city label)
        // Only re-resolve lat/lon if we don't have a fix yet
        if (settings.latitude == 0.0f && settings.longitude == 0.0f) {
            location_resolve();
        }
        flights_fetch();
        ui_update_flights();
        last_fetch_at = millis();
    }

    // ── Update "X ago" label every second ───────────────────────────────────
    static unsigned long last_header_ms = 0;
    if (millis() - last_header_ms >= 1000) {
        last_header_ms = millis();
        ui_update_header();
    }
}

// ── Called from ui.h settings callback when ZIP is confirmed via keyboard ────
// Expose this so ui.h can request a re-locate without needing a full fetch here
void request_relocate() {
    need_relocate = true;
}
