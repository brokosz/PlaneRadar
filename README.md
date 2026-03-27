# ✈ PlaneRadar

> Real-time overhead flight display for the **JC4827W543** — the same ESP32-S3 + 4.3" touch TFT board used by [Halo-F1](https://halof1.com).

**[⚡ Flash via browser →](https://brokosz.github.io/PlaneRadar)**

---

![PlaneRadar display mockup](docs/preview.png)

## What it shows

Every aircraft currently overhead, sorted by distance, refreshed every 30 seconds:

| Column | Example | Source |
|--------|---------|--------|
| Flight # | `AA2341` | FlightRadar24 |
| Route | `ORD→LAX` | FlightRadar24 |
| Aircraft type | `B738` | FlightRadar24 |
| Altitude | `35,000 ft` | ADS-B |
| Speed | `485 kts` | ADS-B |
| Trend | `↑` climbing | ADS-B |
| Distance | `12.3 mi` | calculated |

Primary data source: **FlightRadar24** (unofficial feed, no API key needed).
Fallback: **OpenSky Network** (official free API).

---

## Hardware

| | |
|-|--|
| Board | JC4827W543 |
| MCU | ESP32-S3 |
| Display | 4.3" TFT, 480×272, capacitive touch |
| Touch | GT911 |

Same hardware as [Halo-F1](https://halof1.com) — buy it on AliExpress by searching **JC4827W543**.

---

## Flash

Open **[brokosz.github.io/PlaneRadar](https://brokosz.github.io/PlaneRadar)** in Chrome or Edge, plug in your board, click **Install**. No software needed.

> Safari and Firefox don't support Web Serial — use Chrome or Edge.

---

## First boot

1. The board broadcasts a Wi-Fi network called **`PlaneRadar`**
2. Connect to it on your phone — a setup page opens automatically
3. Enter your home Wi-Fi credentials
4. Optionally enter a **ZIP code** (leave blank to auto-locate via IP)
5. Flights appear within ~30 seconds

---

## On-device settings

Tap the **⚙** icon (top-right) at any time:

- **Location** — Auto (IP geolocation) or manual ZIP code
- **Search radius** — 25 / 50 / 100 / 200 miles
- **Update interval** — 30s / 60s / 2min / 5min

---

## Build from source

Firmware is compiled automatically by GitHub Actions on every push. No local toolchain needed.

To trigger a build manually: **Actions → Build Firmware → Run workflow**.

The compiled binary is committed to `docs/firmware/PlaneRadar.bin` and immediately served by the web installer.

### If you want to build locally

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software)
2. Add ESP32 board support (URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`)
3. Install libraries: `ArduinoJson`, `WiFiManager`, `lvgl`, `bb_spi_lcd`, `bb_captouch`
4. Copy driver files from [Halo-F1](https://github.com/FabioRoss/Halo-F1) into this folder: `lv_conf.h`, `lv_bb_spi_lcd.h`, `lv_bb_spi_lcd.cpp`, `touchscreen.h`
5. Board settings: ESP32S3 Dev Module · Flash 16MB · PSRAM OPI · Partition Huge APP · USB HW CDC
6. **Sketch → Export Compiled Binary**, then run `./scripts/export_firmware.sh`

---

## Project structure

```
PlaneRadar/
├── PlaneRadar.ino       Main sketch — init, WiFi, fetch loop
├── ui.h                 LVGL screens (main, settings, WiFi)
├── flight_data.h        FlightRadar24 + OpenSky fetch & parse
├── location.h           IP geolocation + ZIP → lat/lon
├── settings.h           NVS-persisted settings
├── docs/
│   ├── index.html       Web installer page (GitHub Pages)
│   ├── manifest.json    esp-web-tools firmware manifest
│   └── firmware/        Compiled binary (auto-updated by CI)
└── .github/workflows/
    └── build.yml        Arduino CLI build → commit firmware
```

---

## Credits

- Hardware driver files (`lv_bb_spi_lcd`, `touchscreen.h`) — [Halo-F1 by FabioRoss](https://github.com/FabioRoss/Halo-F1)
- Web installer — [esp-web-tools by ESPHome](https://esphome.github.io/esp-web-tools/)
- Flight data — [FlightRadar24](https://flightradar24.com) / [OpenSky Network](https://opensky-network.org)

---

*Not affiliated with FlightRadar24. Uses their publicly accessible data feed.*
