# AGENTS.md — Air Quality Checker

Guidelines for AI agents working on this codebase.

---

## Project Overview

Battery-powered indoor air quality monitor.
- **Hardware**: custom PCB v9 (rev I) from the sibling [`air-quality-pcb`](../air-quality-pcb) repo
- **MCU**: ESP32-C3-WROOM-02-N4 (Arduino framework via PlatformIO), native USB Serial/JTAG
- **Sensor**: BME688 soldered on-board, I2C **0x76**; optional STEMMA QT port (unused by the firmware)
- **Actuator**: SG92R micro servo — physical IAQ feedback
- **Connectivity**: WiFi is optional; device must function fully offline

---

## Architecture

The firmware is split into single-responsibility components wired together in `main.cpp`:

| File | Responsibility |
|---|---|
| `main.cpp` | Entry point; pin map; boot flow (power check → mode switch → setup portal / WiFi / modem-off); owns the `onSensorData` callback |
| `BirdySensor` | BME688 driver via BSEC2; EEPROM state persistence; fires callback with `BirdyData` |
| `BirdyServo` | SG92R control; maps IAQ score (0–500) to servo angle; gates SERVO_EN (IO10) around moves |
| `BirdyMode` | SW4 slide-switch decode: OFF / SPARSE / CONTINUOUS (read once at boot) |
| `BirdyBattery` | `BirdyPower` snapshot read once per wake: VBAT (IO3 ADC), USB present (IO1 ADC), charging (IO20); VBAT uploaded as nullable `battery` column |
| `BirdyConfig` | Runtime credentials (WiFi + API) in `/config.json` on LittleFS |
| `BirdyStore` | SPARSE-mode reading buffer (`/readings.log`, JSONL) between upload windows |
| `BirdySetup` | Captive-portal provisioning AP `Birdy-Setup-<last 3 MAC bytes>` with ~3 min timeout |
| `BirdyAPI` | WiFi + HTTP upload (single reading and JSON-array batch) |
| `BirdyLED` | Status LED tied to BSEC calibration accuracy state; setup long–short–short pattern |
| `BirdyData.h` | Shared plain struct: `iaq`, `accuracy`, `temperature`, `humidity`, `pressure`, `co2`, `voc` |

Data flows one way: `BirdySensor` → `onSensorData` callback → `BirdyServo`, `BirdyLED`, `BirdyAPI`/`BirdyStore`.

### Pin map (PCB v9, ESP32-C3-WROOM-02)

| GPIO | Net | Use |
|---|---|---|
| IO0 | SERVO_PWM | servo signal via R17 100 Ω |
| IO1 | VBUS_SENSE | ADC1_CH1, 47.5 k / 22.1 k divider, 11 dB — USB present if VBUS > 3.8 V |
| IO2 | QT_PWR_N | STEMMA QT power, active low — unused; left as input so R2 keeps the port off |
| IO3 | VBAT_SENSE | ADC1_CH3, 1 M / 270 k divider, 2.5 dB |
| IO4 | MODE_A | SW4 POS.1 → LOW = WiFi OFF (internal pull-up) |
| IO5 | MODE_B | SW4 POS.3 → LOW = CONTINUOUS (both HIGH = SPARSE) |
| IO6 / IO7 | SDA / SCL | I2C (10 k pull-ups on board) |
| IO8 | STATUS_LED_N | status LED, active low; strap pin |
| IO9 | BOOT | SW2; strap pin |
| IO10 | SERVO_EN | HIGH = servo rail on; R4 100 k pull-down |
| IO18 / IO19 | USB D− / D+ | native USB (console + flashing) |
| IO20 | CHG_DET | charger STAT: LOW = charging (only valid with USB present) |

The pins moved between board revisions (v8 had servo IO5, VBAT IO0, mode IO3/IO4). Check the
`air-quality-pcb` netlist, not old docs, before changing them.

---

## Key Design Constraints

### Battery Life (highest priority)
- ESP32-C3 deep sleep (RTC timer wake) is the primary lever. Board sleep floor ≈ 66 µA (estimate, mostly the AP2112K LDO).
- BSEC ULP mode (5-minute sample interval, ~90 µA average) is preferred over LP (3 s, ~0.9 mA).
- The servo must only move when the IAQ *band* changes, not on every sample.
- **SERVO_EN (IO10)** must be HIGH only around an actual servo move (≥ 50 ms before — firmware uses 100 ms — LOW right after) and **never HIGH across a deep sleep** — `BirdyServo::detach()` enforces the LOW before every sleep.
- **Never drive SERVO_PWM while SERVO_EN is LOW** — it back-feeds the unpowered servo through R17. `BirdyServo` parks the pin LOW whenever the rail is off.
- The ESP32 radio is off until WiFi is started: OFF boots and SPARSE boots outside the upload window must simply never touch `WiFi`.
- BSEC calibration state must be saved before any deep sleep and restored on wake (RTC memory every wake, EEPROM periodically); without this, the sensor needs 30+ min to re-stabilize on every boot.

