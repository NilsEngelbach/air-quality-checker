#include "BirdyAPI.h"

#ifdef WIFI_ENABLED

BirdyAPI::BirdyAPI(
    const char *ssid,
    const char *password,
    const char *apiKey,
    const char *apiUrl,
    const char *birdyId)
    : ssid(ssid), password(password), apiKey(apiKey),
      apiUrl(apiUrl), birdyId(birdyId)
{
}

bool BirdyAPI::initialize()
{
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS)
        {
            Serial.println("[API] WiFi connect timeout — offline mode");
            WiFi.mode(WIFI_OFF);
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\n[API] connected: %s\n", WiFi.localIP().toString().c_str());
    client.setInsecure();
    http.begin(client, apiUrl);
    return true;
}

bool BirdyAPI::persistData(const BirdyData &data)
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    JsonDocument doc;
    doc["sensor_id"]  = birdyId;
    doc["iaq"]        = data.iaq;
    doc["co2"]        = data.co2;
    doc["voc"]        = data.voc;
    doc["temperature"]= data.temperature;
    doc["pressure"]   = data.pressure;
    doc["humidity"]   = data.humidity;
    doc["accuracy"]   = data.accuracy;

    String body;
    serializeJson(doc, body);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", apiKey);
    http.addHeader("Authorization", "Bearer " + String(apiKey));
    http.addHeader("Prefer", "return=minimal");

    int code = http.POST(body);
    bool ok = (code == HTTP_CODE_OK || code == HTTP_CODE_CREATED);
    if (ok)
    {
        Serial.printf("[API] POST success: %d\n", code);
    }
    else
    {
        String body = http.getString();
        Serial.printf("[API] POST failed: %d - %s\n", code, http.errorToString(code).c_str());
        Serial.printf("[API] response: %s\n", body.c_str());
    }
    return ok;
}

#endif // WIFI_ENABLED
