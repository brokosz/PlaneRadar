#pragma once
#include <lvgl.h>
#include "settings.h"
#include "flight_data.h"

// ── Colours ──────────────────────────────────────────────────────────────────
#define C_BG        lv_color_hex(0x080808)
#define C_SURFACE   lv_color_hex(0x111111)
#define C_SURFACE2  lv_color_hex(0x1a1a1a)
#define C_ACCENT    lv_color_hex(0x00e676)   // radar green
#define C_TEXT      lv_color_hex(0xdedede)
#define C_DIM       lv_color_hex(0x666666)
#define C_SEP       lv_color_hex(0x222222)
#define C_DIST      lv_color_hex(0x4fc3f7)   // cyan
#define C_SPD       lv_color_hex(0xffb74d)   // amber

// ── Fonts (available from lv_conf.h copied from Halo-F1) ─────────────────────
LV_FONT_DECLARE(lv_font_montserrat_38)
LV_FONT_DECLARE(lv_font_montserrat_20)
LV_FONT_DECLARE(lv_font_montserrat_16)
LV_FONT_DECLARE(lv_font_montserrat_14)
LV_FONT_DECLARE(lv_font_montserrat_12)

// Defined in PlaneRadar.ino — triggers location re-resolve on next loop tick
extern void request_relocate();

// Forward declarations for screen builders (defined later in this file)
static void build_main_screen();
static void build_settings_screen();
void ui_show_main_screen();
void ui_rebuild_screens();

#define FONT_TITLE  &lv_font_montserrat_20
#define FONT_HDR    &lv_font_montserrat_14
#define FONT_DATA   &lv_font_montserrat_14
#define FONT_SMALL  &lv_font_montserrat_12

// ── Screen / widget handles ──────────────────────────────────────────────────
#define MAX_ROWS 20

static lv_obj_t *scr_wifi     = nullptr;
static lv_obj_t *scr_main     = nullptr;
static lv_obj_t *scr_settings = nullptr;

// Main screen
static lv_obj_t *lbl_city    = nullptr;
static lv_obj_t *lbl_updated = nullptr;
static lv_obj_t *lbl_count   = nullptr;

// Flight list rows (pre-allocated)
struct FlightRow {
    lv_obj_t *row;
    lv_obj_t *lbl_flt;    // flight number
    lv_obj_t *lbl_route;  // ORG→DST
    lv_obj_t *lbl_type;   // aircraft type
    lv_obj_t *lbl_alt;    // altitude
    lv_obj_t *lbl_spd;    // speed
    lv_obj_t *lbl_trend;  // arrow symbol
    lv_obj_t *lbl_dist;   // distance
};
static FlightRow g_rows[MAX_ROWS];

// Settings screen widgets
static lv_obj_t *btn_loc_auto = nullptr;
static lv_obj_t *btn_loc_zip  = nullptr;
static lv_obj_t *ta_zip       = nullptr;
static lv_obj_t *kb           = nullptr;
static lv_obj_t *lbl_loc_info = nullptr;
static lv_obj_t *btn_r[4]     = {};
static lv_obj_t *btn_i[4]     = {};
static lv_obj_t *btn_ori[2]   = {};   // [0]=landscape, [1]=portrait
static lv_obj_t *btn_tab[3]        = {};  // [0]=ALL, [1]=AIRLINES, [2]=SETTINGS
static const int RADIUS_OPTS[]   = {25, 50, 100, 200};
static const int INTERVAL_OPTS[] = {30, 60, 120, 300};

// Status overlay
static lv_obj_t *ov_status  = nullptr;
static lv_obj_t *lbl_status = nullptr;

// Detail card (flight info popup)
static lv_obj_t *det_overlay = nullptr;
static lv_obj_t *det_flt     = nullptr;
static lv_obj_t *det_sub     = nullptr;
static lv_obj_t *det_alt     = nullptr;
static lv_obj_t *det_spd     = nullptr;
static lv_obj_t *det_hdg     = nullptr;
static lv_obj_t *det_dist    = nullptr;
static lv_obj_t *det_vs      = nullptr;

// ── Internal helpers ─────────────────────────────────────────────────────────
static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                             lv_color_t color, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(l, align, 0);
    return l;
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text,
                           lv_event_cb_t cb, void *user_data = nullptr) {
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_style_bg_color(b,     C_SURFACE2, 0);
    lv_obj_set_style_bg_color(b,     C_ACCENT,   LV_STATE_CHECKED);
    lv_obj_set_style_border_color(b, C_SEP, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, FONT_SMALL, 0);
    lv_obj_set_style_text_color(l, C_TEXT, 0);
    lv_obj_set_style_text_color(l, C_BG, LV_STATE_CHECKED);
    lv_obj_center(l);
    return b;
}

// ── WiFi screen ───────────────────────────────────────────────────────────────
void ui_show_wifi_screen() {
    lv_screen_load(scr_wifi);
}

