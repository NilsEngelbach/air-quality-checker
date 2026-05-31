#pragma once

#include <Wire.h>
#include <bsec2.h>
#include <EEPROM.h>
#include "BirdyData.h"

using SensorCallback = void (*)(const BirdyData &);

class BirdySensor
{
public:
    BirdySensor(SensorCallback userCallback);

    // Pass savedState from RTC/EEPROM to restore calibration, or nullptr for cold start.
    void initialize(const uint8_t *savedState);

    // Call in loop(). Fires the callback once per measurement cycle.
    void update();

    // Copy current BSEC state blob into dst (must be BSEC_MAX_STATE_BLOB_SIZE bytes).
    bool getState(uint8_t *dst);

    // Write state blob to EEPROM as a fallback for power-loss recovery.
    bool saveStateToEeprom();

private:
    static BirdySensor *instance;
    Bsec2          sensor;
    SensorCallback userCallback;

    static void internalCallback(bme68xData data, bsecOutputs outputs, Bsec2 bsec);
    void        checkStatus();
    bool        loadStateFromEeprom();
    const char *accuracyLabel(uint8_t accuracy);
};
