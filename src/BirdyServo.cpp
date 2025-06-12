#include "BirdyServo.h"

BirdyServo::BirdyServo(uint8_t pin)
    : pin(pin)
{
}

void BirdyServo::initialize()
{
    servo.attach(pin, 500, 2500);
    servo.write(0);
    delay(1000);
    servo.write(180);
    delay(1000);
    servo.write(0);
}

void BirdyServo::updatePosition(float iaqValue)
{
    // IAQ 0-50 excellent, 51-100 good
    if (iaqValue < 101)
    {
        servo.write(0);
        Serial.printf("Good: Moving to: %d°\n", 0);
    }
    // 101-150 moderate
    else if (iaqValue < 151)
    {
        int angle = map(iaqValue, 101, 150, 0, 135) + 45;
        servo.write(angle);
        Serial.printf("Moderate: Moving to: %d°\n", angle);
    }
    // > 151 poor
    else
    {
        servo.write(180);
        Serial.printf("Bad: Moving to: %d°\n", 180);
    }
}