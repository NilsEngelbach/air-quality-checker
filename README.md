# Air Quality Checker

A battery-powered indoor air quality monitor using an ESP32-C3, BME688 sensor, and servo motor for physical feedback. WiFi connectivity is optional and additive — the core loop runs standalone.

---

## Requirements

### Functional
1. Measure air quality on a regular interval and provide physical feedback by rotating the servo motor to indicate the current IAQ level.

### Additional
1. WiFi connectivity is optional — the device must work fully offline.
2. Battery-powered; battery life should be maximized through aggressive power management.

---

## Hardware

The firmware targets the custom board **v9 (rev I)** from the sibling
[`air-quality-pcb`](../air-quality-pcb) repo (PCB v7 and later share the ESP32-C3). Earlier ESP8266 boards (Feather HUZZAH,
PCB v1–v6) need an ESP8266 firmware revision.

| Component | Part | Notes |
|---|---|---|
| MCU | ESP32-C3-WROOM-02-N4 | 4 MB flash, native USB (console + flashing), RTC-timer deep sleep |
| Sensor | BME688, on-board | I2C **0x76** (SDA IO6, SCL IO7) |
| Actuator | SG92R Micro Servo | Physical IAQ feedback, runs from VSYS through a load switch (SERVO_EN) |
| Power | LiPo (protected cell) + MCP73831 charger over USB-C | VBAT, USB presence and charge status readable by the firmware |
| Mode switch | SW4, 3 positions | WiFi OFF / SPARSE / CONTINUOUS |
| Add-on port | STEMMA QT (JST SH 4P), switched 3V3 | Not used by the firmware yet |

### Pin map (PCB v9)

| GPIO | Net | Use |
|---|---|---|
| IO0 | SERVO_PWM | Servo signal (via 100 Ω) |
| IO1 | VBUS_SENSE | ADC: USB present |
| IO2 | QT_PWR_N | STEMMA QT power, active low (unused, stays off) |
| IO3 | VBAT_SENSE | ADC: battery voltage (1 M / 270 k divider) |
| IO4 / IO5 | MODE_A / MODE_B | SW4: A low = OFF, B low = CONTINUOUS, neither = SPARSE |
| IO6 / IO7 | SDA / SCL | I2C |
| IO8 | STATUS_LED_N | Status LED, active low |
| IO9 | BOOT | BOOT button (force setup / download mode) |
| IO10 | SERVO_EN | Servo rail on/off |
| IO20 | CHG_DET | Charger status (LOW = charging, valid with USB) |

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

The **ESP32-C3 dominates current draw** while awake (WiFi TX peaks ~350 mA; deep sleep: ~5 µA, board floor ≈ 66 µA estimated, mostly the LDO). The primary battery-life lever is deep sleep duration. The sensor should run in **ULP mode** or **forced mode** (manual single-shot) to match.

### I2C

The BME688 sits on the PCB at address **0x76** (SDO tied to GND), on SDA IO6 / SCL IO7 with 10 kΩ pull-ups.
0x77 is left free for add-on breakouts on the STEMMA QT port.

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
| `BSEC Software Library` (v2.x) | IAQ, eCO2, bVOC via BSEC2 | Closed-source pre-compiled binary; pinned 1.10.2610 ships an ESP32-C3 build |
| `madhephaestus/ESP32Servo` | Servo PWM | LEDC-based `Servo` class for the ESP32 family |
| `adafruit/Adafruit BME680 Library` | Alternative: raw T/H/P/gas resistance only | Open-source, simpler, no IAQ |

For battery-powered use with IAQ: use **BSEC2** and persist calibration state to EEPROM/LittleFS before deep sleep; restore on wake.

---

## Power Strategy

1. **ESP32-C3 deep sleep** (RTC timer, 290 s) between measurements is the dominant factor.
2. Sensor runs in **BSEC ULP mode** (5-minute intervals) or **forced mode** for maximum battery life.
3. **WiFi mode is selected by the SW4 slide switch**: OFF / SPARSE (batch upload hourly) / CONTINUOUS (upload every sample). On OFF and between SPARSE windows the radio is never started.
4. BSEC calibration state is kept in RTC memory across sleeps and backed up to EEPROM, so accuracy is not lost.
5. Servo is driven only when the IAQ level changes band, not on every measurement.
6. **SERVO_EN (IO10)** switches the servo rail (VSYS through a load switch): HIGH 100 ms before a move, LOW immediately after, never HIGH across a deep sleep (0 µA idle). The PWM pin is held LOW while the rail is off.
7. **Battery guards** (on battery only): below **3.5 V** servo moves are skipped (a servo start could brown out the MCU) and retried on a later wake; below **3.3 V** the device flashes the LED once and sleeps in 1 h steps without measuring until it is charged.

