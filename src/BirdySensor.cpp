#include "BirdySensor.h"

// ULP config (300 s) is required for deep-sleep duty cycling. LP (3 s) is
// incompatible because BSEC anchors its gas-resistance baseline around a
// continuous 3 s heater cadence — after 290 s of sleep the heater is cold,
// the baseline has drifted, and BSEC resets accuracy to 0 every wake.
// ULP explicitly models the ~300 s gap; accuracy survives sleep with state restore.
static const uint8_t bsecConfig[] = {
#include "config/bme688/bme688_sel_33v_300s_4d/bsec_selectivity.txt"
};

BirdySensor *BirdySensor::instance = nullptr;

BirdySensor::BirdySensor(SensorCallback userCallback)
    : sensor(), userCallback(userCallback)
{
    instance = this;
}

void BirdySensor::initialize(const uint8_t *savedState)
{
    Serial.println("[Sensor] init");

    Wire.beginTransmission(BME68X_I2C_ADDR_HIGH);
    if (Wire.endTransmission() != 0)
    {
        Serial.println("[Sensor] ERROR: not found on I2C 0x77");
        return;
    }

    if (!sensor.begin(BME68X_I2C_ADDR_HIGH, Wire))
    {
        checkStatus();
        return;
    }

    if (!sensor.setConfig(bsecConfig))
    {
        checkStatus();
        return;
    }

    // Restore calibration state. Priority: RTC (passed in) → EEPROM → cold start.
    if (savedState)
    {
        uint8_t mutable_state[BSEC_MAX_STATE_BLOB_SIZE];
        memcpy(mutable_state, savedState, BSEC_MAX_STATE_BLOB_SIZE);
        if (!sensor.setState(mutable_state))
        {
            Serial.println("[Sensor] RTC state rejected, trying EEPROM");
            loadStateFromEeprom();
        }
        else
        {
            Serial.println("[Sensor] calibration restored from RTC");
        }
    }
    else
    {
        if (loadStateFromEeprom())
            Serial.println("[Sensor] calibration restored from EEPROM");
        else
            Serial.println("[Sensor] cold start — 48 h burn-in needed for accuracy=3");
    }

    bsecSensor sensorList[] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
    };

    if (!sensor.updateSubscription(sensorList, 6, BSEC_SAMPLE_RATE_ULP))
    {
        checkStatus();
        return;
    }

    sensor.attachCallback(internalCallback);
    Serial.println("[Sensor] ready");
}

void BirdySensor::update()
{
    if (!sensor.run())
        checkStatus();
}

bool BirdySensor::getState(uint8_t *dst)
{
    return sensor.getState(dst);
}

bool BirdySensor::saveStateToEeprom()
{
    uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
    if (!sensor.getState(state))
        return false;

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
        EEPROM.write(i + 1, state[i]);
    EEPROM.commit();
    Serial.println("[Sensor] state written to EEPROM");
    return true;
}

bool BirdySensor::loadStateFromEeprom()
{
    if (EEPROM.read(0) != BSEC_MAX_STATE_BLOB_SIZE)
        return false;

    uint8_t state[BSEC_MAX_STATE_BLOB_SIZE];
    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
        state[i] = EEPROM.read(i + 1);

    return sensor.setState(state);
}

void BirdySensor::internalCallback(bme68xData data, bsecOutputs outputs, Bsec2 bsec)
{
    if (!outputs.nOutputs || !instance)
        return;

    BirdyData current = {};

    Serial.printf("[Sensor] t=%lld ms\n", outputs.output[0].time_stamp / 1000000LL);
    for (uint8_t i = 0; i < outputs.nOutputs; i++)
    {
        const bsecData &o = outputs.output[i];
        switch (o.sensor_id)
        {
        case BSEC_OUTPUT_IAQ:
            Serial.printf("  IAQ: %.1f (%s)\n", o.signal, instance->accuracyLabel(o.accuracy));
            current.iaq      = o.signal;
            current.accuracy = o.accuracy;
            break;
        case BSEC_OUTPUT_CO2_EQUIVALENT:
            Serial.printf("  eCO2: %.0f ppm\n", o.signal);
            current.co2 = o.signal;
            break;
        case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
            Serial.printf("  bVOC: %.2f ppm\n", o.signal);
            current.voc = o.signal;
            break;
        case BSEC_OUTPUT_RAW_TEMPERATURE:
            Serial.printf("  Temp: %.1f C\n", o.signal);
            current.temperature = o.signal;
            break;
        case BSEC_OUTPUT_RAW_PRESSURE:
            Serial.printf("  Pres: %.1f hPa\n", o.signal);
            current.pressure = o.signal;
            break;
        case BSEC_OUTPUT_RAW_HUMIDITY:
            Serial.printf("  Hum:  %.1f %%\n", o.signal);
            current.humidity = o.signal;
            break;
        default:
            break;
        }
    }

    if (instance->userCallback)
        instance->userCallback(current);
}

void BirdySensor::checkStatus()
{
    if (sensor.status != BSEC_OK)
        Serial.printf("[Sensor] BSEC status: %d\n", sensor.status);
    if (sensor.sensor.status != BME68X_OK)
        Serial.printf("[Sensor] BME68x status: %d\n", sensor.sensor.status);
}

const char *BirdySensor::accuracyLabel(uint8_t accuracy)
{
    switch (accuracy)
    {
    case 0: return "stabilizing";
    case 1: return "uncertain";
    case 2: return "calibrating";
    case 3: return "calibrated";
    default: return "unknown";
    }
}
