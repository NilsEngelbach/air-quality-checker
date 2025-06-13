#pragma once

// Accuracy levels
#define BSEC_ACCURACY_UNRELIABLE 0
#define BSEC_ACCURACY_LOW 1
#define BSEC_ACCURACY_MEDIUM 2
#define BSEC_ACCURACY_HIGH 3

struct BirdyData
{
    float iaq;
    float co2;
    float voc;
    float temperature;
    float pressure;
    float humidity;
    uint8_t accuracy;
};