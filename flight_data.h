#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <math.h>
#include "settings.h"

#define MAX_FLIGHTS 40

struct Flight {
    char  flight_num[12];    // "AA2341"
    char  origin[5];         // IATA "ORD"
    char  dest[5];           // IATA "LAX"
    char  aircraft[8];       // "B738"
    char  registration[12];  // "N12345"
    char  callsign[12];      // "AAL2341"
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
unsigned long g_last_fetch_ms = 0;   // millis() at last successful fetch

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

// ── FlightRadar24 unofficial API ─────────────────────────────────────────────
// bounds = north,south,west,east
static bool fetch_fr24(float lat, float lon, float radius_mi) {
    float dlat = radius_mi / 69.0f;
    float dlon = radius_mi / (69.0f * cosf(lat * (float)M_PI / 180.0f));

    char url[380];
    snprintf(url, sizeof(url),
        "https://data-live.flightradar24.com/zones/fcgi/feed.js"
        "?bounds=%.4f,%.4f,%.4f,%.4f"
        "&faa=1&satellite=1&mlat=1&flarm=1&adsb=1"
        "&gnd=0&air=1&vehicles=0&estimated=0&maxage=14400&gliders=1&stats=0",
        lat + dlat, lat - dlat,
        lon - dlon, lon + dlon);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.addHeader("User-Agent", "Mozilla/5.0 (compatible; PlaneRadar/1.0)");
    http.addHeader("Accept",     "application/json");
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int code = http.GET();
    Serial.printf("[FR24] HTTP %d\n", code);
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (err) { Serial.printf("[FR24] JSON: %s\n", err.c_str()); return false; }

    g_flight_count = 0;
    for (JsonPair kv : doc.as<JsonObject>()) {
        if (g_flight_count >= MAX_FLIGHTS) break;
        if (!kv.value().is<JsonArray>()) continue;
        JsonArray arr = kv.value().as<JsonArray>();
        if (arr.size() < 13) continue;

        Flight &f = g_flights[g_flight_count];
        f.lat         = arr[0].as<float>();
        f.lon         = arr[1].as<float>();
        f.heading     = arr[2].as<int>();
        f.altitude_ft = arr[3].as<int>();
        f.speed_kts   = arr[4].as<int>();
        strncpy(f.aircraft,    arr[7]  | "",    sizeof(f.aircraft)     - 1);
        strncpy(f.registration,arr[8]  | "",    sizeof(f.registration) - 1);
        strncpy(f.origin,      arr[10] | "---", sizeof(f.origin)       - 1);
        strncpy(f.dest,        arr[11] | "---", sizeof(f.dest)         - 1);
        strncpy(f.flight_num,  arr[12] | "",    sizeof(f.flight_num)   - 1);
        f.vert_speed_fpm = (arr.size() > 14) ? arr[14].as<int>() : 0;
        strncpy(f.callsign, (arr.size() > 15) ? (arr[15] | "") : "",
                sizeof(f.callsign) - 1);

        // Fall back to callsign if no flight number
        if (strlen(f.flight_num) == 0 && strlen(f.callsign) > 0)
            strncpy(f.flight_num, f.callsign, sizeof(f.flight_num) - 1);
        if (strlen(f.flight_num) == 0)
            strncpy(f.flight_num, "N/A", sizeof(f.flight_num) - 1);

        f.distance_mi = haversine_mi(lat, lon, f.lat, f.lon);
        g_flight_count++;
    }

    Serial.printf("[FR24] Parsed %d flights\n", g_flight_count);
    return true;
}

// ── OpenSky Network fallback (official free API, no key needed) ───────────────
// Returns less info (no route/type), but very reliable
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
    if (err || !doc["states"].is<JsonArray>()) return false;

    g_flight_count = 0;
    for (JsonArray state : doc["states"].as<JsonArray>()) {
        if (g_flight_count >= MAX_FLIGHTS) break;
        if (state[5].isNull() || state[6].isNull()) continue; // no position
        if (state[8].as<bool>()) continue;                     // on ground

        Flight &f = g_flights[g_flight_count];
        f.lon         = state[5].as<float>();
        f.lat         = state[6].as<float>();
        f.altitude_ft = (int)(state[7].as<float>() * 3.28084f);   // m → ft
        f.speed_kts   = (int)(state[9].as<float>() * 1.94384f);   // m/s → kts
        f.heading     = (int)(state[10].as<float>());
        f.vert_speed_fpm = (int)(state[11].isNull() ? 0 : state[11].as<float>() * 196.85f);

        // Callsign (trim trailing spaces)
        const char *cs = state[1] | "N/A";
        strncpy(f.callsign,   cs, sizeof(f.callsign)   - 1);
        strncpy(f.flight_num, cs, sizeof(f.flight_num) - 1);
        // trim trailing spaces
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
    return true;
}

// ── Public fetch — FR24 primary, OpenSky fallback ───────────────────────────
inline bool flights_fetch() {
    float lat = settings.latitude;
    float lon = settings.longitude;
    float r   = (float)settings.radius_miles;

    if (lat == 0.0f && lon == 0.0f) {
        Serial.println("[FETCH] No location set — skipping");
        return false;
    }

    bool ok = fetch_fr24(lat, lon, r);
    if (!ok || g_flight_count == 0) {
        Serial.println("[FETCH] FR24 failed or empty — trying OpenSky");
        ok = fetch_opensky(lat, lon, r);
    }

    if (ok && g_flight_count > 0) {
        qsort(g_flights, g_flight_count, sizeof(Flight), cmp_distance);
        g_last_fetch_ms = millis();
    }

    return ok;
}

// ── Helpers ──────────────────────────────────────────────────────────────────
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
