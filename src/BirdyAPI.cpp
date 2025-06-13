#include "BirdyAPI.h"

BirdyAPI::BirdyAPI(
    const char *ssid,
    const char *password,
    const char *apiKey,
    const char *apiUrl,
    const char *birdyId) : ssid(ssid),
                           password(password),
                           apiKey(apiKey),
                           apiUrl(apiUrl),
                           birdyId(birdyId),
                           lastUpdate(-API_UPDATE_INTERVAL)
{
}

void BirdyAPI::initialize()
{
    WiFi.begin(this->ssid, this->password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("\tConnecting to WiFi...");
    }
    this->client.setInsecure();
    this->http.begin(this->client, this->apiUrl);
}

bool BirdyAPI::shouldUpdate()
{
    unsigned long currentTime = millis();
    if (currentTime - this->lastUpdate >= API_UPDATE_INTERVAL)
    {
        this->lastUpdate = currentTime;
        return true;
    }
    return false;
}

bool BirdyAPI::persistData(const BirdyData &data)
{
    if (!shouldUpdate())
    {
        return true;
    }

    JsonDocument doc;

    doc["sensor_id"] = this->birdyId;
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
    http.addHeader("apikey", this->apiKey);
    http.addHeader("Authorization", "Bearer " + String(this->apiKey));
    http.addHeader("Prefer", "return=minimal");

    int httpCode = this->http.POST(jsonString);
    bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

    if (!success)
    {
        Serial.printf("HTTP POST failed, error: %s\n", this->http.errorToString(httpCode).c_str());
    }

    return success;
}