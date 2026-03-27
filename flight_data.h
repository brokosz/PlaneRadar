#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <math.h>
#include "settings.h"

#define MAX_FLIGHTS 40

struct Flight {
    char  flight_num[12];    // "AA2341" or callsign
    char  origin[5];         // IATA "ORD"  (--- if unavailable)
    char  dest[5];           // IATA "LAX"  (--- if unavailable)
    char  aircraft[8];       // ICAO type "B738"
    char  registration[12];  // "N12345"
    char  callsign[12];      // same as flight_num
    float lat;
    float lon;
    int   altitude_ft;
    int   speed_kts;
    int   heading;
    int   vert_speed_fpm;
    float distance_mi;
};

Flight g_flights[MAX_FLIGHTS];
int    g_flight_count   = 0;
unsigned long g_last_fetch_ms = 0;

// ── Haversine distance (miles) ───────────────────────────────────────────────
static float haversine_mi(float lat1, float lon1, float lat2, float lon2) {
    const float R = 3958.8f;
    float dlat = (lat2 - lat1) * (float)M_PI / 180.0f;
    float dlon = (lon2 - lon1) * (float)M_PI / 180.0f;
    float a = sinf(dlat / 2) * sinf(dlat / 2)
            + cosf(lat1 * (float)M_PI / 180.0f)
            * cosf(lat2 * (float)M_PI / 180.0f)
            * sinf(dlon / 2) * sinf(dlon / 2);
    return R * 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
}

static int cmp_distance(const void *a, const void *b) {
    float da = ((const Flight *)a)->distance_mi;
    float db = ((const Flight *)b)->distance_mi;
    return (da > db) - (da < db);
}

// ── ArduinoJson filter for readsb "ac" objects ───────────────────────────────
// Only the fields we actually use are kept; everything else is discarded
// during streaming. Reduces peak heap by ~70% on large responses.
static void make_readsb_filter(JsonDocument &f) {
    f["ac"][0]["hex"]       = true;
    f["ac"][0]["flight"]    = true;
    f["ac"][0]["r"]         = true;
    f["ac"][0]["t"]         = true;
    f["ac"][0]["lat"]       = true;
    f["ac"][0]["lon"]       = true;
    f["ac"][0]["alt_baro"]  = true;
    f["ac"][0]["gs"]        = true;
    f["ac"][0]["track"]     = true;
    f["ac"][0]["baro_rate"] = true;
}

// ── Shared parser for readsb-format "ac" arrays (airplanes.live + adsb.fi) ───
static int parse_readsb_ac(JsonArray ac, float ref_lat, float ref_lon) {
    int count = 0;
    for (JsonObject a : ac) {
        if (count >= MAX_FLIGHTS) break;
        if (!a["alt_baro"].is<int>()) continue;   // "ground" string → skip
        int alt = a["alt_baro"].as<int>();
        if (alt <= 0) continue;
        if (a["lat"].isNull() || a["lon"].isNull()) continue;

        Flight &f = g_flights[count];
        f.lat            = a["lat"].as<float>();
        f.lon            = a["lon"].as<float>();
        f.altitude_ft    = alt;
        f.speed_kts      = a["gs"].as<int>();
        f.heading        = a["track"].as<int>();
        f.vert_speed_fpm = a["baro_rate"].as<int>();

        const char *cs = a["flight"] | "";
        strncpy(f.flight_num, cs, sizeof(f.flight_num) - 1);
        f.flight_num[sizeof(f.flight_num) - 1] = '\0';
        for (int i = strlen(f.flight_num) - 1; i >= 0 && f.flight_num[i] == ' '; i--)
            f.flight_num[i] = '\0';
        if (strlen(f.flight_num) == 0)
            strncpy(f.flight_num, a["hex"] | "N/A", sizeof(f.flight_num) - 1);

        strncpy(f.callsign,     f.flight_num,    sizeof(f.callsign)     - 1);
        strncpy(f.aircraft,     a["t"] | "---",  sizeof(f.aircraft)     - 1);
        strncpy(f.registration, a["r"] | "---",  sizeof(f.registration) - 1);
        strncpy(f.origin,       "---",            sizeof(f.origin)       - 1);
        strncpy(f.dest,         "---",            sizeof(f.dest)         - 1);

        f.distance_mi = haversine_mi(ref_lat, ref_lon, f.lat, f.lon);
        count++;
    }
    return count;
}

// ── Shared HTTP fetch + filtered JSON parse for readsb-format APIs ───────────
// Plain HTTP (no TLS) eliminates the mbedTLS 16 KB contiguous-SRAM
// requirement that causes esp-aes OOM failures on a fragmented heap.
// Flight data is public — no need for encryption.
static bool fetch_readsb_url(const char *tag, const char *url,
                              float ref_lat, float ref_lon) {
    WiFiClient client;   // plain TCP — no TLS overhead

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(15000);

    int code = http.GET();
    Serial.printf("[%s] HTTP %d\n", tag, code);
    if (code != 200) { http.end(); return false; }

    JsonDocument filter;
    make_readsb_filter(filter);
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();
    if (err) { Serial.printf("[%s] JSON err: %s\n", tag, err.c_str()); return false; }

    JsonArray ac = doc["ac"].as<JsonArray>();
    if (ac.isNull()) { Serial.printf("[%s] no ac array\n", tag); return false; }

    g_flight_count = parse_readsb_ac(ac, ref_lat, ref_lon);
    Serial.printf("[%s] Parsed %d airborne flights\n", tag, g_flight_count);
    return g_flight_count > 0;
}

