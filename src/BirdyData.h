#pragma once

#include <ArduinoJson.h>

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