// ── Status overlay ────────────────────────────────────────────────────────────
void ui_show_status(const char *msg) {
    if (!ov_status) return;
    if (msg) {
        lv_label_set_text(lbl_status, msg);
        lv_obj_clear_flag(ov_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ov_status, LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_handler();
}

// ── "X ago" string helper ─────────────────────────────────────────────────────
static void fmt_ago(char *buf, size_t len) {
    if (g_last_fetch_ms == 0) { snprintf(buf, len, "never"); return; }
    unsigned long s = (millis() - g_last_fetch_ms) / 1000;
    if (s < 60)       snprintf(buf, len, "%lus ago", s);
    else if (s < 3600) snprintf(buf, len, "%lum ago", s / 60);
    else               snprintf(buf, len, ">1h ago");
}

// ── Update header ─────────────────────────────────────────────────────────────
void ui_update_header() {
    char buf[40];
    if (lbl_city) lv_label_set_text(lbl_city, settings.city_name);
    if (lbl_updated) {
        fmt_ago(buf, sizeof(buf));
        lv_label_set_text(lbl_updated, buf);
    }
    if (lbl_count) {
        snprintf(buf, sizeof(buf), "%d flights", g_flight_count);
        lv_label_set_text(lbl_count, buf);
    }
}

// ── Update flight list rows ───────────────────────────────────────────────────
void ui_update_flights() {
    for (int i = 0; i < MAX_ROWS; i++) {
        if (!g_rows[i].row) continue;
        if (i < g_flight_count) {
            const Flight &f = g_flights[i];
            char alt[16], spd[12], dist[12];

            if (f.altitude_ft > 0)
                snprintf(alt,  sizeof(alt),  "%d ft", f.altitude_ft);
            else
                snprintf(alt,  sizeof(alt),  "---");

            snprintf(spd,  sizeof(spd),  "%d kts", f.speed_kts);
            snprintf(dist, sizeof(dist), "%.1f mi", f.distance_mi);

            lv_label_set_text(g_rows[i].lbl_flt,   f.flight_num);
            // Show registration in route column — ADS-B has no route data
            lv_label_set_text(g_rows[i].lbl_route,
                strcmp(f.registration, "---") != 0 ? f.registration : "");
            lv_label_set_text(g_rows[i].lbl_type,   f.aircraft[0] ? f.aircraft : "---");
            lv_label_set_text(g_rows[i].lbl_alt,    alt);
            lv_label_set_text(g_rows[i].lbl_spd,    spd);
            lv_label_set_text(g_rows[i].lbl_trend,  trend_arrow(f));
            lv_label_set_text(g_rows[i].lbl_dist,   dist);
            lv_obj_set_style_text_color(g_rows[i].lbl_trend,
                lv_color_hex(trend_color(f)), 0);

            // Alternate row background
            lv_obj_set_style_bg_color(g_rows[i].row,
                (i % 2 == 0) ? C_SURFACE : lv_color_hex(0x0d0d0d), 0);

            lv_obj_clear_flag(g_rows[i].row, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_rows[i].row, LV_OBJ_FLAG_HIDDEN);
        }
    }

    ui_update_header();

    // Show empty-state if no flights
    // (rows already hidden — the "no flights" label handles this)
    if (lbl_count) {
        char buf[40];
        if (g_flight_count == 0)
            snprintf(buf, sizeof(buf), "No flights in range");
        else
            snprintf(buf, sizeof(buf), "%d aircraft overhead", g_flight_count);
        lv_label_set_text(lbl_count, buf);
    }
}

// ── Settings callbacks ────────────────────────────────────────────────────────
static void cb_close_settings(lv_event_t *e) {
    lv_screen_load_anim(scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, false);
}

static void cb_open_settings(lv_event_t *e) {
    // Refresh UI to reflect current settings before opening
    if (btn_loc_auto) lv_obj_set_state(btn_loc_auto,
        LV_STATE_CHECKED, settings.location_mode == LOC_AUTO);
    if (btn_loc_zip) lv_obj_set_state(btn_loc_zip,
        LV_STATE_CHECKED, settings.location_mode == LOC_ZIP);
    if (ta_zip && strlen(settings.zip_code) > 0)
        lv_textarea_set_text(ta_zip, settings.zip_code);
    if (lbl_loc_info) {
        char buf[80];
        snprintf(buf, sizeof(buf), "%.4f, %.4f  \xe2\x80\x94  %s",
            settings.latitude, settings.longitude, settings.city_name);
        lv_label_set_text(lbl_loc_info, buf);
    }
    for (int i = 0; i < 4; i++) {
        if (btn_r[i]) lv_obj_set_state(btn_r[i],
            LV_STATE_CHECKED, RADIUS_OPTS[i] == settings.radius_miles);
        if (btn_i[i]) lv_obj_set_state(btn_i[i],
            LV_STATE_CHECKED, INTERVAL_OPTS[i] == settings.update_interval_s);
    }
    if (btn_ori[0]) lv_obj_set_state(btn_ori[0], LV_STATE_CHECKED, settings.orientation == ORI_LANDSCAPE);
    if (btn_ori[1]) lv_obj_set_state(btn_ori[1], LV_STATE_CHECKED, settings.orientation == ORI_PORTRAIT);
    if (scr_settings)
        lv_screen_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}

static void cb_loc_auto(lv_event_t *e) {
    settings.location_mode = LOC_AUTO;
    if (btn_loc_auto) lv_obj_add_state(btn_loc_auto, LV_STATE_CHECKED);
    if (btn_loc_zip)  lv_obj_clear_state(btn_loc_zip, LV_STATE_CHECKED);
    settings_save();
}

static void cb_loc_zip(lv_event_t *e) {
    settings.location_mode = LOC_ZIP;
    if (btn_loc_zip)  lv_obj_add_state(btn_loc_zip, LV_STATE_CHECKED);
    if (btn_loc_auto) lv_obj_clear_state(btn_loc_auto, LV_STATE_CHECKED);
    if (kb) lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    settings_save();
}

static void cb_ta_zip(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED && kb)
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

static void cb_kb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (code == LV_EVENT_READY && ta_zip) {
            const char *zip = lv_textarea_get_text(ta_zip);
            strncpy(settings.zip_code, zip, sizeof(settings.zip_code) - 1);
            settings.location_mode = LOC_ZIP;
            settings_save();
            request_relocate();
        }
        if (kb) lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void cb_radius(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    settings.radius_miles = RADIUS_OPTS[idx];
    for (int i = 0; i < 4; i++)
        if (btn_r[i]) lv_obj_set_state(btn_r[i], LV_STATE_CHECKED, i == idx);
    settings_save();
}

static void cb_interval(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    settings.update_interval_s = INTERVAL_OPTS[idx];
    for (int i = 0; i < 4; i++)
        if (btn_i[i]) lv_obj_set_state(btn_i[i], LV_STATE_CHECKED, i == idx);
    settings_save();
}

static void cb_orientation(lv_event_t *e) {
    Orientation ori = (Orientation)(intptr_t)lv_event_get_user_data(e);
    if (ori == settings.orientation) return;   // no change
    settings.orientation = ori;
    settings_save();
    // Hardware rotation via bb_spi_lcd — reliable in partial rendering mode.
    // rot 1 = landscape (480×272),  rot 3 = portrait (272×480)
    lv_bb_spi_lcd_set_rotation(lv_display_get_default(),
        ori == ORI_LANDSCAPE ? 1 : 3);
    // Rebuild all screens for the new dimensions
    lv_obj_del(scr_main);     scr_main     = nullptr;
    lv_obj_del(scr_settings); scr_settings = nullptr;
    // Clear row handles so build_main_screen re-creates them
    for (int i = 0; i < MAX_ROWS; i++) g_rows[i] = {};
    btn_loc_auto = btn_loc_zip = ta_zip = kb = lbl_loc_info = nullptr;
    for (int i = 0; i < 4; i++) { btn_r[i] = btn_i[i] = nullptr; }
    btn_ori[0] = btn_ori[1] = nullptr;
    btn_tab[0] = btn_tab[1] = btn_tab[2] = nullptr;
    det_overlay = det_flt = det_sub = nullptr;
    det_alt = det_spd = det_hdg = det_dist = det_vs = nullptr;
    ov_status = lbl_status = nullptr;
    lbl_city = lbl_updated = lbl_count = nullptr;
    build_main_screen();
    build_settings_screen();
    ui_show_main_screen();
}

static void cb_tab(lv_event_t *e) {
    int tab = (int)(intptr_t)lv_event_get_user_data(e);
    if (tab == 2) { cb_open_settings(e); return; }
    bool commercial = (tab == 1);
    if (settings.commercial_only == commercial) return;
    settings.commercial_only = commercial;
    settings_save();
    for (int i = 0; i < 3; i++)
        if (btn_tab[i]) lv_obj_set_state(btn_tab[i], LV_STATE_CHECKED, i == tab);
    ui_show_status(commercial ? "Airlines only..." : "All traffic...");
    flights_fetch();
    ui_update_flights();
    ui_show_status(nullptr);
}

// ── Detail card callbacks ─────────────────────────────────────────────────────
static void cb_close_detail(lv_event_t *) {
    if (det_overlay) lv_obj_add_flag(det_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void cb_row_click(lv_event_t *e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (!det_overlay || idx >= g_flight_count) return;
    const Flight &f = g_flights[idx];
    char buf[64];

    lv_label_set_text(det_flt, f.flight_num);

    // "B737  N12345" — type and registration
    snprintf(buf, sizeof(buf), "%s   %s",
        strcmp(f.aircraft,     "---") != 0 ? f.aircraft     : "?",
        strcmp(f.registration, "---") != 0 ? f.registration : "");
    lv_label_set_text(det_sub, buf);

    // altitude with trend indicator colour
    snprintf(buf, sizeof(buf), "%d ft  %s", f.altitude_ft, trend_arrow(f));
    lv_label_set_text(det_alt, buf);
    lv_obj_set_style_text_color(det_alt, lv_color_hex(trend_color(f)), 0);

    snprintf(buf, sizeof(buf), "%d kts", f.speed_kts);
    lv_label_set_text(det_spd, buf);

    snprintf(buf, sizeof(buf), "%d deg", f.heading);
    lv_label_set_text(det_hdg, buf);

    snprintf(buf, sizeof(buf), "%.1f mi", f.distance_mi);
    lv_label_set_text(det_dist, buf);

    snprintf(buf, sizeof(buf), "%+d fpm", f.vert_speed_fpm);
    lv_label_set_text(det_vs, buf);

    lv_obj_clear_flag(det_overlay, LV_OBJ_FLAG_HIDDEN);
}

// ── Row factory — adapts to landscape (480×33) or portrait (272×48, 2 lines) ──
static FlightRow make_row(lv_obj_t *parent, int idx) {
    bool port = (settings.orientation == ORI_PORTRAIT);
    FlightRow r;
    lv_color_t row_bg = (idx % 2 == 0) ? C_SURFACE : lv_color_hex(0x0d0d0d);

    r.row = lv_obj_create(parent);
    lv_obj_set_size(r.row, port ? 272 : 480, port ? 48 : 33);
    lv_obj_set_style_bg_color(r.row, row_bg, 0);
    lv_obj_set_style_bg_opa(r.row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(r.row, 0, 0);
    lv_obj_set_style_radius(r.row, 0, 0);
    lv_obj_set_style_pad_all(r.row, 0, 0);
    lv_obj_clear_flag(r.row, LV_OBJ_FLAG_SCROLLABLE);

    if (!port) {
        // ── Landscape: single line, 7 columns ─────────────────────────────
        // FLT(80) ROUTE(90) TYPE(52) ALT(88) SPD(72) TREND(28) DIST(64) = 474
        auto col = [&](int x, int w, lv_color_t c, lv_text_align_t a) -> lv_obj_t * {
            lv_obj_t *l = make_label(r.row, FONT_DATA, c, a);
            lv_obj_set_pos(l, x + 4, 9);
            lv_obj_set_width(l, w - 6);
            return l;
        };
        r.lbl_flt   = col(0,   80, C_ACCENT,               LV_TEXT_ALIGN_LEFT);
        r.lbl_route = col(80,  90, C_TEXT,                  LV_TEXT_ALIGN_LEFT);
        r.lbl_type  = col(170, 52, C_DIM,                   LV_TEXT_ALIGN_LEFT);
        r.lbl_alt   = col(222, 88, lv_color_hex(0x66bb6a),  LV_TEXT_ALIGN_RIGHT);
        r.lbl_spd   = col(310, 72, C_SPD,                   LV_TEXT_ALIGN_RIGHT);
        r.lbl_trend = col(382, 28, C_DIM,                   LV_TEXT_ALIGN_CENTER);
        r.lbl_dist  = col(410, 64, C_DIST,                  LV_TEXT_ALIGN_RIGHT);
    } else {
        // ── Portrait: two lines, width=272 ────────────────────────────────
        // Line 1 (y=6):  FLIGHT(140)              DIST(right, 70)
        // Line 2 (y=28): ALT(110)  SPD(84)  TREND(22)
        auto col = [&](int x, int w, int y, lv_color_t c, lv_text_align_t a) -> lv_obj_t * {
            lv_obj_t *l = make_label(r.row, FONT_DATA, c, a);
            lv_obj_set_pos(l, x + 4, y);
            lv_obj_set_width(l, w - 6);
            return l;
        };
        r.lbl_flt   = col(0,   140,  6, C_ACCENT,               LV_TEXT_ALIGN_LEFT);
        r.lbl_dist  = col(196,  70,  6, C_DIST,                  LV_TEXT_ALIGN_RIGHT);
        r.lbl_alt   = col(0,   110, 28, lv_color_hex(0x66bb6a),  LV_TEXT_ALIGN_LEFT);
        r.lbl_spd   = col(110,  84, 28, C_SPD,                   LV_TEXT_ALIGN_LEFT);
        r.lbl_trend = col(194,  26, 28, C_DIM,                   LV_TEXT_ALIGN_CENTER);
        // Not shown in portrait rows — kept off-screen so ui_update_flights doesn't crash
        r.lbl_route = make_label(r.row, FONT_DATA, C_TEXT, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(r.lbl_route, -500, 0);
        lv_obj_set_width(r.lbl_route, 1);
        r.lbl_type  = make_label(r.row, FONT_DATA, C_DIM, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(r.lbl_type,  -500, 0);
        lv_obj_set_width(r.lbl_type, 1);
    }
    return r;
}

// ── Main screen build — adapts to landscape (480×272) or portrait (272×480) ──
static void build_main_screen() {
    bool port  = (settings.orientation == ORI_PORTRAIT);
    int  scr_w = port ? 272 : 480;
    int  scr_h = port ? 480 : 272;

    scr_main = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_main, C_BG, 0);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);

    // ── Header bar ───────────────────────────────────────────────────────────
    lv_obj_t *hdr = lv_obj_create(scr_main);
    lv_obj_set_size(hdr, scr_w, 40);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(hdr, LV_OBJ_FLAG_CLICKABLE);   // required for LV_EVENT_CLICKED to fire
    // Double-tap anywhere on the header opens settings (fallback if gear is hard to reach)
    lv_obj_add_event_cb(hdr, [](lv_event_t *e) {
        static uint32_t last_tap = 0;
        uint32_t now = lv_tick_get();
        if (now - last_tap < 400) cb_open_settings(e);
        last_tap = now;
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "OVERHEAD");
    lv_obj_set_style_text_font(lbl_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_title, C_ACCENT, 0);
    lv_obj_set_pos(lbl_title, 8, 10);

    if (!port) {
        // Landscape: city, updated, count all fit in 480px header (no gear button)
        lbl_city = make_label(hdr, FONT_SMALL, C_DIM, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(lbl_city, 140, 14);
        lv_obj_set_width(lbl_city, 110);
        lv_label_set_text(lbl_city, settings.city_name);

        lbl_updated = make_label(hdr, FONT_SMALL, C_DIM, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(lbl_updated, 258, 14);
        lv_obj_set_width(lbl_updated, 86);
        lv_label_set_text(lbl_updated, "---");

        lbl_count = make_label(hdr, FONT_SMALL, C_DIM, LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_pos(lbl_count, 350, 14);
        lv_obj_set_width(lbl_count, 126);
        lv_label_set_text(lbl_count, "");
    } else {
        // Portrait: count in header; city/updated not shown on main screen
        lbl_count = make_label(hdr, FONT_SMALL, C_DIM, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(lbl_count, 134, 14);
        lv_obj_set_width(lbl_count, 134);
        lv_label_set_text(lbl_count, "");
    }

    // ── Column headers ────────────────────────────────────────────────────────
    int list_y;
    {
        int hdr_y = 40, hdr_h = 18;
        lv_obj_t *col_hdr = lv_obj_create(scr_main);
        lv_obj_set_size(col_hdr, scr_w, hdr_h);
        lv_obj_set_pos(col_hdr, 0, hdr_y);
        lv_obj_set_style_bg_color(col_hdr, lv_color_hex(0x0d0d0d), 0);
        lv_obj_set_style_bg_opa(col_hdr, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(col_hdr, 0, 0);
        lv_obj_set_style_radius(col_hdr, 0, 0);
        lv_obj_set_style_pad_all(col_hdr, 0, 0);
        lv_obj_clear_flag(col_hdr, LV_OBJ_FLAG_SCROLLABLE);
        auto ch = [&](int x, int w, const char *text, lv_text_align_t align) {
            lv_obj_t *l = make_label(col_hdr, FONT_SMALL, C_DIM, align);
            lv_obj_set_pos(l, x + 4, 3);
            lv_obj_set_width(l, w - 6);
            lv_label_set_text(l, text);
        };
        if (!port) {
            ch(0,   80, "FLIGHT",   LV_TEXT_ALIGN_LEFT);
            ch(80,  90, "REG",      LV_TEXT_ALIGN_LEFT);
            ch(170, 52, "TYPE",     LV_TEXT_ALIGN_LEFT);
            ch(222, 88, "ALTITUDE", LV_TEXT_ALIGN_RIGHT);
            ch(310, 72, "SPEED",    LV_TEXT_ALIGN_RIGHT);
            ch(382, 28, "",         LV_TEXT_ALIGN_CENTER);
            ch(410, 64, "DIST",     LV_TEXT_ALIGN_RIGHT);
        } else {
            ch(0,   140, "FLIGHT", LV_TEXT_ALIGN_LEFT);
            ch(196,  70, "DIST",   LV_TEXT_ALIGN_RIGHT);
        }
        lv_obj_t *sep = lv_obj_create(scr_main);
        lv_obj_set_size(sep, scr_w, 1);
        lv_obj_set_pos(sep, 0, hdr_y + hdr_h);
        lv_obj_set_style_bg_color(sep, C_SEP, 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        list_y = hdr_y + hdr_h + 1;  // landscape: 59, portrait: 59
    }

    // ── Scrollable flight list ────────────────────────────────────────────────
    const int TAB_H = 36;
    int list_h = scr_h - list_y - TAB_H;
    lv_obj_t *list = lv_obj_create(scr_main);
    lv_obj_set_pos(list, 0, list_y);
    lv_obj_set_size(list, scr_w, list_h);
    lv_obj_set_style_bg_color(list, C_BG, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < MAX_ROWS; i++) {
        g_rows[i] = make_row(list, i);
        lv_obj_add_flag(g_rows[i].row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_rows[i].row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_rows[i].row, cb_row_click, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    // ── Bottom tab bar ────────────────────────────────────────────────────────
    {
        lv_obj_t *tabs = lv_obj_create(scr_main);
        lv_obj_set_size(tabs, scr_w, TAB_H);
        lv_obj_set_pos(tabs, 0, scr_h - TAB_H);
        lv_obj_set_style_bg_color(tabs, C_SURFACE, 0);
        lv_obj_set_style_bg_opa(tabs, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tabs, 0, 0);
        lv_obj_set_style_radius(tabs, 0, 0);
        lv_obj_set_style_pad_all(tabs, 0, 0);
        lv_obj_set_style_pad_column(tabs, 0, 0);
        lv_obj_clear_flag(tabs, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);

        const char *tab_labels[] = { "ALL", "AIRLINES", "SETTINGS" };
        int tab_w = scr_w / 3;
        for (int i = 0; i < 3; i++) {
            btn_tab[i] = lv_btn_create(tabs);
            lv_obj_set_size(btn_tab[i], tab_w, TAB_H);
            lv_obj_set_style_bg_color(btn_tab[i], C_SURFACE,  0);
            lv_obj_set_style_bg_color(btn_tab[i], C_SURFACE2, LV_STATE_CHECKED);
            lv_obj_set_style_border_width(btn_tab[i], 0, 0);
            lv_obj_set_style_border_width(btn_tab[i], 2, LV_STATE_CHECKED);
            lv_obj_set_style_border_side(btn_tab[i],
                (lv_border_side_t)LV_BORDER_SIDE_TOP, LV_STATE_CHECKED);
            lv_obj_set_style_border_color(btn_tab[i], C_ACCENT, LV_STATE_CHECKED);
            lv_obj_set_style_radius(btn_tab[i], 0, 0);
            lv_obj_set_style_pad_all(btn_tab[i], 0, 0);
            lv_obj_add_flag(btn_tab[i], LV_OBJ_FLAG_CHECKABLE);
            lv_obj_add_event_cb(btn_tab[i], cb_tab, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            lv_obj_t *lbl = lv_label_create(btn_tab[i]);
            lv_label_set_text(lbl, tab_labels[i]);
            lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
            lv_obj_set_style_text_color(lbl, C_DIM, 0);
            lv_obj_set_style_text_color(lbl, C_ACCENT, LV_STATE_CHECKED);
            lv_obj_center(lbl);
            // Mark active tab based on current filter
            bool active = (i == (settings.commercial_only ? 1 : 0));
            lv_obj_set_state(btn_tab[i], LV_STATE_CHECKED, active);
        }
    }

    // ── Detail card overlay ───────────────────────────────────────────────────
    int cw = scr_w - 32;   // card width (16px margin each side)
    int key_w = 62, val_w = cw - 24 - key_w - 4;  // inside card: pad=12 each side

    det_overlay = lv_obj_create(scr_main);
    lv_obj_set_size(det_overlay, scr_w, scr_h);
    lv_obj_set_pos(det_overlay, 0, 0);
    lv_obj_set_style_bg_color(det_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(det_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(det_overlay, 0, 0);
    lv_obj_set_style_radius(det_overlay, 0, 0);
    lv_obj_clear_flag(det_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(det_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(det_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(det_overlay, cb_close_detail, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *card = lv_obj_create(det_overlay);
    lv_obj_set_width(card, cw);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, C_ACCENT, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 5, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    // Stop clicks on the card from bubbling to the overlay (which would close it)
    lv_obj_add_event_cb(card, [](lv_event_t *e){ lv_event_stop_bubbling(e); },
                        LV_EVENT_CLICKED, nullptr);

    // Flight number (large, accent)
    det_flt = lv_label_create(card);
    lv_label_set_text(det_flt, "---");
    lv_obj_set_style_text_font(det_flt, FONT_TITLE, 0);
    lv_obj_set_style_text_color(det_flt, C_ACCENT, 0);

    // Aircraft type · registration
    det_sub = lv_label_create(card);
    lv_label_set_text(det_sub, "");
    lv_obj_set_style_text_font(det_sub, FONT_SMALL, 0);
    lv_obj_set_style_text_color(det_sub, C_DIM, 0);

    // Separator
    lv_obj_t *det_sep = lv_obj_create(card);
    lv_obj_set_size(det_sep, cw - 24, 1);
    lv_obj_set_style_bg_color(det_sep, C_SEP, 0);
    lv_obj_set_style_bg_opa(det_sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(det_sep, 0, 0);
    lv_obj_set_style_pad_all(det_sep, 0, 0);

    // Helper: data row returns the value label
    auto det_row = [&](const char *key, lv_color_t vc) -> lv_obj_t * {
        lv_obj_t *row = lv_obj_create(card);
        lv_obj_set_size(row, cw - 24, 20);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *k = make_label(row, FONT_SMALL, C_DIM, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(k, 0, 2);
        lv_obj_set_width(k, key_w);
        lv_label_set_text(k, key);
        lv_obj_t *v = make_label(row, FONT_DATA, vc, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(v, key_w + 4, 0);
        lv_obj_set_width(v, val_w);
        lv_label_set_text(v, "---");
        return v;
    };

    det_alt   = det_row("ALT",     lv_color_hex(0x66bb6a));
    det_spd   = det_row("SPEED",   C_SPD);
    det_hdg   = det_row("HEADING", C_TEXT);
    det_dist  = det_row("DIST",    C_DIST);
    det_vs    = det_row("V/SPEED", C_DIM);

    // Close button
    lv_obj_t *btn_close = lv_btn_create(card);
    lv_obj_set_size(btn_close, cw - 24, 32);
    lv_obj_set_style_bg_color(btn_close, C_SURFACE2, 0);
    lv_obj_set_style_border_width(btn_close, 0, 0);
    lv_obj_set_style_radius(btn_close, 4, 0);
    lv_obj_add_event_cb(btn_close, cb_close_detail, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_close_lbl = lv_label_create(btn_close);
    lv_label_set_text(btn_close_lbl, "CLOSE");
    lv_obj_set_style_text_font(btn_close_lbl, FONT_SMALL, 0);
    lv_obj_set_style_text_color(btn_close_lbl, C_DIM, 0);
    lv_obj_center(btn_close_lbl);

    // ── Status overlay ────────────────────────────────────────────────────────
    ov_status = lv_obj_create(scr_main);
    lv_obj_set_size(ov_status, scr_w, scr_h);
    lv_obj_set_pos(ov_status, 0, 0);
    lv_obj_set_style_bg_color(ov_status, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov_status, LV_OPA_80, 0);
    lv_obj_set_style_border_width(ov_status, 0, 0);
    lv_obj_clear_flag(ov_status, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ov_status, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *spinner = lv_spinner_create(ov_status);
    lv_obj_set_size(spinner, 48, 48);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_arc_color(spinner, C_ACCENT, LV_PART_INDICATOR);

    lbl_status = lv_label_create(ov_status);
    lv_label_set_text(lbl_status, "Loading...");
    lv_obj_set_style_text_font(lbl_status, FONT_HDR, 0);
    lv_obj_set_style_text_color(lbl_status, C_TEXT, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_width(lbl_status, scr_w - 40);
    lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
}

// ── Settings screen build — adapts to orientation ────────────────────────────
static void build_settings_screen() {
    bool port  = (settings.orientation == ORI_PORTRAIT);
    int  scr_w = port ? 272 : 480;
    int  scr_h = port ? 480 : 272;
    int  body_w = scr_w - 24;  // 12px padding each side

    scr_settings = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_settings, C_BG, 0);
    lv_obj_set_style_bg_opa(scr_settings, LV_OPA_COVER, 0);
    // Portrait needs scroll since more sections than fit on screen
    if (port) lv_obj_set_style_base_dir(scr_settings, LV_BASE_DIR_LTR, 0);
    else lv_obj_clear_flag(scr_settings, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t *hdr = lv_obj_create(scr_settings);
    lv_obj_set_size(hdr, scr_w, 40);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, C_SURFACE, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_back = lv_btn_create(hdr);
    lv_obj_set_size(btn_back, 48, 36);
    lv_obj_set_pos(btn_back, 2, 2);
    lv_obj_set_style_bg_color(btn_back, C_SURFACE, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, cb_close_settings, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "< BACK");
    lv_obj_set_style_text_color(lbl_back, C_ACCENT, 0);
    lv_obj_center(lbl_back);

    lv_obj_t *lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "SETTINGS");
    lv_obj_set_style_text_font(lbl_title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(lbl_title, C_TEXT, 0);
    lv_obj_set_pos(lbl_title, 55, 10);

    // ── Body (scrollable in portrait to fit all sections) ────────────────────
    lv_obj_t *body = lv_obj_create(scr_settings);
    lv_obj_set_pos(body, 0, 40);
    lv_obj_set_size(body, scr_w, scr_h - 40);
    lv_obj_set_style_bg_color(body, C_BG, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_radius(body, 0, 0);
    lv_obj_set_style_pad_hor(body, 12, 0);
    lv_obj_set_style_pad_ver(body, 8, 0);
    lv_obj_set_style_pad_row(body, 10, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    if (!port) lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // Section label helper
    auto section = [&](const char *text) {
        lv_obj_t *l = lv_label_create(body);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_font(l, FONT_SMALL, 0);
        lv_obj_set_style_text_color(l, C_ACCENT, 0);
        lv_obj_set_width(l, body_w);
    };

    // Row of buttons helper
    auto row_obj = [&]() {
        lv_obj_t *r = lv_obj_create(body);
        lv_obj_set_size(r, body_w, 36);
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(r, 0, 0);
        lv_obj_set_style_radius(r, 0, 0);
        lv_obj_set_style_pad_all(r, 0, 0);
        lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_flex_main_place(r, LV_FLEX_ALIGN_START, 0);
        lv_obj_set_style_pad_column(r, 6, 0);
        return r;
    };

    // ── Orientation ───────────────────────────────────────────────────────────
    section("ORIENTATION");
    lv_obj_t *row0 = row_obj();
    btn_ori[0] = make_btn(row0, "Landscape", cb_orientation, (void *)(intptr_t)ORI_LANDSCAPE);
    lv_obj_set_size(btn_ori[0], port ? 110 : 120, 32);
    btn_ori[1] = make_btn(row0, "Portrait",  cb_orientation, (void *)(intptr_t)ORI_PORTRAIT);
    lv_obj_set_size(btn_ori[1], port ? 100 : 110, 32);
    lv_obj_set_state(btn_ori[0], LV_STATE_CHECKED, settings.orientation == ORI_LANDSCAPE);
    lv_obj_set_state(btn_ori[1], LV_STATE_CHECKED, settings.orientation == ORI_PORTRAIT);

    // ── Location mode ─────────────────────────────────────────────────────────
    section("LOCATION");
    lv_obj_t *row1 = row_obj();
    btn_loc_auto = make_btn(row1, "Auto (IP)", cb_loc_auto);
    lv_obj_set_size(btn_loc_auto, 100, 32);
    btn_loc_zip  = make_btn(row1, "ZIP Code", cb_loc_zip);
    lv_obj_set_size(btn_loc_zip, 100, 32);

    // ZIP text area
    lv_obj_t *zip_row = row_obj();
    ta_zip = lv_textarea_create(zip_row);
    lv_obj_set_size(ta_zip, 120, 32);
    lv_textarea_set_max_length(ta_zip, 10);
    lv_textarea_set_one_line(ta_zip, true);
    lv_textarea_set_placeholder_text(ta_zip, "ZIP Code");
    lv_obj_set_style_text_font(ta_zip, FONT_DATA, 0);
    lv_obj_set_style_text_color(ta_zip, C_TEXT, 0);
    lv_obj_set_style_bg_color(ta_zip, C_SURFACE2, 0);
    lv_obj_set_style_border_color(ta_zip, C_SEP, 0);
    lv_obj_add_event_cb(ta_zip, cb_ta_zip, LV_EVENT_CLICKED, nullptr);

    // Current location info (landscape: inline; portrait: separate row)
    if (!port) {
        lbl_loc_info = make_label(zip_row, FONT_SMALL, C_DIM, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(lbl_loc_info, 130, 8);
        lv_obj_set_width(lbl_loc_info, body_w - 130);
    } else {
        lv_obj_t *info_row = row_obj();
        lbl_loc_info = make_label(info_row, FONT_SMALL, C_DIM, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_width(lbl_loc_info, body_w);
    }

    // Keyboard (hidden by default, sized for current orientation)
    kb = lv_keyboard_create(scr_settings);
    lv_obj_set_size(kb, scr_w, port ? 220 : 180);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta_zip);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_event_cb(kb, cb_kb, LV_EVENT_READY,  nullptr);
    lv_obj_add_event_cb(kb, cb_kb, LV_EVENT_CANCEL, nullptr);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    // ── Radius ───────────────────────────────────────────────────────────────
    section("SEARCH RADIUS");
    lv_obj_t *row2 = row_obj();
    const char *r_labels[] = {"25 mi", "50 mi", "100 mi", "200 mi"};
    int r_w = port ? 56 : 80;
    for (int i = 0; i < 4; i++) {
        btn_r[i] = make_btn(row2, r_labels[i], cb_radius, (void *)(intptr_t)i);
        lv_obj_set_size(btn_r[i], r_w, 32);
    }

    // ── Update interval ───────────────────────────────────────────────────────
    section("UPDATE EVERY");
    lv_obj_t *row3 = row_obj();
    const char *i_labels[] = {"30 s", "60 s", "2 min", "5 min"};
    int i_w = port ? 56 : 80;
    for (int i = 0; i < 4; i++) {
        btn_i[i] = make_btn(row3, i_labels[i], cb_interval, (void *)(intptr_t)i);
        lv_obj_set_size(btn_i[i], i_w, 32);
    }
}

// ── WiFi screen build ─────────────────────────────────────────────────────────
static void build_wifi_screen() {
    scr_wifi = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_wifi, C_BG, 0);
    lv_obj_set_style_bg_opa(scr_wifi, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr_wifi, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_label_create(scr_wifi);
    lv_label_set_text(icon, "WiFi");
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_38, 0);
    lv_obj_set_style_text_color(icon, C_ACCENT, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *l1 = lv_label_create(scr_wifi);
    lv_label_set_text(l1, "Connect to Wi-Fi");
    lv_obj_set_style_text_font(l1, FONT_TITLE, 0);
    lv_obj_set_style_text_color(l1, C_TEXT, 0);
    lv_obj_align(l1, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t *l2 = lv_label_create(scr_wifi);
    lv_label_set_text(l2, "Join the \"PlaneRadar\" network\nand follow the setup page");
    lv_obj_set_style_text_font(l2, FONT_SMALL, 0);
    lv_obj_set_style_text_color(l2, C_DIM, 0);
    lv_obj_set_style_text_align(l2, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l2, LV_ALIGN_CENTER, 0, 40);

    // Spinner
    lv_obj_t *sp = lv_spinner_create(scr_wifi);
    lv_obj_set_size(sp, 36, 36);
    lv_obj_align(sp, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_arc_color(sp, C_ACCENT, LV_PART_INDICATOR);
}

// ── Rebuild main + settings screens (called after orientation change) ─────────
void ui_rebuild_screens() {
    if (scr_main)     { lv_obj_del(scr_main);     scr_main     = nullptr; }
    if (scr_settings) { lv_obj_del(scr_settings); scr_settings = nullptr; }
    for (int i = 0; i < MAX_ROWS; i++) g_rows[i] = {};
    btn_loc_auto = btn_loc_zip = ta_zip = kb = lbl_loc_info = nullptr;
    for (int i = 0; i < 4; i++) { btn_r[i] = btn_i[i] = nullptr; }
    btn_ori[0] = btn_ori[1] = nullptr;
    btn_tab[0] = btn_tab[1] = btn_tab[2] = nullptr;
    det_overlay = det_flt = det_sub = nullptr;
    det_alt = det_spd = det_hdg = det_dist = det_vs = nullptr;
    ov_status = lbl_status = nullptr;
    lbl_city = lbl_updated = lbl_count = nullptr;
    build_main_screen();
    build_settings_screen();
}

// ── Public init ───────────────────────────────────────────────────────────────
void ui_create() {
    build_wifi_screen();
    build_main_screen();
    build_settings_screen();
    lv_screen_load(scr_wifi);
}

void ui_show_main_screen() {
    lv_screen_load_anim(scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 400, 0, false);
}
