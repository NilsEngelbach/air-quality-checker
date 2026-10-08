#pragma once

#include <ESP32Servo.h>

class BirdyServo
{
public:
    // enablePin = SERVO_EN (IO10 on v9) — switches the servo rail (Q7 → Q6,
    // SERVO_PWR from VSYS). Held LOW at all times except around an actual
    // move, so the servo draws nothing in deep sleep (R4 pulls it low too).
    BirdyServo(uint8_t pin, uint8_t enablePin);

    // Restore last known band without moving the servo. Drives SERVO_EN and
    // the PWM pin LOW.
    void initialize(uint8_t lastBand);

    // Map iaqValue to a band (0–6). Moves only if the band changed.
    // Returns the current band so main.cpp can persist it to RTC memory.
    uint8_t setIaq(float iaqValue);

    // Stop PWM signal to cut idle current before deep sleep; also guarantees
    // SERVO_EN is LOW across the sleep (firmware contract).
    void detach();

private:
    Servo   servo;
    uint8_t pin;
    uint8_t enablePin;
    uint8_t currentBand;

    void releasePwmPin();

    static uint8_t iaqToBand(float iaq);
    static int     bandToAngle(uint8_t band);
};
