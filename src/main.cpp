#include <Arduino.h>
#include <umm_malloc/umm_heap_select.h>
#include <Wire.h>
#include <bsec.h>

/* Configure the BSEC library with information about the sensor
    18v/33v = Voltage at Vdd. 1.8V or 3.3V
      => We have 33v
    3s/300s = BSEC operating mode, BSEC_SAMPLE_RATE_LP or BSEC_SAMPLE_RATE_ULP
      => we want ULP because we are battery powered
    4d/28d = Operating age of the sensor in days
      => currently 4d (2025-02-05)
    generic_18v_3s_4d
    generic_18v_3s_28d
    generic_18v_300s_4d
    generic_18v_300s_28d
    generic_33v_3s_4d
    generic_33v_3s_28d
    generic_33v_300s_4d
    generic_33v_300s_28d
*/
const uint8_t bsec_config_iaq[] = {
#include "config/generic_33v_3s_4d/bsec_iaq.txt"
};

#include "secrets.h"
#include <sys/time.h>

// Constants
#define DEV_MODE // uncomment this for debug / dev to have serial + logs

// Macros
#ifdef DEV_MODE
#define LOG(fmt, ...) (Serial.printf("%09llu: " fmt "\n", GetTimestamp(), ##__VA_ARGS__))
#else
#define LOG(fmt, ...)
#endif

// Function headers
bool CheckAndGetSensorStatus(void);
int64_t GetTimestamp();
void EnterErrorLEDStateForever(void);

// Global variables
Bsec airSensor;

String output;

bsec_virtual_sensor_t sensor_list[13] = {
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
    BSEC_OUTPUT_GAS_PERCENTAGE,
};

// Arduino hooks
void setup()
{
#ifdef DEV_MODE
  Serial.begin(115200);

  while (!Serial)
    delay(10); // wait for console to see every log- only for development
#endif

  pinMode(LED_BUILTIN, OUTPUT); // configure LED at 0 (RED) for output usage
  digitalWrite(LED_BUILTIN, HIGH);

  LOG("Starting...");

  LOG("Init BME680 sensor");
  Wire.begin();
  airSensor.begin(BME68X_I2C_ADDR_HIGH, Wire);
  if (!CheckAndGetSensorStatus())
  {
    LOG("Failed to init BME680, check wiring!");
    EnterErrorLEDStateForever();
  }

  LOG("BSEC version %d.%d.%d.%d", airSensor.version.major, airSensor.version.minor, airSensor.version.major_bugfix, airSensor.version.minor_bugfix);

  LOG("Set config for BME680 sensor");
  airSensor.setConfig(bsec_config_iaq);
  if (!CheckAndGetSensorStatus())
  {
    LOG("Failed to set config!");
    EnterErrorLEDStateForever();
  }

  // TODO: How to load the state of the sensor?
  // https://github.com/boschsensortec/BSEC-Arduino-library/blob/master/examples/esp32DeepSleep/esp32DeepSleep.ino#L90
  // How to write / read to/from flash?
  // Maybe this? https://github.com/esp8266/Arduino/blob/master/libraries/EEPROM/examples/eeprom_write/eeprom_write.ino
  // Or this? https://arduino-esp8266.readthedocs.io/en/3.1.2/filesystem.html
  // or this? https://blog.hirnschall.net/accept-cookies/?ref=/esp8266-eeprom/&oref=https://www.bing.com/

  LOG("Subscribe to %d elements in sensor_list", sizeof(sensor_list) / sizeof(sensor_list[0])); // overall size in bytes / element size = elements
  airSensor.updateSubscription(sensor_list, 13, BSEC_SAMPLE_RATE_LP);
  CheckAndGetSensorStatus();

  LOG("Setup done - going over to loop!");

  // Print the header
  output = "Timestamp, IAQ, IAQ accuracy, Static IAQ, CO2 equivalent, breath VOC equivalent, raw temp[°C], pressure [Pa], raw relative humidity [%], gas [Ohm], Stab Status, run in status, comp temp[°C], comp humidity [%], gas percentage";
  Serial.println(output);
}

void loop(void)
{
  unsigned long time_trigger = millis();
  if (airSensor.run())
  {
    output = String(time_trigger);
    output += ", " + String(airSensor.iaq);
    output += ", " + String(airSensor.iaqAccuracy);
    output += ", " + String(airSensor.staticIaq);
    output += ", " + String(airSensor.co2Equivalent);
    output += ", " + String(airSensor.breathVocEquivalent);
    output += ", " + String(airSensor.rawTemperature);
    output += ", " + String(airSensor.pressure);
    output += ", " + String(airSensor.rawHumidity);
    output += ", " + String(airSensor.gasResistance);
    output += ", " + String(airSensor.stabStatus);
    output += ", " + String(airSensor.runInStatus);
    output += ", " + String(airSensor.temperature);
    output += ", " + String(airSensor.humidity);
    output += ", " + String(airSensor.gasPercentage);
    Serial.println(output);

    LOG("Temperature raw %.2f°C compensated %.2f°C", airSensor.rawTemperature, airSensor.temperature);
    LOG("Humidity raw %.2f%% compensated %.2f%%", airSensor.rawHumidity, airSensor.humidity);
    LOG("Pressure %.2f kPa", airSensor.pressure / 1000);
    LOG("Gas resistance %.2f kOhm", airSensor.gasResistance / 1000);
  }
  else
  {
    CheckAndGetSensorStatus();
  }
}

// Function implementations
bool CheckAndGetSensorStatus(void)
{
  if (airSensor.bsecStatus != BSEC_OK)
  {
    if (airSensor.bsecStatus < BSEC_OK)
    {
      LOG("BSEC error, status %d!", airSensor.bsecStatus);
      return false;
    }
    else
    {
      LOG("BSEC warning, status %d!", airSensor.bsecStatus);
    }
  }

  if (airSensor.bme68xStatus != BME68X_OK)
  {
    if (airSensor.bme68xStatus < BME68X_OK)
    {
      LOG("BME68X Sensor error, bme680_status %d!", airSensor.bme68xStatus);
      return false;
    }
    else
    {
      LOG("BME68X Sensor warning, status %d!", airSensor.bme68xStatus);
    }
  }

  return true;
}

int64_t GetTimestamp()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

void EnterErrorLEDStateForever(void)
{
  LOG("Enter Blink Error LED forever");
  while (true)
  {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
  }
}
