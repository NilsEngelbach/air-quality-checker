#pragma once

#include <Arduino.h>

// WiFi operating mode, selected by the SW4 slide switch (v6+ hardware).
// OFF        — never touch WiFi (lowest power)
// SPARSE     — buffer readings in LittleFS, upload a batch every N boots
// CONTINUOUS — upload every sample
enum class BirdyWifiMode : uint8_t
{
    OFF = 0,
    SPARSE,
    CONTINUOUS,
};

class BirdyMode
{
public:
    // Read once at boot. MODE_A/MODE_B are sampled with the internal pull-ups
    // (SW4 shorts one of them to GND, or neither in the middle position), then
    // released to Hi-Z so a closed contact draws nothing for the rest of the wake.
    //
    // SW4 (OS103011MA7QP1, common pin 2 = GND), v9 pins:
    //   POS.1: MODE_A (IO4) LOW -> OFF
    //   POS.2: both HIGH        -> SPARSE
    //   POS.3: MODE_B (IO5) LOW -> CONTINUOUS
    static BirdyWifiMode read(uint8_t pinOff, uint8_t pinContinuous)
    {
        pinMode(pinOff, INPUT_PULLUP);
        pinMode(pinContinuous, INPUT_PULLUP);
        delayMicroseconds(200);  // let the weak pull-ups settle

        bool offGrounded  = (digitalRead(pinOff) == LOW);
        bool contGrounded = (digitalRead(pinContinuous) == LOW);

        pinMode(pinOff, INPUT);
        pinMode(pinContinuous, INPUT);

        if (offGrounded)  return BirdyWifiMode::OFF;
        if (contGrounded) return BirdyWifiMode::CONTINUOUS;
        return BirdyWifiMode::SPARSE;
    }

    static const char *label(BirdyWifiMode mode)
    {
        switch (mode)
        {
        case BirdyWifiMode::OFF:        return "OFF";
        case BirdyWifiMode::SPARSE:     return "SPARSE";
        case BirdyWifiMode::CONTINUOUS: return "CONTINUOUS";
        default:                        return "?";
        }
    }
};
