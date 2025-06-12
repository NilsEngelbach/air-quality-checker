#include "BirdyAPI.h"
#include "secrets.h"

BirdyAPI::BirdyAPI(const char *apiUrl) : apiUrl(apiUrl)
{
}

void BirdyAPI::initialize()
{
    // Setup WiFi connection with SSID and password from secrets.h
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("\tConnecting to WiFi...");
    }
}

bool BirdyAPI::persistData(const BirdyData &data)
{
    JsonDocument doc;

    doc["iaq"] = data.iaq;
    doc["co2"] = data.co2;
    doc["voc"] = data.voc;
    doc["temperature"] = data.temperature;
    doc["pressure"] = data.pressure;
    doc["humidity"] = data.humidity;
    doc["accuracy"] = data.accuracy;
    doc["timestamp"] = millis();

    String jsonString;
    serializeJson(doc, jsonString);

    WiFiClient client;
    http.begin(client, apiUrl);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonString);
    bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

    if (!success)
    {
        Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return success;
}