# AGENTS.md — Air Quality Checker

Guidelines for AI agents working on this codebase.

---

## Project Overview

Battery-powered indoor air quality monitor.
- **MCU**: ESP8266 Feather HUZZAH (Arduino framework via PlatformIO)
- **Sensor**: BME688 STEMMA QT over I2C
- **Actuator**: SG92R micro servo — physical IAQ feedback
- **Connectivity**: WiFi is optional; device must function fully offline

---

## Architecture

The firmware is split into single-responsibility components wired together in `main.cpp`:

| File | Responsibility |
|---|---|
| `main.cpp` | Entry point; wires components; owns the `onSensorData` callback |
| `BirdySensor` | BME688 driver via BSEC2; EEPROM state persistence; fires callback with `BirdyData` |
| `BirdyServo` | SG92R control; maps IAQ score (0–500) to servo angle |
| `BirdyAPI` | Optional WiFi + HTTP upload; guarded so offline mode still compiles |
| `BirdyLED` | Status LED tied to BSEC calibration accuracy state |
| `BirdyData.h` | Shared plain struct: `iaq`, `accuracy`, `temperature`, `humidity`, `pressure`, `co2`, `voc` |
| `secrets.h` | WiFi + API credentials — gitignored, never committed |

Data flows one way: `BirdySensor` → `onSensorData` callback → `BirdyServo`, `BirdyLED`, `BirdyAPI`.

---

## Key Design Constraints

### Battery Life (highest priority)
- ESP8266 deep sleep is the primary lever. Active draw: 70–170 mA; deep sleep: ~20 µA.
- BSEC ULP mode (5-minute sample interval, ~90 µA average) is preferred over LP (3 s, ~0.9 mA).
- The servo must only move when the IAQ *band* changes, not on every sample.
- WiFi must stay off by default. Never enable WiFi unconditionally in `setup()`.
- BSEC calibration state must be saved to EEPROM before any deep sleep and restored on wake; without this, the sensor needs 30+ min to re-stabilize on every boot.

### WiFi is Optional
- All WiFi code lives in `BirdyAPI`. The rest of the firmware must compile and run with WiFi disabled.
- Guard WiFi init behind a compile flag (`WIFI_ENABLED`) or a runtime check, not a hard dependency.

### IAQ Accuracy Awareness
- IAQ accuracy = 0 means the sensor is still stabilizing. Do not drive the servo or report data until accuracy >= 1.
- Accuracy = 3 is the target for reliable readings (takes hours of exposure to varying air quality).
- The `BirdyLED` communicates accuracy state to the user visually.

---

## BME688 / BSEC2 Notes

- Library: `boschsensortec/Bosch-BSEC2-Library` (closed-source binary, ESP8266 confirmed working)
- BSEC config file: `config/bme688/bme688_sel_33v_3s_4d/bsec_selectivity.txt` (LP mode config; swap for `300s` variant for ULP)
- I2C address: `BME68X_I2C_ADDR_HIGH` = `0x77`; alternate `0x76` via SDO jumper
- ESP8266 I2C pins: SDA = GPIO 4, SCL = GPIO 5
- BSEC state blob stored at EEPROM address 0 (size byte) + 1..N (state bytes). Do not use address 0 for other purposes.
- IAQ scale: 0 (clean) → 500 (extremely polluted). Bands: 0–50 excellent, 51–100 good, 101–150 light, 151–200 moderate, 201–250 heavy, 251–350 severe, >350 extreme.
- eCO2 is a VOC-correlated estimate, **not** a real CO₂ measurement. Do not present it as such.

---

## SG92R Servo Notes

- Operating voltage: 4.8–6 V. Power from VBAT/VUSB rail, **not** the 3.3 V regulator.
- PWM: standard 50 Hz, 1–2 ms pulse width (0°–180°).
- Only rotate when IAQ band changes to avoid draining the battery with constant tiny adjustments.
- Detach servo (disable PWM signal) after movement completes to cut idle current.

---

## Coding Conventions

- Language: C++11, Arduino framework
- Class per component; header declares interface, `.cpp` implements it
- No global state except the singleton pattern already used in `BirdySensor` (instance pointer for static callback)
- No `delay()` in `loop()` — use non-blocking timing (`millis()` deltas) or BSEC's own timing
- No heap allocation after `setup()` completes
- `Serial.println` for debug; wrap in `#ifdef DEBUG` for release builds
- `secrets.h` is gitignored — always use `#ifdef` guards or `#ifndef` defaults so it compiles without it

---

## What NOT to Change Without Discussion

- BSEC calibration state save/load logic in `BirdySensor` — the EEPROM layout is fixed and changing it invalidates saved state on deployed devices
- The `BirdyData` struct field names and types — downstream components depend on the exact layout
- WiFi as a hard boot dependency — it must remain optional

---

## PlatformIO

- Board: `huzzah`
- Platform: `espressif8266`
- Framework: `arduino`
- See `platformio.ini` for lib_deps

---

## Testing

No automated test suite. Verify with:
1. Serial monitor at 115200 baud — check BSEC outputs and accuracy progression
2. Servo physically responds to IAQ changes
3. Deep sleep current measurable with a multimeter in series with the battery line
