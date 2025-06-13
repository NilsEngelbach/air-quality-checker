#pragma once

#include <Servo.h>

class BirdyServo
{
public:
    BirdyServo(uint8_t pin);
    void initialize();
    void setIaq(float iaqValue);

private:
    Servo servo;
    uint8_t pin;
};