#include <Wire.h>
#include <bsec2.h>
#include <EEPROM.h>

#define BSEC_ACCURACY_UNRELIABLE 0
#define BSEC_ACCURACY_LOW 1
#define BSEC_ACCURACY_MEDIUM 2
#define BSEC_ACCURACY_HIGH 3

Bsec2 iaqSensor;
static uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
const uint8_t bsecConfig[] = {
#include "config/bme688/bme688_sel_33v_3s_4d/bsec_selectivity.txt"
};

static uint8_t lastAccuracy = 0;
const unsigned long STATE_SAVE_INTERVAL = 5 * 60 * 1000; // 5 minutes in milliseconds
static unsigned long lastStateSave = 0;

void checkBsecStatus(Bsec2 bsec);
void callback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec);
bool loadState(Bsec2 bsec);
bool saveState(Bsec2 bsec);

const char *getAccuracyString(uint8_t accuracy)
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

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);
  Serial.println("\nStarting Air Quality Sensor");
  Serial.println("----------------------------------------");

  Wire.begin();

  Serial.println("[1/6] Starting communication with BME688");

  Wire.beginTransmission(BME68X_I2C_ADDR_HIGH);
  byte error = Wire.endTransmission();
  if (error != 0)
  {
    Serial.println("Error: Could not find BME688 sensor!");
    Serial.println("Error code: " + String(error));
    return;
  }

  Serial.println("BME688 sensor found!");
  if (!iaqSensor.begin(BME68X_I2C_ADDR_HIGH, Wire))
  {
    checkBsecStatus(iaqSensor);
  }

  Serial.println("\n[2/6] Set configuration");
  if (!iaqSensor.setConfig(bsecConfig))
  {
    checkBsecStatus(iaqSensor);
  }

  Serial.println("\n[3/6] Load state");
  if (!loadState(iaqSensor))
  {
    checkBsecStatus(iaqSensor);
  }

  bsecSensor sensorList[] = {
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_CO2_EQUIVALENT,
      BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY};

  Serial.println("\n[4/6] Update subscription");
  if (!iaqSensor.updateSubscription(sensorList, 6, BSEC_SAMPLE_RATE_LP))
  {
    checkBsecStatus(iaqSensor);
  }

  Serial.println("\n[5/6] Attach callback");
  iaqSensor.attachCallback(callback);

  Serial.println("\n[6/6] Start");
  Serial.println("----------------------------------------");
}

void loop()
{
  if (!iaqSensor.run())
  {
    checkBsecStatus(iaqSensor);
  }
}

void callback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec)
{
  if (!outputs.nOutputs)
  {
    return;
  }

  Serial.println("\nBSEC outputs [" + String((int)(outputs.output[0].time_stamp / INT64_C(1000000))) + "]");
  for (uint8_t i = 0; i < outputs.nOutputs; i++)
  {
    const bsecData output = outputs.output[i];
    switch (output.sensor_id)
    {
    case BSEC_OUTPUT_IAQ:
      Serial.printf("IAQ: %.2f (Accuracy: %s)\n", output.signal, getAccuracyString(output.accuracy));

      // Check if accuracy has improved or time interval has passed
      if (output.accuracy > lastAccuracy || (millis() - lastStateSave >= STATE_SAVE_INTERVAL))
      {
        if (saveState(bsec))
        {
          Serial.println("State saved due to " + String(output.accuracy > lastAccuracy ? "improved accuracy" : "time interval"));
          lastAccuracy = output.accuracy;
          lastStateSave = millis();
        }
      }
      break;
    case BSEC_OUTPUT_CO2_EQUIVALENT:
      Serial.printf("CO2 equivalent: %.2f ppm\n", output.signal);
      break;
    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
      Serial.printf("VOC equivalent: %.2f ppm\n", output.signal);
      break;
    case BSEC_OUTPUT_RAW_TEMPERATURE:
      Serial.printf("Temperature: %.2f °C\n", output.signal);
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      Serial.printf("Pressure: %.2f hPa\n", output.signal);
      break;
    case BSEC_OUTPUT_RAW_HUMIDITY:
      Serial.printf("Humidity: %.2f %%\n", output.signal);
      break;
    default:
      break;
    }
  }
}

void checkBsecStatus(Bsec2 bsec)
{
  if (bsec.status < BSEC_OK)
  {
    Serial.println("BSEC error code : " + String(bsec.status));
  }
  else if (bsec.status > BSEC_OK)
  {
    Serial.println("BSEC warning code : " + String(bsec.status));
  }

  if (bsec.sensor.status < BME68X_OK)
  {
    Serial.println("BME68X error code : " + String(bsec.sensor.status));
  }
  else if (bsec.sensor.status > BME68X_OK)
  {
    Serial.println("BME68X warning code : " + String(bsec.sensor.status));
  }
}

bool loadState(Bsec2 bsec)
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

  return bsec.setState(bsecState);
}

bool saveState(Bsec2 bsec)
{
  if (!bsec.getState(bsecState))
    return false;

  Serial.println("Writing state to EEPROM");
  Serial.print("State file: ");

  for (uint8_t i = 0; i < BSEC_MAX_STATE_BLOB_SIZE; i++)
  {
    EEPROM.write(i + 1, bsecState[i]);
    Serial.print(String(bsecState[i], HEX) + ", ");
  }
  Serial.println();

  EEPROM.write(0, BSEC_MAX_STATE_BLOB_SIZE);
  EEPROM.commit();
  return true;
}
