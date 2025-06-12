#include <Wire.h>
#include <EEPROM.h>
#include "secrets.h"
#include "BirdyData.h"
#include "BirdyAPI.h"
#include "BirdyServo.h"
#include "BirdySensor.h"

// API
BirdyAPI birdyAPI(WIFI_SSID, WIFI_PASSWORD, API_URL);

// Servo
#define SERVO_PIN 13
BirdyServo birdyServo(SERVO_PIN);

// Sensor
void onSensorData(const BirdyData &data);
BirdySensor birdySensor(onSensorData);

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);
  Wire.begin();

  Serial.println("\nStarting Birdy");
  Serial.println("----------------------------------------");

  Serial.println("[1/3] Servo motor initializing...");
  birdyServo.initialize();

  Serial.println("[2/3] Sensor initializing...");
  birdySensor.initialize();

  Serial.println("[3/3] API initializing...");
  birdyAPI.initialize();

  Serial.println("----------------------------------------");
}

void loop()
{
  birdySensor.update();
}

void onSensorData(const BirdyData &data)
{
  birdyServo.updatePosition(data.iaq);
  birdyAPI.persistData(data);
}