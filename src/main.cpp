#include <Arduino.h>
#include <umm_malloc/umm_heap_select.h>
#include <Wire.h>
#include <bsec.h>

#include "secrets.h"

// Functions
void checkSensorStatus(void);

// Global variables
Bsec bme;
String output;

void setup() {
    Serial.begin(115200);
    Wire.begin();

    Serial.println("Starting...");

    bme.begin(BME68X_I2C_ADDR_HIGH, Wire);
    output = "\nBSEC library version " + String(bme.version.major) + "." + String(bme.version.minor) + "." + String(bme.version.major_bugfix) + "." + String(bme.version.minor_bugfix);
    Serial.println(output);
    checkSensorStatus();

    bsec_virtual_sensor_t sensorList[13] = {
        BSEC_OUTPUT_IAQ,
        BSEC_OUTPUT_STATIC_IAQ,
        BSEC_OUTPUT_CO2_EQUIVALENT,
        BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
        BSEC_OUTPUT_RAW_TEMPERATURE,
        BSEC_OUTPUT_RAW_PRESSURE,
        BSEC_OUTPUT_RAW_HUMIDITY,
        BSEC_OUTPUT_RAW_GAS,
        BSEC_OUTPUT_STABILIZATION_STATUS,
        BSEC_OUTPUT_RUN_IN_STATUS,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
        BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
        BSEC_OUTPUT_GAS_PERCENTAGE
    };

    bme.updateSubscription(sensorList, 13, BSEC_SAMPLE_RATE_LP);
    checkSensorStatus();

    // Print the header
    output = "Timestamp, IAQ, IAQ accuracy, Static IAQ, CO2 equivalent, breath VOC equivalent, raw temp[°C], pressure [Pa], raw relative humidity [%], gas [Ohm], Stab Status, run in status, comp temp[°C], comp humidity [%], gas percentage";
    Serial.println(output);
}

void loop(void)
{
  unsigned long time_trigger = millis();
  if (bme.run()) {
    output = String(time_trigger);
    output += ", " + String(bme.iaq);
    output += ", " + String(bme.iaqAccuracy);
    output += ", " + String(bme.staticIaq);
    output += ", " + String(bme.co2Equivalent);
    output += ", " + String(bme.breathVocEquivalent);
    output += ", " + String(bme.rawTemperature);
    output += ", " + String(bme.pressure);
    output += ", " + String(bme.rawHumidity);
    output += ", " + String(bme.gasResistance);
    output += ", " + String(bme.stabStatus);
    output += ", " + String(bme.runInStatus);
    output += ", " + String(bme.temperature);
    output += ", " + String(bme.humidity);
    output += ", " + String(bme.gasPercentage);
    Serial.println(output);
  } else {
    checkSensorStatus();
  }
}


void checkSensorStatus(void)
{
  if (bme.bsecStatus != BSEC_OK) {
    if (bme.bsecStatus < BSEC_OK) {
      output = "BSEC error code : " + String(bme.bsecStatus);
      Serial.println(output);
    } else {
      output = "BSEC warning code : " + String(bme.bsecStatus);
      Serial.println(output);
    }
  }

  if (bme.bme68xStatus != BME68X_OK) {
    if (bme.bme68xStatus < BME68X_OK) {
      output = "BME68X error code : " + String(bme.bme68xStatus);
      Serial.println(output);
    } else {
      output = "BME68X warning code : " + String(bme.bme68xStatus);
      Serial.println(output);
    }
  }
}