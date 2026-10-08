#pragma once

#include <Arduino.h>
#include "BirdyData.h"

#define BLINK_INTERVAL 500

class BirdyLED
{
public:
    BirdyLED(uint8_t pin);
    void initialize();
    void update();
    void setAccuracy(uint8_t accuracy);
    void off();

    // Single blocking flash (setup() only — never from loop()).
    void flash(uint16_t ms);

    // Setup-mode (captive portal) pattern: long–short–short
    // (600 ON, 200 OFF, 200 ON, 200 OFF, 200 ON, 1000 OFF, repeat).
    // While enabled it overrides the normal accuracy behavior.
    void setSetupPattern(bool enabled);

private:
    uint8_t pin;
    unsigned long lastBlinkTime;
    bool ledState;
    uint8_t accuracy;

    bool     setupPattern = false;
    uint8_t  patternStep  = 0;
    unsigned long patternStepStart = 0;
};
