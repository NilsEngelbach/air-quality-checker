#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "BirdyData.h"

class BirdyAPI
{
public:
    BirdyAPI(const char *apiUrl);
    void initialize();
    bool persistData(const BirdyData &data);

private:
    const char *apiUrl;
    HTTPClient http;
    WiFiClient client;
};