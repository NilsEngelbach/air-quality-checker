#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "BirdyData.h"

#define API_UPDATE_INTERVAL (60 * 1000) // Send data every minute

class BirdyAPI
{
public:
    BirdyAPI(
        const char *ssid,
        const char *password,
        const char *apiKey,
        const char *apiUrl,
        const char *birdyId);
    void initialize();
    bool persistData(const BirdyData &data);

private:
    const char *apiUrl;
    const char *ssid;
    const char *password;
    const char *apiKey;
    const char *birdyId;
    HTTPClient http;
    WiFiClientSecure client;

    unsigned long lastUpdate = 0;
    bool shouldUpdate();
};