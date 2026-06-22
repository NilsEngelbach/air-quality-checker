# Air Quality Checker

A battery-powered indoor air quality monitor using an ESP8266, BME688 sensor, and servo motor for physical feedback. WiFi connectivity is optional and additive — the core loop runs standalone.

---

## Requirements

### Functional
1. Measure air quality on a regular interval and provide physical feedback by rotating the servo motor to indicate the current IAQ level.

### Additional
1. WiFi connectivity is optional — the device must work fully offline.
2. Battery-powered; battery life should be maximized through aggressive power management.

---

## Hardware

| Component | Part | Notes |
|---|---|---|
| MCU | Adafruit Feather HUZZAH ESP8266 | 3.3 V logic, built-in LiPo charger |
| Sensor | Adafruit BME688 STEMMA QT | I2C via JST SH cable |
| Actuator | SG92R Micro Servo | Physical IAQ feedback |

---

## BME688 Sensor

### What It Measures

| Parameter | Range | Accuracy |
|---|---|---|
| Temperature | -40 to +85 °C | ±1.0 °C (±0.5 °C at 25 °C) |
| Humidity | 0–100 % RH | ±3 % RH |
| Barometric Pressure | 300–1100 hPa | ±1 hPa absolute |
| Gas / VOC (MOX) | ppb level | Broadband; see limitations |

The gas sensor is a **Metal Oxide Semiconductor (MOX)** element. It detects reducing gases (VOCs broadly: ethanol, acetone, toluene, H₂, CO, H₂S, alcohols) and oxidizing gases (NO₂, ozone) by measuring the electrical resistance of a heated metal oxide surface.

### Air Quality Index (IAQ)

Bosch's **BSEC2** library processes raw sensor data into an IAQ score. This requires a closed-source pre-compiled binary running on the MCU.

| IAQ Score | Classification |
|---|---|
| 0–50 | Excellent |
| 51–100 | Good |
| 101–150 | Lightly polluted |
| 151–200 | Moderately polluted |
| 201–250 | Heavily polluted |
| 251–350 | Severely polluted |
| >350 | Extremely polluted |

BSEC also outputs:
- **eCO2** — estimated CO₂ equivalent in ppm (derived from VOC correlation, **not** a true CO₂ reading)
- **bVOC** — breath VOC equivalent in ppm
- **IAQ Accuracy** (0–3): calibration confidence level

#### IAQ Accuracy States

| Value | Meaning |
|---|---|
| 0 | Stabilizing (first ~5 min after power-on) |
| 1 | Uncertain — needs more environmental variation |
| 2 | Calibrating — auto-trim in progress |
| 3 | Calibrated — high accuracy |

**Important:** First-time use requires ~48 hours of burn-in. Subsequent power-ons need ~30 minutes stabilization unless BSEC calibration state is saved to flash/EEPROM and restored on boot.

### BME688 vs BME680

The BME688 adds **multi-step gas scanning**: up to 10 programmable heater set-points per scan cycle (vs. 1 on BME680). Different gases have distinct resistance response curves at different temperatures, so the multi-point scan provides better selectivity and is the basis for Bosch's AI Studio custom model training.

### Power Consumption

| Mode | Average Current | Sample Interval |
|---|---|---|
| Sleep (sensor only) | ~0.15 µA | — |
| T/H/P only at 1 Hz | ~3.7 µA | 1 s |
| BSEC ULP (Ultra-Low Power) | ~90 µA | 300 s (5 min) |
| BSEC LP (Low Power) | ~0.9 mA | 3 s |
| Active gas scan (heater on) | ~3.9 mA | during scan only |

The **ESP8266 dominates current draw** (active: 70–170 mA; deep sleep: ~20 µA). The primary battery-life lever is ESP8266 deep sleep duration. The sensor should run in **ULP mode** or **forced mode** (manual single-shot) to match.

### I2C Wiring (STEMMA QT → Feather HUZZAH)

| Wire Color | Signal | Huzzah Pin |
|---|---|---|
| Red | 3.3 V | 3V |
| Black | GND | GND |
| Blue | SDA | GPIO 4 (SDA) |
| Yellow | SCL | GPIO 5 (SCL) |

