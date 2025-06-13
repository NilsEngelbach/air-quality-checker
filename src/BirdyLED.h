#pragma once

#include <Arduino.h>
#include "BirdyData.h"

class BirdyLED
{
public:
    BirdyLED(uint8_t pin);
    void initialize();
    void update();
    void setAccuracy(uint8_t accuracy);

private:
    uint8_t pin;
    unsigned long lastBlinkTime;
    bool ledState;
    uint8_t accuracy;
    static const unsigned long BLINK_INTERVAL = 500; // Blink every 500ms
};