#pragma once

#include <Wire.h>
#include <bsec2.h>
#include <bme68xLibrary.h>
#include <EEPROM.h>
#include "BirdyData.h"

using SensorCallback = void (*)(const BirdyData &);

class BirdySensor
{
public:
    BirdySensor(SensorCallback userCallback);

    // Pass savedState from RTC/EEPROM to restore calibration, or nullptr for cold start.
    // timeOffsetMs is the accumulated wall-clock time across deep-sleep resets. BSEC's
    // 20-min ULP calibration window is measured via its millis() callback; Arduino's
    // millis() resets to 0 on every wake, so without this offset the window never
    // closes and accuracy stays at 0 forever.
    void initialize(const uint8_t *savedState, int64_t timeOffsetMs = 0);

    // Call in loop(). Fires the callback once per measurement cycle.
    void update();

    // Copy current BSEC state blob into dst (must be BSEC_MAX_STATE_BLOB_SIZE bytes).
    bool getState(uint8_t *dst);

    // Write state blob to EEPROM as a fallback for power-loss recovery.
    bool saveStateToEeprom();

private:
    static BirdySensor  *instance;
    static int64_t       baseTimeOffsetMs;
    static unsigned long bsecMillisCallback();

    Bsec2          sensor;
    bme68xScommT   commIntf;
    SensorCallback userCallback;

    static void internalCallback(bme68xData data, bsecOutputs outputs, Bsec2 bsec);
    void        checkStatus();
    bool        loadStateFromEeprom();
    const char *accuracyLabel(uint8_t accuracy);
};