Default I2C address: **0x77** (alternate 0x76 via SDO solder jumper on the breakout).

### Known Limitations

- **Cannot measure actual CO₂.** eCO₂ is an estimate correlated from VOC readings.
- **Cannot identify specific gas species** from a single heater temperature — it is a broadband detector, not a spectrometer.
- **No reliable absolute agreement between units** without per-device calibration. Baselines vary >100% across devices; BSEC auto-calibration compensates over time.
- **Humidity and VOC cross-sensitivity** at a single temperature — BSEC's compensation partially mitigates this.
- **Outdoor use is unreliable** — BSEC IAQ is calibrated for indoor environments.
- **Not a precision instrument.** Provides reliable trends, not absolute values.

---

## Libraries

| Library | Purpose | Notes |
|---|---|---|
| `boschsensortec/Bosch-BME68x-Library` | Raw sensor driver | Open-source |
| `BSEC Software Library` (v2.x) | IAQ, eCO2, bVOC via BSEC2 | Closed-source pre-compiled binary; ESP8266 confirmed |
| `adafruit/Adafruit BME680 Library` | Alternative: raw T/H/P/gas resistance only | Open-source, simpler, no IAQ |

For battery-powered use with IAQ: use **BSEC2** and persist calibration state to EEPROM/LittleFS before deep sleep; restore on wake.

---

## Power Strategy

1. **ESP8266 deep sleep** between measurements is the dominant factor.
2. Sensor runs in **BSEC ULP mode** (5-minute intervals) or **forced mode** for maximum battery life.
3. **WiFi stays off by default**; only enabled on demand (e.g., button press, scheduled upload window).
4. BSEC calibration state is saved to EEPROM so accuracy is not lost across sleep cycles.
5. Servo is driven only when the IAQ level changes band, not on every measurement.

---

## Project Structure

```
src/
  main.cpp          — setup/loop, component wiring
  BirdySensor.*     — BME688 + BSEC2 integration, EEPROM state persistence
  BirdyServo.*      — SG92R control, IAQ → angle mapping
  BirdyAPI.*        — optional WiFi + HTTP data upload
  BirdyLED.*        — status LED
  BirdyData.h       — shared data struct (IAQ, temp, humidity, pressure, CO2, VOC)
  secrets.h         — WiFi credentials and API config (gitignored)
doc/
  setup-1.jpg       — wiring photo
```

---

## Setup

### Prerequisites
- PlatformIO IDE extension (VS Code)
- USB driver: [Adafruit Feather HUZZAH setup guide](https://learn.adafruit.com/adafruit-feather-huzzah-esp8266/using-arduino-ide)

### WiFi / API (optional)
Copy `src/secrets.h.example` to `src/secrets.h` and fill in your values:
```c
#pragma once

#define WIFI_SSID     "<<YourWiFiSSID>>"
#define WIFI_PASSWORD "<<YourWiFiPassword>>"

#define BIRDY_ID  "<<YourBirdyUUID>>"
#define API_KEY   "<<YourSupabaseServiceRoleKey>>"
#define API_URL   "https://<<YourProjectRef>>.supabase.co/rest/v1/air_quality_data"
```

The `API_KEY` must be the **service_role** key from your Supabase project so the device can insert readings while RLS is enabled.

### Build & Flash
```sh
pio run --target upload
pio device monitor
```

---

## References

- [Bosch BME688 Product Page](https://www.bosch-sensortec.com/en/products/environmental-sensors/gas-sensors/bme688)
- [Bosch BSEC2 Library — GitHub](https://github.com/boschsensortec/Bosch-BSEC2-Library)
- [Adafruit BME688 STEMMA QT](https://www.adafruit.com/product/5046)
- [Adafruit Feather HUZZAH ESP8266](https://learn.adafruit.com/adafruit-feather-huzzah-esp8266)
- [Hackster.io — Visual CO₂ Indoor Air Quality Sensor](https://www.hackster.io/mcmchris/visual-co2-indoor-air-quality-sensor-509d4c)
- [DIY Air Quality Monitor](https://howtomechatronics.com/projects/diy-air-quality-monitor-pm2-5-co2-voc-ozone-temp-hum-arduino-meter/)
