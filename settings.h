#pragma once
#include <Arduino.h>
#include <Preferences.h>

enum LocationMode : uint8_t { LOC_AUTO = 0, LOC_ZIP = 1 };
enum Orientation  : uint8_t { ORI_LANDSCAPE = 0, ORI_PORTRAIT = 1 };

struct Settings {
    LocationMode location_mode  = LOC_AUTO;
    char         zip_code[12]   = {0};
    float        latitude       = 0.0f;
    float        longitude      = 0.0f;
    int          radius_miles   = 50;
    int          update_interval_s = 30;
    char         city_name[64]  = "Unknown";
    Orientation  orientation    = ORI_LANDSCAPE;
    bool         commercial_only = false;  // false = all traffic, true = airlines only
};

extern Settings settings;
Settings settings;

inline void settings_load() {
    Preferences p;
    p.begin("planeradar", true);
    settings.location_mode     = (LocationMode)p.getInt("loc_mode", LOC_AUTO);
    p.getString("zip",   settings.zip_code,   sizeof(settings.zip_code));
    settings.latitude          = p.getFloat("lat",      0.0f);
    settings.longitude         = p.getFloat("lon",      0.0f);
    settings.radius_miles      = p.getInt("radius",     50);
    settings.update_interval_s = p.getInt("interval",   30);
    p.getString("city",  settings.city_name,  sizeof(settings.city_name));
    settings.orientation       = (Orientation)p.getInt("orient", ORI_LANDSCAPE);
    settings.commercial_only   = p.getBool("commercial", false);
    p.end();
}

inline void settings_save() {
    Preferences p;
    p.begin("planeradar", false);
    p.putInt("loc_mode",  settings.location_mode);
    p.putString("zip",    settings.zip_code);
    p.putFloat("lat",     settings.latitude);
    p.putFloat("lon",     settings.longitude);
    p.putInt("radius",    settings.radius_miles);
    p.putInt("interval",  settings.update_interval_s);
    p.putString("city",   settings.city_name);
    p.putInt("orient",    settings.orientation);
    p.putBool("commercial", settings.commercial_only);
    p.end();
}
