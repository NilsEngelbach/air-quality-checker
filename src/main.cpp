#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>
#include <FS.h>
#include <LittleFS.h>

#include "secrets.h"

Adafruit_BME680 bme; // I2C

void setup() {
    Serial.begin(115200);
    Wire.begin();

    Serial.println("Starting...");

    if (!bme.begin()) {
        Serial.println("BME688 nicht gefunden!");
        while (1);
    }
    
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setGasHeater(320, 150);
}

void loop() {
    // Perform a measurement
    if (!bme.performReading()) {
        Serial.println("Failed to perform reading");
        return;
    }

    // Print sensor values
    Serial.print("Temperature = ");
    Serial.print(bme.temperature);
    Serial.println(" *C");

    Serial.print("Pressure = ");
    Serial.print(bme.pressure / 100.0);
    Serial.println(" hPa");

    Serial.print("Humidity = ");
    Serial.print(bme.humidity);
    Serial.println(" %");

    Serial.print("Gas Resistance = ");
    Serial.print(bme.gas_resistance);
    Serial.println(" ohms");

    Serial.println();

    delay(2000); // Wait 2 seconds before next read
}