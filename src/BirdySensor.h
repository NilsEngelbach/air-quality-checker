#pragma once

#include <Wire.h>
#include <bsec2.h>
#include <EEPROM.h>
#include "BirdyData.h"

#define STATE_SAVE_INTERVAL (5 * 60 * 1000) // Persist state every 5 minutes

using SensorCallback = void (*)(const BirdyData &);

class BirdySensor
{
public:
    BirdySensor(SensorCallback userCallback);
    void initialize();
    void update();

private:
    static BirdySensor *instance;
    Bsec2 sensor;
    uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
    const uint8_t *bsecConfig;
    uint8_t lastAccuracy;
    long lastStateSave;
    SensorCallback userCallback;

    static void internalCallback(bme68xData data, bsecOutputs outputs, Bsec2 bsec);
    const char *getAccuracyString(uint8_t accuracy);
    void checkStatus();
    bool loadState();
    bool saveState();
    void persistState(bsecData output);
};