---

## Project Structure

```
src/
  main.cpp          — pin map, setup/loop, boot flow, per-mode data handling
  BirdySensor.*     — BME688 + BSEC2 integration, EEPROM state persistence
  BirdyServo.*      — SG92R control, IAQ → angle mapping, SERVO_EN gating
  BirdyMode.h       — SW4 slide-switch decode (OFF / SPARSE / CONTINUOUS)
  BirdyBattery.h    — VBAT, USB-present and charging state (ADC + CHG_DET)
  BirdyConfig.*     — runtime credentials (/config.json on LittleFS)
  BirdyStore.*      — SPARSE-mode LittleFS reading buffer (JSONL)
  BirdySetup.*      — captive-portal provisioning AP (Birdy-Setup-<last 3 MAC bytes>)
  BirdyAPI.*        — WiFi + HTTP upload (single + batch)
  BirdyLED.*        — status LED incl. setup long–short–short pattern
  BirdyData.h       — shared data struct (IAQ, temp, humidity, pressure, CO2, VOC)
doc/
  setup-1.jpg       — wiring photo of the original Feather HUZZAH prototype
  circuit.md        — prototype wiring (historical; the PCB repo documents the board)
```

---

## Setup

### Prerequisites
- PlatformIO IDE extension (VS Code)
- No USB driver needed: the ESP32-C3 enumerates as a USB JTAG/serial device (VID 303A).

### WiFi / API (optional provisioning)

Credentials are **never compiled in**. Set the slide switch to SPARSE or
CONTINUOUS and power on (ideally on USB — a setup session costs ~15–20 mAh):

1. With no stored credentials the device starts the captive portal AP
   **`Birdy-Setup-<xxxxxx>`** (last 3 bytes of the MAC) and blinks *long–short–short*.
2. Join it with a phone, fill in SSID/password + API URL/key/Birdy ID, save —
   the device stores them in LittleFS `/config.json` and reboots into the
   selected mode.
3. Setup times out after ~3 min and returns to deep sleep if nobody configures.
4. To re-provision later: press **BOOT** within ~0.5 s after releasing RESET
   (forces setup mode even with credentials stored). Both buttons are inside the
   enclosure. Holding BOOT *while* releasing RESET enters download mode instead.

The `API_KEY` must be the **service_role** key from your Supabase project so the device can insert readings while RLS is enabled. API URL: `https://<<YourProjectRef>>.supabase.co/rest/v1/air_quality_data`.

Stored credentials survive normal firmware flashes; only a full flash erase wipes them.

### Battery voltage

Every wake also reads the battery through the VBAT_SENSE divider (R20/R21 → IO3, ADC1 at 2.5 dB) and uploads it as the nullable `battery` column (volts). **Apply dashboard migration `009_battery_voltage.sql` before flashing this firmware** — PostgREST rejects inserts with unknown columns.

### Build & Flash
```sh
pio run --target upload      # env "birdy"; "birdy_debug" adds verbose logging
pio device monitor
```

Flashing and the console run over the board's own USB-C port. The USB device
**disappears while the board is in deep sleep** (most of the time): flash right after
a reset or power-on, or put it into download mode (hold BOOT, tap RESET). The first
boot formats the LittleFS partition, so a freshly flashed board has no credentials
and enters setup mode in SPARSE/CONTINUOUS.

---

## References

- [Bosch BME688 Product Page](https://www.bosch-sensortec.com/en/products/environmental-sensors/gas-sensors/bme688)
- [Bosch BSEC2 Library — GitHub](https://github.com/boschsensortec/Bosch-BSEC2-Library)
- [ESP32-C3-WROOM-02 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3-wroom-02_datasheet_en.pdf)
- [air-quality-pcb](../air-quality-pcb) — board revisions, pin maps and design review (v9 = current)
- [Hackster.io — Visual CO₂ Indoor Air Quality Sensor](https://www.hackster.io/mcmchris/visual-co2-indoor-air-quality-sensor-509d4c)
- [DIY Air Quality Monitor](https://howtomechatronics.com/projects/diy-air-quality-monitor-pm2-5-co2-voc-ozone-temp-hum-arduino-meter/)
