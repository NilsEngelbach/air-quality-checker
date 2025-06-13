#include "BirdyLED.h"

BirdyLED::BirdyLED(uint8_t pin)
    : pin(pin), lastBlinkTime(0), ledState(false)
{
}

void BirdyLED::initialize()
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}

void BirdyLED::setAccuracy(uint8_t accuracy)
{
    this->accuracy = accuracy;
}

void BirdyLED::update()
{
    // If accuracy is LOW or better, keep LED on
    if (this->accuracy >= BSEC_ACCURACY_LOW)
    {
        digitalWrite(pin, LOW);
        return;
    }

    // Otherwise, blink the LED
    unsigned long currentTime = millis();
    if (currentTime - this->lastBlinkTime >= BLINK_INTERVAL)
    {
        this->ledState = !this->ledState;
        digitalWrite(this->pin, this->ledState ? HIGH : LOW);
        this->lastBlinkTime = currentTime;
    }
}