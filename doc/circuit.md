# Circuit Description

## Component Overview

| # | Component | Part |
|---|---|---|
| 1 | MCU | Adafruit Feather HUZZAH ESP8266 |
| 2 | Sensor | Adafruit BME688 STEMMA QT breakout |
| 3 | Actuator | SG92R micro servo |
| 4 | Power | Single-cell LiPo battery (3.7 V) |

---

## Critical Wiring: Deep Sleep Wake-up

**GPIO16 (D0) must be connected to RST.**

The ESP8266 exits deep sleep by pulsing RST low via GPIO16. Without this wire, `ESP.deepSleep()` puts the MCU to sleep permanently — it will never wake up.

- On the Huzzah, solder a short wire between the pad labeled **D0 / IO16** and the **RST** pad/pin, or bridge them with a 0 Ω resistor.
- Do **not** use GPIO16 for anything else in the firmware.

---

## BME688 STEMMA QT → Huzzah (I2C)

Connect via the JST SH 4-pin STEMMA QT cable:

| STEMMA QT | Wire Color | Huzzah Pin | Notes |
|---|---|---|---|
| VIN | Red | **3V** | 3.3 V LDO — sensor stays powered through deep sleep |
| GND | Black | GND | Common ground |
| SDA | Blue | SDA (GPIO 4) | I2C data |
| SCL | Yellow | SCL (GPIO 5) | I2C clock |

The Adafruit BME688 breakout includes 10 kΩ pull-up resistors on SDA and SCL. No external pull-ups needed.

**Why 3V instead of GPIO 12?**
The BME688 gas sensor needs to stay physically stabilized between wakes for BSEC to recover its calibrated accuracy quickly. Cutting power via GPIO 12 causes a full cold start of the gas heater on every wake, and BSEC may take many LP cycles (potentially never within a single 290 s wake window) to restore accuracy=3. Keeping VIN on the 3.3 V rail means the sensor substrate stays at ambient temperature and BSEC re-anchors within 1–2 measurements.

Trade-off: the breakout's power-indicator LED draws ~1.5 mA continuously, including during the 290 s sleep window (~121 µAh/cycle). This roughly halves battery life compared to the GPIO 12 approach.

Default I2C address: **0x77**. To use 0x76, close the SDO solder jumper on the breakout.

---

## SG92R Servo → Huzzah

| Servo Wire | Color | Connect To | Notes |
|---|---|---|---|
| Signal | Orange / Yellow | GPIO 13 | PWM control, 3.3 V logic |
| Power | Red | VBAT | See power note below |
| GND | Brown / Black | GND | Common ground |

**Power note:** The SG92R is rated for 4.8–6 V. The Huzzah's **VBAT** pin outputs the raw LiPo voltage (~3.7–4.2 V when on battery, ~5 V via USB). At 3.7 V the servo operates with slightly reduced torque, which is acceptable for this application. If torque is insufficient, add a small 3.3 V → 5 V boost converter (e.g., PAM2401) between VBAT and the servo power lead.

**Decoupling:** Place a **100 µF electrolytic capacitor** between VBAT and GND close to the servo connector to absorb current spikes during movement. Without it, servo transients can brown-out the MCU.

**Optional protection:** A 100 Ω resistor in series with the signal wire protects the GPIO from short-circuit current.

---

## Power Architecture

```
LiPo battery (3.7 V)
    ↓ JST connector
Huzzah built-in charger (MCP73831)
    ↓ charges battery when USB is connected
Huzzah 3.3 V LDO (AP2112K)
    ├─→ ESP8266 VCC
    ├─→ BME688 VIN (3V pin — always on, including during deep sleep)
    └─→ Servo signal line (GPIO 13 logic level)

Huzzah VBAT pin (unregulated battery rail)
    └─→ Servo power (red wire)
```

---

## Deep Sleep Current Budget (estimate)

| Phase | Duration / cycle | Current | Charge / cycle |
|---|---|---|---|
| Boot + BSEC restore | ~1 s | ~80 mA | ~22 µAh |
| Gas heater active | ~2 s | ~85 mA | ~47 µAh |
| Servo move (on band change) | ~0.7 s (occasional) | ~200 mA | ~39 µAh (amortized) |
| Deep sleep (BME688 LED on, sensor idle) | ~290 s | ~1.52 mA | ~123 µAh |
| **Total per cycle** | **~300 s** | | **~192 µAh** |

**Estimated battery life (2000 mAh LiPo, no WiFi):**
~2000 mAh / 2.3 mA avg ≈ **~13 days**.

> Wiring VIN to GPIO 12 instead of 3V cuts deep-sleep current to ~20 µA (LED off, sensor off), extending battery life to ~28 days. However, this causes a cold gas-heater start on every wake, preventing BSEC from recovering its calibrated accuracy within a single measurement cycle.

WiFi upload on every boot reduces this to roughly **3–5 days** depending on connection time.

---

## Pinout Reference

See [Huzzah ESP8266 pinout diagram](adafruit_products_Huzzah_ESP8266_Pinout_v1.2-1.png) in this folder.

| Huzzah Pin | Function in this project |
|---|---|
| 3V | BME688 VIN (always on) |
| GND | Common ground |
| SDA (GPIO 4) | BME688 I2C data |
| SCL (GPIO 5) | BME688 I2C clock |
| GPIO 13 | Servo signal |
| GPIO 2 | Built-in LED (active LOW) |
| GPIO 12 | (unused) |
| GPIO 16 (D0) | → RST (deep sleep wakeup) |
| VBAT | Servo power |
| RST | ← GPIO 16 (deep sleep wakeup) |
