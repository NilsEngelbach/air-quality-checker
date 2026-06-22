#pragma once

#ifdef WIFI_ENABLED

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "BirdyData.h"

#define WIFI_CONNECT_TIMEOUT_MS 10000

class BirdyAPI
{
public:
    BirdyAPI(
        const char *ssid,
        const char *password,
        const char *apiKey,
        const char *apiUrl,
        const char *birdyId);

    // Returns false if WiFi did not connect within the timeout.
    bool initialize();

    bool persistData(const BirdyData &data);

private:
    const char *ssid;
    const char *password;
    const char *apiKey;
    const char *apiUrl;
    const char *birdyId;
    HTTPClient      http;
    WiFiClientSecure client;
};

#endif // WIFI_ENABLED