### Battery protection (servo on VSYS, no boost since PCB v8)
- On battery, **skip servo moves below VBAT 3.5 V** (`VBAT_SERVO_MIN`): the start current can pull VSYS under the LDO dropout and brown out the C3. The band is not updated, so the move is retried on a later wake.
- On battery, **below VBAT 3.3 V** (`VBAT_CUTOFF`) the device flashes the LED once and sleeps 1 h at a time without measuring, until USB is present or the cell recovers.
- Battery/USB state is read once in `setup()`, before WiFi starts (TX load would skew VBAT).
- Servo moves happen before any upload, never during a WiFi TX burst.

### WiFi is Optional
- Whether WiFi runs is decided **at runtime** by the SW4 mode switch (`BirdyMode`), not at compile time — one firmware image serves all modes, no reflashing.
- Credentials are **never compiled in**: they come from the setup portal (`BirdySetup`) and are stored in `/config.json` (`BirdyConfig`). Do not reintroduce `secrets.h` or a `WIFI_ENABLED`-style compile gate.
- `WiFi.persistent(false)` always — the WiFi driver must not keep its own flash copy. (On the ESP32 that copy can only be read after starting the radio, which would cost power on every boot.)

### IAQ Accuracy Awareness
- IAQ accuracy = 0 means the sensor is still stabilizing. Do not drive the servo or report data until accuracy >= 1.
- Accuracy = 3 is the target for reliable readings (takes hours of exposure to varying air quality).
- The `BirdyLED` communicates accuracy state to the user visually.

---

## BME688 / BSEC2 Notes

- Library: `boschsensortec/bsec2@1.10.2610` (closed-source binary; ships `src/esp32c3/libalgobsec.a`, picked by its `extra_script.py`)
- BSEC config file: `config/bme688/bme688_sel_33v_300s_4d/bsec_selectivity.txt` (ULP, 300 s)
- I2C address: `BME68X_I2C_ADDR_LOW` = `0x76` (SDO tied to GND on the PCB, keeps 0x77 free for add-ons)
- I2C pins: SDA = IO6, SCL = IO7 — pass them to `Wire.begin()` (the ESP32-C3 core defaults differ)
- BSEC state blob stored at EEPROM address 0 (size byte) + 1..N (state bytes). Do not use address 0 for other purposes. (ESP32 `EEPROM` is emulated in an NVS partition.)
- RTC copy lives in `RTC_NOINIT_ATTR rtcData` (survives deep sleep and software reset); it is discarded after power-on and brownout resets.
- IAQ scale: 0 (clean) → 500 (extremely polluted). Bands: 0–50 excellent, 51–100 good, 101–150 light, 151–200 moderate, 201–250 heavy, 251–350 severe, >350 extreme.
- eCO2 is a VOC-correlated estimate, **not** a real CO₂ measurement. Do not present it as such.

---

## SG92R Servo Notes

- Rated 4.8–6 V. On PCB v9 it runs from VSYS through a load switch: ≈ 4.5 V on USB, 3.0–4.2 V on battery (less torque and speed on battery).
- PWM: standard 50 Hz, 500–2500 µs pulse range used (0°–180°), via `ESP32Servo` (LEDC).
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
- No credentials in source — provisioning is runtime-only via the setup portal (`BirdyConfig`/`BirdySetup`)

---

## What NOT to Change Without Discussion

- BSEC calibration state save/load logic in `BirdySensor` — the EEPROM layout is fixed and changing it invalidates saved state on deployed devices
- The `BirdyData` struct field names and types — downstream components depend on the exact layout
- WiFi as a hard boot dependency — mode must stay runtime-selectable via SW4, and the device must fully work with the switch OFF
- The SERVO_EN firmware contract and the battery thresholds (see above) — they come from the PCB design review (F19, F25)

---

## PlatformIO

- Board: `esp32-c3-devkitm-1` (same 4 MB flash as the WROOM-02-N4)
- Platform: `espressif32@^6.9.0` (Arduino core 2.0.x)
- Framework: `arduino`, with `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1` (`Serial` = native USB)
- Environments: `birdy` (default), `birdy_debug`
- See `platformio.ini` for lib_deps

---

## Testing

No automated test suite. Verify with:
1. Serial monitor at 115200 baud (native USB) — check BSEC outputs and accuracy progression. The USB port disappears during deep sleep; reconnect after each wake or log to LittleFS.
2. Servo physically responds to IAQ changes
3. Deep sleep current measurable with a multimeter in series with the battery line
