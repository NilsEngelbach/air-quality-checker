#include "BirdyServo.h"
#include <Arduino.h>

// IAQ bands map to servo angles across the full 0–180° range.
// Band: 0=excellent(0-50), 1=good(51-100), 2=light(101-150),
//       3=moderate(151-200), 4=heavy(201-250), 5=severe(251-350), 6=extreme(>350)
static const int BAND_ANGLES[7] = { 0, 30, 60, 90, 120, 150, 180 };

// SERVO_PWR ramps through Q6's soft start (~0.5 V/ms, ≈ 8 ms to VSYS); the
// board contract is ≥ 50 ms between SERVO_EN HIGH and the first PWM pulse.
#define SERVO_SETTLE_MS 100

BirdyServo::BirdyServo(uint8_t pin, uint8_t enablePin)
    : pin(pin), enablePin(enablePin), currentBand(0xFF)  // 0xFF = unknown, forces a move on first reading
{
}

void BirdyServo::initialize(uint8_t lastBand)
{
    // R4 holds SERVO_EN LOW at boot; drive it LOW explicitly as early as
    // possible so the rail can never be on outside a move.
    pinMode(enablePin, OUTPUT);
    digitalWrite(enablePin, LOW);
    releasePwmPin();

    currentBand = lastBand;
    // Do not attach or move; the servo holds its mechanical position.
    // Movement only happens in setIaq() when the band changes.
}

uint8_t BirdyServo::setIaq(float iaqValue)
{
    uint8_t newBand = iaqToBand(iaqValue);
    if (newBand == currentBand)
        return currentBand;

    int angle = bandToAngle(newBand);
    Serial.printf("[Servo] band %d→%d  angle=%d°  (rail on)\n", currentBand, newBand, angle);

    digitalWrite(enablePin, HIGH);   // SERVO_EN: Q6 connects SERVO_PWR to VSYS
    delay(SERVO_SETTLE_MS);

    servo.attach(pin, 500, 2500);
    servo.write(angle);
    delay(700);   // SG92R rated 0.1 s/60° at 4.8 V; 700 ms covers full 180° (slower on battery)
    servo.detach();
    releasePwmPin();                 // no PWM while the rail is off (R17 back-feed)

    digitalWrite(enablePin, LOW);    // rail back to 0 µA
    Serial.println("[Servo] move done (rail off)");

    currentBand = newBand;
    return currentBand;
}

void BirdyServo::detach()
{
    servo.detach();
    releasePwmPin();
    digitalWrite(enablePin, LOW);  // never HIGH across a deep sleep
}

void BirdyServo::releasePwmPin()
{
    // Park the signal LOW: a HIGH level into the unpowered servo would
    // back-feed it through R17.
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

uint8_t BirdyServo::iaqToBand(float iaq)
{
    if (iaq <= 50)  return 0;
    if (iaq <= 100) return 1;
    if (iaq <= 150) return 2;
    if (iaq <= 200) return 3;
    if (iaq <= 250) return 4;
    if (iaq <= 350) return 5;
    return 6;
}

int BirdyServo::bandToAngle(uint8_t band)
{
    if (band >= 7) band = 6;
    return BAND_ANGLES[band];
}
