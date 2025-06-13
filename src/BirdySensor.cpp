#include "BirdySensor.h"

static const uint8_t config[] = {
#include "config/bme688/bme688_sel_33v_3s_4d/bsec_selectivity.txt"
};

BirdySensor *BirdySensor::instance = nullptr;

BirdySensor::BirdySensor(SensorCallback userCallback) : sensor(),
                                                        bsecState{},
                                                        bsecConfig(config),
                                                        lastAccuracy(0),
                                                        lastStateSave(0),
                                                        userCallback(userCallback)
{
    instance = this;
}

void BirdySensor::initialize()
{
    Serial.println("\n\t[1/5] Starting communication with Sensor");

    Wire.beginTransmission(BME68X_I2C_ADDR_HIGH);
    byte error = Wire.endTransmission();
    if (error != 0)
    {
        Serial.println("\t\tError: Could not find sensor!");
        Serial.println("\t\tError code: " + String(error));
        return;
    }

    Serial.println("\t\tSensor found!");
    if (!sensor.begin(BME68X_I2C_ADDR_HIGH, Wire))
    {
        checkStatus();
    }

    Serial.println("\n\t[2/5] Set configuration");
    if (!sensor.setConfig(this->bsecConfig))
    {
        checkStatus();
    }

    Serial.println("\n\t[3/5] Load state");
    if (!this->loadState())
    {
        checkStatus();
    }

    bsecSensor sensorList[] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY};

    Serial.println("\n\t[4/5] Update subscription");
    if (!this->sensor.updateSubscription(sensorList, 6, BSEC_SAMPLE_RATE_LP))
    {
        checkStatus();
    }

    Serial.println("\n\t[5/5] Attach callback");
    this->sensor.attachCallback(this->internalCallback);
}

void BirdySensor::update()
{
    if (!this->sensor.run())
    {
        checkStatus();
    }
}

void BirdySensor::internalCallback(bme68xData data, bsecOutputs outputs, Bsec2 bsec)
{
    if (!outputs.nOutputs || !instance)
    {
        return;
    }

    BirdyData currentData;

    Serial.println("\nData @ " + String((int)(outputs.output[0].time_stamp / INT64_C(1000000))));
    for (uint8_t i = 0; i < outputs.nOutputs; i++)
    {
        const bsecData output = outputs.output[i];
        switch (output.sensor_id)
        {
        case BSEC_OUTPUT_IAQ:
            Serial.printf("\tIAQ: %.2f (Accuracy: %s)\n", output.signal, instance->getAccuracyString(output.accuracy));
            currentData.iaq = output.signal;
            currentData.accuracy = output.accuracy;
            instance->persistState(output);
            break;
        case BSEC_OUTPUT_CO2_EQUIVALENT:
            Serial.printf("\tCO2 equivalent: %.2f ppm\n", output.signal);
            currentData.co2 = output.signal;
            break;
        case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
            Serial.printf("\tVOC equivalent: %.2f ppm\n", output.signal);
            currentData.voc = output.signal;
            break;
        case BSEC_OUTPUT_RAW_TEMPERATURE:
            Serial.printf("\tTemperature: %.2f °C\n", output.signal);
            currentData.temperature = output.signal;
            break;
        case BSEC_OUTPUT_RAW_PRESSURE:
            Serial.printf("\tPressure: %.2f hPa\n", output.signal);
            currentData.pressure = output.signal;
            break;
        case BSEC_OUTPUT_RAW_HUMIDITY:
            Serial.printf("\tHumidity: %.2f %%\n", output.signal);
            currentData.humidity = output.signal;
            break;
        default:
            break;
        }
    }

    if (instance->userCallback)
    {
        instance->userCallback(currentData);
    }
}

const char *BirdySensor::getAccuracyString(uint8_t accuracy)
{
    switch (accuracy)
    {
    case BSEC_ACCURACY_UNRELIABLE:
        return "Unreliable";
    case BSEC_ACCURACY_LOW:
        return "Low";
    case BSEC_ACCURACY_MEDIUM:
        return "Medium";
    case BSEC_ACCURACY_HIGH:
        return "High";
    default:
        return "Unknown";
    }
}

void BirdySensor::checkStatus()
{
    if (this->sensor.status < BSEC_OK)
    {
        Serial.println("BSEC error code : " + String(this->sensor.status));
    }
    else if (this->sensor.status > BSEC_OK)
    {
        Serial.println("BSEC warning code : " + String(this->sensor.status));
    }

    if (this->sensor.sensor.status < BME68X_OK)
    {
        Serial.println("BME68X error code : " + String(this->sensor.sensor.status));
    }
    else if (this->sensor.sensor.status > BME68X_OK)
    {
        Serial.println("BME68X warning code : " + String(this->sensor.sensor.status));
    }
}

bool BirdySensor::loadState()
{
    uint8_t size = EEPROM.read(0);
    Serial.println("EEPROM Size: [" + String(size) + "|" + String(BSEC_MAX_STATE_BLOB_SIZE) + "]");
    if (size != BSEC_MAX_STATE_BLOB_SIZE)
    {
        Serial.println("No state available");
        return false;
    }

    Serial.print("State file: ");
    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
    {
        bsecState[i] = EEPROM.read(i + 1);
        Serial.print(String(bsecState[i], HEX) + ", ");
    }
    Serial.println();

    return this->sensor.setState(this->bsecState);
}

bool BirdySensor::saveState()
{
    if (!this->sensor.getState(this->bsecState))
        return false;

    Serial.println("Writing state to EEPROM");
    Serial.print("State file: ");

    for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
    {
        EEPROM.write(i + 1, this->bsecState[i]);
        Serial.print(String(this->bsecState[i], HEX) + ", ");
    }
    Serial.println();

    EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
    EEPROM.commit();
    return true;
}

void BirdySensor::persistState(bsecData output)
{
    if (output.accuracy > this->lastAccuracy || (millis() - this->lastStateSave >= STATE_SAVE_INTERVAL))
    {
        if (saveState())
        {
            Serial.println("State saved due to " + String(output.accuracy > this->lastAccuracy ? "improved accuracy" : "time interval"));
            this->lastAccuracy = output.accuracy;
            this->lastStateSave = millis();
        }
    }
}