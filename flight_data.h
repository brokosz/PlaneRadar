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

// ── airplanes.live — free community ADS-B aggregator, no key required ────────
// radius is in nautical miles; 1 statute mile ≈ 0.869 nm
// Response: {"ac":[{hex,flight,r,t,lat,lon,alt_baro,gs,track,baro_rate,...}]}
static bool fetch_airplaneslive(float lat, float lon, float radius_mi) {
    float radius_nm = radius_mi * 0.868976f;

    char url[120];
    snprintf(url, sizeof(url),
        "https://api.airplanes.live/v2/point/%.4f/%.4f/%.0f",
        lat, lon, radius_nm);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(15000);

    int code = http.GET();
    Serial.printf("[APL] HTTP %d\n", code);
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) {
        Serial.printf("[APL] JSON err: %s\n", err.c_str());
        return false;
    }

    JsonArray ac = doc["ac"].as<JsonArray>();
    if (ac.isNull()) {
        Serial.println("[APL] No 'ac' array in response");
        return false;
    }

    g_flight_count = 0;
    for (JsonObject a : ac) {
        if (g_flight_count >= MAX_FLIGHTS) break;

        // Skip aircraft on the ground (alt_baro == "ground" or not a number)
        if (!a["alt_baro"].is<int>()) continue;
        int alt = a["alt_baro"].as<int>();
        if (alt <= 0) continue;

        // Need at least a position
        if (a["lat"].isNull() || a["lon"].isNull()) continue;

        Flight &f = g_flights[g_flight_count];
        f.lat         = a["lat"].as<float>();
        f.lon         = a["lon"].as<float>();
        f.altitude_ft = alt;
        f.speed_kts   = a["gs"].as<int>();
        f.heading     = a["track"].as<int>();
        f.vert_speed_fpm = a["baro_rate"].as<int>();  // 0 if missing

        // Flight number / callsign (trim trailing spaces)
        const char *cs = a["flight"] | "";
        strncpy(f.flight_num, cs, sizeof(f.flight_num) - 1);
        f.flight_num[sizeof(f.flight_num) - 1] = '\0';
        for (int i = strlen(f.flight_num) - 1; i >= 0 && f.flight_num[i] == ' '; i--)
            f.flight_num[i] = '\0';
        if (strlen(f.flight_num) == 0)
            strncpy(f.flight_num, a["hex"] | "N/A", sizeof(f.flight_num) - 1);

        strncpy(f.callsign,    f.flight_num,         sizeof(f.callsign)     - 1);
        strncpy(f.aircraft,    a["t"]  | "---",      sizeof(f.aircraft)     - 1);
        strncpy(f.registration,a["r"]  | "---",      sizeof(f.registration) - 1);
        strncpy(f.origin,      "---",                sizeof(f.origin)       - 1);
        strncpy(f.dest,        "---",                sizeof(f.dest)         - 1);

        f.distance_mi = haversine_mi(lat, lon, f.lat, f.lon);
        g_flight_count++;
    }

    Serial.printf("[APL] Parsed %d airborne flights\n", g_flight_count);
    return g_flight_count > 0;
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

// ── Public fetch — airplanes.live primary, OpenSky fallback ─────────────────
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
        Serial.println("[FETCH] airplanes.live failed — trying OpenSky");
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
