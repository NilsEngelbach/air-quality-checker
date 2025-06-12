#include "BirdyServo.h"

BirdyServo::BirdyServo(uint8_t pin)
    : pin(pin)
{
}

void BirdyServo::initialize()
{
    servo.attach(pin);
    servo.write(0);
}

void BirdyServo::updatePosition(float iaqValue)
{
    // IAQ ranges from 0 (excellent) to 500 (hazardous)
    // Map to servo angles: 0° (excellent) to 180° (hazardous)
    int angle = map(iaqValue, 0, 500, 0, 180);
    Serial.printf("Moving to: %d°\n", angle);
    servo.write(angle);
}