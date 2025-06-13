#include <Wire.h>
#include <EEPROM.h>
#include "secrets.h"
#include "BirdyData.h"
#include "BirdyAPI.h"
#include "BirdyServo.h"
#include "BirdySensor.h"
#include "BirdyLED.h"

// API
BirdyAPI birdyAPI(WIFI_SSID, WIFI_PASSWORD, API_KEY, API_URL, BIRDY_ID);

// Servo
#define SERVO_PIN 13
BirdyServo birdyServo(SERVO_PIN);

// LED
#define LED_PIN 2
BirdyLED birdyLED(LED_PIN);

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

  Serial.println("[1/4] LED initializing...");
  birdyLED.initialize();

  Serial.println("[2/4] Servo motor initializing...");
  birdyServo.initialize();

  Serial.println("[3/4] Sensor initializing...");
  birdySensor.initialize();

  Serial.println("[4/4] API initializing...");
  birdyAPI.initialize();

  Serial.println("----------------------------------------");
}

void loop()
{
  birdySensor.update();
  birdyLED.update();
}

void onSensorData(const BirdyData &data)
{
  birdyServo.setIaq(data.iaq);
  birdyLED.setAccuracy(data.accuracy);
  birdyAPI.persistData(data);
}