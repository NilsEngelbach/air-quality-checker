#include "BirdyServo.h"
#include <Arduino.h>

// IAQ bands map to servo angles across the full 0–180° range.
// Band: 0=excellent(0-50), 1=good(51-100), 2=light(101-150),
//       3=moderate(151-200), 4=heavy(201-250), 5=severe(251-350), 6=extreme(>350)
static const int BAND_ANGLES[7] = { 0, 30, 60, 90, 120, 150, 180 };

BirdyServo::BirdyServo(uint8_t pin)
    : pin(pin), currentBand(0xFF)  // 0xFF = unknown, forces a move on first reading
{
}

void BirdyServo::initialize(uint8_t lastBand)
{
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
    Serial.printf("[Servo] band %d→%d  angle=%d°\n", currentBand, newBand, angle);

    servo.attach(pin, 500, 2500);
    servo.write(angle);
    delay(700);   // SG92R rated 0.1 s/60° at 4.8 V; 700 ms covers full 180°
    servo.detach();

    currentBand = newBand;
    return currentBand;
}

void BirdyServo::detach()
{
    servo.detach();
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
