#pragma once

#include <Arduino.h>

// Power sensing on the v9 board (ESP32-C3, ADC1 — eFuse-calibrated readings):
//   VBAT ── R20 1 M ──┬── R21 270 k ── GND        VBAT_SENSE → IO3 (ADC1_CH3), C17 100 nF
//                     4.2 V → 0.89 V, inside the 2.5 dB range (0–1.05 V)
//   VBUS ── R12 47.5 k ─┬── R13 22.1 k ── GND     VBUS_SENSE → IO1 (ADC1_CH1)
//                       5.0 V → 1.59 V, inside the 11 dB range (0–2.5 V)
//   charger STAT ── R31 10 k ─┬── R32 18 k ── GND CHG_DET → IO20 (digital)
//                             LOW = charging, HIGH = done; hi-Z (reads LOW) without VBUS
// The VBAT divider draws 3.3 µA continuously, so reading it costs nothing extra.
struct BirdyPower
{
    float vbat;      // battery voltage (V)
    bool  usb;       // VBUS present
    bool  charging;  // only meaningful when usb is true
};

class BirdyBattery
{
public:
    // VBAT / Vadc and VBUS / Vadc of the two dividers.
    static constexpr float VBAT_RATIO    = 1270.0f / 270.0f;
    static constexpr float VBUS_RATIO    = (47.5f + 22.1f) / 22.1f;
    static constexpr float USB_MIN_VOLTS = 3.8f;  // VBUS_SENSE ≈ 1.2 V

    // Average several VBAT samples: the divider's ~213 kΩ source impedance makes
    // single reads noisy. C17 (100 nF) covers the ADC sampling spike.
    // ~10 ms total — negligible energy per wake.
    static BirdyPower read(uint8_t vbatPin, uint8_t vbusPin, uint8_t chgPin,
                           uint8_t samples = 8)
    {
        analogSetPinAttenuation(vbatPin, ADC_2_5db);
        analogSetPinAttenuation(vbusPin, ADC_11db);
        pinMode(chgPin, INPUT);

        BirdyPower p;
        p.vbat     = readMilliVolts(vbatPin, samples) / 1000.0f * VBAT_RATIO;
        p.usb      = readMilliVolts(vbusPin, 1) / 1000.0f * VBUS_RATIO > USB_MIN_VOLTS;
        p.charging = p.usb && digitalRead(chgPin) == LOW;
        return p;
    }

private:
    static float readMilliVolts(uint8_t pin, uint8_t samples)
    {
        uint32_t acc = 0;
        for (uint8_t i = 0; i < samples; i++)
        {
            acc += analogReadMilliVolts(pin);
            delay(1);
        }
        return (float)acc / samples;
    }
};