// ── airplanes.live — free community ADS-B aggregator ─────────────────────────
static bool fetch_airplaneslive(float lat, float lon, float radius_mi) {
    char url[120];
    snprintf(url, sizeof(url),
        "http://api.airplanes.live/v2/point/%.4f/%.4f/%.0f",
        lat, lon, radius_mi * 0.868976f);
    return fetch_readsb_url("APL", url, lat, lon);
}

// ── adsb.fi open data — second community ADS-B source ────────────────────────
static bool fetch_adsbfi(float lat, float lon, float radius_mi) {
    char url[120];
    snprintf(url, sizeof(url),
        "http://opendata.adsb.fi/api/v3/lat/%.4f/lon/%.4f/dist/%.0f",
        lat, lon, radius_mi * 0.868976f);
    return fetch_readsb_url("ADSBFI", url, lat, lon);
}

// ── OpenSky Network fallback ──────────────────────────────────────────────────
// Anonymous tier: 100 requests/day. Used only when primary fails.
static bool fetch_opensky(float lat, float lon, float radius_mi) {
    float dlat = radius_mi / 69.0f;
    float dlon = radius_mi / (69.0f * cosf(lat * (float)M_PI / 180.0f));

    char url[280];
    snprintf(url, sizeof(url),
        "https://opensky-network.org/api/states/all"
        "?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f",
        lat - dlat, lon - dlon,
        lat + dlat, lon + dlon);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(15000);

    int code = http.GET();
    Serial.printf("[OSN] HTTP %d\n", code);
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) {
        Serial.printf("[OSN] JSON err: %s\n", err.c_str());
        return false;
    }

    if (!doc["states"].is<JsonArray>()) {
        Serial.println("[OSN] states is null or missing (rate-limited?)");
        return false;
    }

    g_flight_count = 0;
    for (JsonArray state : doc["states"].as<JsonArray>()) {
        if (g_flight_count >= MAX_FLIGHTS) break;
        if (state[5].isNull() || state[6].isNull()) continue;
        if (state[8].as<bool>()) continue; // on ground

        Flight &f = g_flights[g_flight_count];
        f.lon         = state[5].as<float>();
        f.lat         = state[6].as<float>();
        f.altitude_ft = (int)(state[7].as<float>() * 3.28084f);
        f.speed_kts   = (int)(state[9].as<float>() * 1.94384f);
        f.heading     = (int)state[10].as<float>();
        f.vert_speed_fpm = (int)((state[11].isNull() ? 0.0f : state[11].as<float>()) * 196.85f);

        const char *cs = state[1] | "N/A";
        strncpy(f.callsign,   cs, sizeof(f.callsign)   - 1);
        strncpy(f.flight_num, cs, sizeof(f.flight_num) - 1);
        for (int i = strlen(f.flight_num) - 1; i >= 0 && f.flight_num[i] == ' '; i--)
            f.flight_num[i] = '\0';

        strncpy(f.origin,      "---", sizeof(f.origin)       - 1);
        strncpy(f.dest,        "---", sizeof(f.dest)         - 1);
        strncpy(f.aircraft,    "---", sizeof(f.aircraft)     - 1);
        strncpy(f.registration,"---", sizeof(f.registration) - 1);

        f.distance_mi = haversine_mi(lat, lon, f.lat, f.lon);
        g_flight_count++;
    }

    Serial.printf("[OSN] Parsed %d flights\n", g_flight_count);
    return g_flight_count > 0;
}

// ── Public fetch — airplanes.live → adsb.fi → OpenSky ───────────────────────
inline bool flights_fetch() {
    float lat = settings.latitude;
    float lon = settings.longitude;
    float r   = (float)settings.radius_miles;

    if (lat == 0.0f && lon == 0.0f) {
        Serial.println("[FETCH] No location — skipping");
        return false;
    }

    bool ok = fetch_airplaneslive(lat, lon, r);
    if (!ok) {
        Serial.println("[FETCH] airplanes.live failed — trying adsb.fi");
        ok = fetch_adsbfi(lat, lon, r);
    }
    if (!ok) {
        Serial.println("[FETCH] adsb.fi failed — trying OpenSky");
        ok = fetch_opensky(lat, lon, r);
    }

    if (ok && g_flight_count > 0) {
        qsort(g_flights, g_flight_count, sizeof(Flight), cmp_distance);
        g_last_fetch_ms = millis();
    }

    return ok;
}

// ── Display helpers ───────────────────────────────────────────────────────────
inline const char *trend_arrow(const Flight &f) {
    if (f.vert_speed_fpm > 200)  return LV_SYMBOL_UP;
    if (f.vert_speed_fpm < -200) return LV_SYMBOL_DOWN;
    return LV_SYMBOL_RIGHT;
}

inline uint32_t trend_color(const Flight &f) {
    if (f.vert_speed_fpm > 200)  return 0x66bb6a; // green  – climbing
    if (f.vert_speed_fpm < -200) return 0xef5350; // red    – descending
    return 0x888888;                               // grey   – level
}
