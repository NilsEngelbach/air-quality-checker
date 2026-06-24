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
            unsigned long elapsed = millis() - start;
            Serial.printf("[API] WiFi connect timeout after %lu ms, status=%d\n",
                          elapsed, (int)WiFi.status());
            WiFi.mode(WIFI_OFF);
            return false;
        }
        delay(500);
        Serial.print(".");
    }
    unsigned long elapsed = millis() - start;
    Serial.printf("\n[API] connected in %lu ms, ip=%s\n",
                  elapsed, WiFi.localIP().toString().c_str());
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

    // Supabase response can occasionally take several seconds; default timeout is too short.
    http.setTimeout(20000);

    const int maxAttempts = 3;
    for (int attempt = 1; attempt <= maxAttempts; attempt++)
    {
        http.addHeader("Content-Type", "application/json");
        http.addHeader("apikey", apiKey);
        http.addHeader("Authorization", "Bearer " + String(apiKey));
        http.addHeader("Prefer", "return=minimal");

        unsigned long postStart = millis();
        int code = http.POST(body);
        unsigned long postElapsed = millis() - postStart;
        bool ok = (code == HTTP_CODE_OK || code == HTTP_CODE_CREATED);
        if (ok)
        {
            Serial.printf("[API] POST success: %d in %lu ms (attempt %d/%d)\n",
                          code, postElapsed, attempt, maxAttempts);
            return true;
        }

        String response = http.getString();
        Serial.printf("[API] POST failed: %d - %s (%lu ms) attempt %d/%d\n",
                      code, http.errorToString(code).c_str(), postElapsed,
                      attempt, maxAttempts);
        Serial.printf("[API] response: %s\n", response.c_str());

        if (attempt < maxAttempts)
        {
            // Reset the connection before retrying in case the TLS stream is stuck.
            http.end();
            http.begin(client, apiUrl);
        }
    }
    return false;
}

#endif // WIFI_ENABLED
