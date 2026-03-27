#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "settings.h"

// ── IP geolocation via ip-api.com (HTTP only on free tier) ──────────────────
inline bool location_by_ip() {
    HTTPClient http;
    http.begin("http://ip-api.com/json?fields=status,city,regionName,lat,lon,zip");
    http.setTimeout(10000);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err) return false;
    if (strcmp(doc["status"] | "", "success") != 0) return false;

    settings.latitude  = doc["lat"].as<float>();
    settings.longitude = doc["lon"].as<float>();

    // Format: "City, State"
    const char *city   = doc["city"]       | "Unknown";
    const char *region = doc["regionName"] | "";
    if (strlen(region) > 0)
        snprintf(settings.city_name, sizeof(settings.city_name), "%s, %s", city, region);
    else
        strncpy(settings.city_name, city, sizeof(settings.city_name) - 1);

    return true;
}

// ── ZIP code → lat/lon via Nominatim (OSM, free, no key) ────────────────────
inline bool location_by_zip(const char *zip) {
    if (!zip || strlen(zip) == 0) return false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    char url[220];
    snprintf(url, sizeof(url),
        "https://nominatim.openstreetmap.org/search"
        "?postalcode=%s&countrycodes=us&format=json&limit=1&addressdetails=1",
        zip);
    http.begin(client, url);
    http.addHeader("User-Agent", "PlaneRadar/1.0 (ESP32; open-source flight display)");
    http.setTimeout(12000);

    int code = http.GET();
    if (code != 200) { http.end(); return false; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err) return false;
    if (!doc.is<JsonArray>() || doc.as<JsonArray>().size() == 0) return false;

    JsonObject first = doc[0].as<JsonObject>();
    settings.latitude  = atof(first["lat"] | "0");
    settings.longitude = atof(first["lon"] | "0");

    // Try address sub-fields first, fall back to display_name
    const char *city  = first["address"]["city"]   |
                        first["address"]["town"]    |
                        first["address"]["village"] | zip;
    const char *state = first["address"]["state"]  | "";
    if (strlen(state) > 0)
        snprintf(settings.city_name, sizeof(settings.city_name), "%s, %s", city, state);
    else
        strncpy(settings.city_name, city, sizeof(settings.city_name) - 1);

    return true;
}

// ── Main resolver — call once after WiFi connects ───────────────────────────
inline void location_resolve() {
    bool ok = false;

    if (settings.location_mode == LOC_ZIP && strlen(settings.zip_code) > 0) {
        Serial.printf("[LOC] Resolving ZIP: %s\n", settings.zip_code);
        ok = location_by_zip(settings.zip_code);
        if (!ok) Serial.println("[LOC] ZIP lookup failed, falling back to IP");
    }

    if (!ok) {
        Serial.println("[LOC] Resolving via IP geolocation");
        ok = location_by_ip();
    }

    if (ok) {
        Serial.printf("[LOC] %s  (%.4f, %.4f)\n",
            settings.city_name, settings.latitude, settings.longitude);
        settings_save();
    } else {
        Serial.println("[LOC] All location methods failed — using last saved coords");
    }
}
