#include "BirdyLED.h"

// Active-LOW LED (v9: IO8, STATUS_LED_N): LOW = on, HIGH = off.
static const bool LED_ON  = LOW;
static const bool LED_OFF = HIGH;

// Setup pattern "long – short – short": ON durations per even step,
// OFF durations per odd step. Total cycle 2400 ms.
static const uint16_t SETUP_ON_MS[3]  = { 600, 200, 200 };
static const uint16_t SETUP_OFF_MS[3] = { 200, 200, 1000 };

BirdyLED::BirdyLED(uint8_t pin)
    : pin(pin), lastBlinkTime(0), ledState(false), accuracy(0)
{
}

void BirdyLED::initialize()
{
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LED_OFF);
}

void BirdyLED::setAccuracy(uint8_t accuracy)
{
    this->accuracy = accuracy;
}

void BirdyLED::setSetupPattern(bool enabled)
{
    if (enabled && !setupPattern)
    {
        patternStep      = 0;
        patternStepStart = millis();
        digitalWrite(pin, LED_ON);  // pattern starts with the long ON
    }
    setupPattern = enabled;
}

void BirdyLED::off()
{
    digitalWrite(pin, LED_OFF);
}

void BirdyLED::flash(uint16_t ms)
{
    digitalWrite(pin, LED_ON);
    delay(ms);
    digitalWrite(pin, LED_OFF);
}

void BirdyLED::update()
{
    if (setupPattern)
    {
        // Steps 0..5: ON,OFF,ON,OFF,ON,OFF
        bool     onPhase  = (patternStep % 2 == 0);
        uint16_t duration = onPhase ? SETUP_ON_MS[patternStep / 2]
                                    : SETUP_OFF_MS[patternStep / 2];
        if (millis() - patternStepStart >= duration)
        {
            patternStep = (patternStep + 1) % 6;
            patternStepStart = millis();
            digitalWrite(pin, (patternStep % 2 == 0) ? LED_ON : LED_OFF);
        }
        return;
    }

    // If accuracy is LOW or better, keep LED on
    if (this->accuracy >= BSEC_ACCURACY_LOW)
    {
        digitalWrite(pin, LED_ON);
        return;
    }

    // Otherwise, blink the LED
    unsigned long currentTime = millis();
    if (currentTime - this->lastBlinkTime >= BLINK_INTERVAL)
    {
        this->ledState = !this->ledState;
        digitalWrite(this->pin, this->ledState ? LED_OFF : LED_ON);
        this->lastBlinkTime = currentTime;
    }
}
