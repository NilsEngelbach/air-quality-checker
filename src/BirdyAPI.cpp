#include "BirdyAPI.h"

BirdyAPI::BirdyAPI(const char *ssid, const char *password, const char *apiKey, const char *apiUrl, const char *birdyId) : ssid(ssid), password(password), apiKey(apiKey), apiUrl(apiUrl), birdyId(birdyId)
{
}

void BirdyAPI::initialize()
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("\tConnecting to WiFi...");
    }
    client.setInsecure();
    http.begin(client, apiUrl);
}

bool BirdyAPI::persistData(const BirdyData &data)
{
    JsonDocument doc;

    doc["sensor_id"] = birdyId;
    doc["timestamp"] = millis();
    doc["iaq"] = data.iaq;
    doc["co2"] = data.co2;
    doc["voc"] = data.voc;
    doc["temperature"] = data.temperature;
    doc["pressure"] = data.pressure;
    doc["humidity"] = data.humidity;
    doc["accuracy"] = data.accuracy;

    String jsonString;
    serializeJson(doc, jsonString);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", apiKey);
    http.addHeader("Authorization", "Bearer " + String(apiKey));
    http.addHeader("Prefer", "return=minimal");

    int httpCode = http.POST(jsonString);
    bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

    if (!success)
    {
        Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    return success;
}