# PlaneRadar — Install Guide

Airplane overhead display for the **JC4827W543** board (ESP32-S3, 4.3" 480×272 touch TFT) — the same hardware as [Halo-F1](https://halof1.com).

---

## 1. Copy hardware driver files from Halo-F1

These four files are specific to the JC4827W543 hardware. Copy them from the [Halo-F1 GitHub repo](https://github.com/FabioRoss/Halo-F1) into this sketch folder:

| File | What it does |
|------|--------------|
| `lv_conf.h` | LVGL 9.3 configuration (fonts, features, memory) |
| `lv_bb_spi_lcd.h` | Display driver header |
| `lv_bb_spi_lcd.cpp` | Display driver (LVGL ↔ bb_spi_lcd bridge) |
| `touchscreen.h` | GT911 capacitive touch → LVGL input device |

Your sketch folder should then contain:
```
PlaneRadar/
├── PlaneRadar.ino
├── ui.h
├── flight_data.h
├── location.h
├── settings.h
├── lv_conf.h          ← from Halo-F1
├── lv_bb_spi_lcd.h    ← from Halo-F1
├── lv_bb_spi_lcd.cpp  ← from Halo-F1
└── touchscreen.h      ← from Halo-F1
```

---

## 2. Install Arduino libraries

In Arduino IDE: **Sketch → Include Library → Manage Libraries...**

| Library | Version | Author |
|---------|---------|--------|
| ArduinoJson | ≥ 7.x | Benoit Blanchon |
| WiFiManager | ≥ 2.0.17 | tablatronix |
| LVGL | 9.3.x | lvgl |
| bb_spi_lcd | ≥ 2.7.1 | bitbank2 |
| bb_captouch | latest | bitbank2 |

---

## 3. Arduino IDE board settings

**Tools menu:**

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | Huge APP (3MB No OTA / 1MB SPIFFS) |
| USB Mode | Hardware CDC and JTAG |
| Upload Speed | 921600 |

---

## 4. Verify DISPLAY_TYPE constant

Open `PlaneRadar.ino` and check that `DISPLAY_TYPE` matches the constant used in `lv_bb_spi_lcd.cpp` from Halo-F1. Look for the `lv_bb_spi_lcd_create(...)` call in Halo-F1's `.ino` file — use the same value.

```cpp
#define DISPLAY_TYPE  LCD_CYD543   // ← verify this matches Halo-F1
```

---

## 5. Flash & first boot

1. Connect the board via USB-C
2. Click **Upload** in Arduino IDE
3. On first boot, the device shows a WiFi setup screen
4. On your phone/laptop: connect to the **"PlaneRadar"** Wi-Fi network
5. A captive portal opens — enter your home Wi-Fi credentials
6. Optionally enter a **ZIP code** in the portal (leave blank to auto-detect via IP)
7. Save → the device connects, locates you, and starts showing flights

---

## 6. Changing settings after first boot

Tap the **⚙ gear icon** (top-right of main screen) to open Settings:

- **Location**: Auto (IP geolocation) or ZIP Code
- **ZIP Code**: Tap the field → numeric keyboard appears → press ✓
- **Search Radius**: 25 / 50 / 100 / 200 miles
- **Update Every**: 30s / 60s / 2min / 5min

Tap **←** to go back. Settings are saved to flash immediately.

---

## 7. Data sources

| Source | Used for | Notes |
|--------|----------|-------|
| [FlightRadar24](https://flightradar24.com) (unofficial) | Primary flight data | Full info: route, aircraft type, flight # |
| [OpenSky Network](https://opensky-network.org) (official) | Fallback | Callsign + position only, no route |
| [ip-api.com](http://ip-api.com) | Auto location | Free, HTTP (not HTTPS) |
| [Nominatim / OSM](https://nominatim.openstreetmap.org) | ZIP → lat/lon | Free, requires User-Agent header |

---

## 8. Display layout

```
┌─ OVERHEAD  Chicago, IL  12s ago  ⚙ ─────────────────────────────────────────┐
│ FLIGHT  ROUTE      TYPE  ALTITUDE    SPEED   ↕  DIST                         │
├─────────────────────────────────────────────────────────────────────────────┤
│ AA2341  ORD→LAX    B738  35,000 ft  485 kts  ↑  12.3 mi                      │
│ UA891   EWR→SFO    B77W  38,000 ft  512 kts  →   8.1 mi                      │
│ SW3421  MDW→PHX    B737  28,000 ft  420 kts  ↓   5.2 mi                      │
│ DL1105  MSP→BOS    A320  33,000 ft  460 kts  →  22.7 mi                      │
│ ...                                                                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

- **Green** flight numbers = closest aircraft
- **Green** altitude = climbing (↑ > 200 fpm)
- **Red** trend = descending (↓ < −200 fpm)
- **Cyan** distance = sorted nearest first
- Scroll the list if more than 6 aircraft are in range
