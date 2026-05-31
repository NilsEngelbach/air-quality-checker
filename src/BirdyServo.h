#pragma once

#include <Servo.h>

class BirdyServo
{
public:
    BirdyServo(uint8_t pin);

    // Restore last known band without moving the servo.
    void initialize(uint8_t lastBand);

    // Map iaqValue to a band (0–6). Moves only if the band changed.
    // Returns the current band so main.cpp can persist it to RTC memory.
    uint8_t setIaq(float iaqValue);

    // Stop PWM signal to cut idle current before deep sleep.
    void detach();

private:
    Servo   servo;
    uint8_t pin;
    uint8_t currentBand;

    static uint8_t iaqToBand(float iaq);
    static int     bandToAngle(uint8_t band);
};
