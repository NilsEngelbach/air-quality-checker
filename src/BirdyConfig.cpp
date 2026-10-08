#include "BirdyConfig.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>

const char *BirdyConfig::CONFIG_PATH = "/config.json";

bool BirdyConfig::begin()
{
    // The WiFi driver never writes its own flash copy of the credentials —
    // they live in /config.json and are passed to WiFi.begin() explicitly.
    WiFi.persistent(false);

    // formatOnFail: a factory-fresh board has an unformatted partition
    // (the ESP8266 core formatted automatically, the ESP32 core does not).
    if (!LittleFS.begin(true))
    {
        Serial.println("[Config] LittleFS mount failed — no config available");
        return false;
    }
    return loadFromFile();
}

bool BirdyConfig::hasWifiCredentials() const
{
    return wifiSsid.length() > 0;
}

bool BirdyConfig::hasApiConfig() const
{
    return apiUrl.length() > 0 && apiKey.length() > 0 && birdyId.length() > 0;
}

bool BirdyConfig::save(const String &ssid, const String &password,
                       const String &apiUrl, const String &apiKey, const String &birdyId)
{
    this->wifiSsid     = ssid;
    this->wifiPassword = password;
    this->apiUrl       = apiUrl;
    this->apiKey       = apiKey;
    this->birdyId      = birdyId;

    return saveToFile();
}

bool BirdyConfig::loadFromFile()
{
    File f = LittleFS.open(CONFIG_PATH, "r");
    if (!f)
    {
        Serial.println("[Config] no /config.json — device not provisioned yet");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err)
    {
        Serial.printf("[Config] /config.json parse error: %s\n", err.c_str());
        return false;
    }

    wifiSsid     = doc["ssid"]    | "";
    wifiPassword = doc["pass"]    | "";
    apiUrl       = doc["apiUrl"]  | "";
    apiKey       = doc["apiKey"]  | "";
    birdyId      = doc["birdyId"] | "";
    Serial.printf("[Config] loaded (ssid=%s apiUrl=%s birdyId=%s)\n",
                  wifiSsid.c_str(), apiUrl.c_str(), birdyId.c_str());
    return true;
}

bool BirdyConfig::saveToFile()
{
    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f)
    {
        Serial.println("[Config] cannot write /config.json");
        return false;
    }

    JsonDocument doc;
    doc["ssid"]    = wifiSsid;
    doc["pass"]    = wifiPassword;
    doc["apiUrl"]  = apiUrl;
    doc["apiKey"]  = apiKey;
    doc["birdyId"] = birdyId;
    serializeJson(doc, f);
    f.close();
    Serial.println("[Config] /config.json written");
    return true;
}
