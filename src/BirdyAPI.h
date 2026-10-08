#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "BirdyData.h"

#define WIFI_CONNECT_TIMEOUT_MS 10000

// WiFi upload. Credentials come from BirdyConfig (/config.json on LittleFS)
// at runtime — nothing is compiled in. Whether WiFi is used
// at all on a given boot is decided by the SW4 mode switch in main.cpp.
class BirdyAPI
{
public:
    // Connect to the stored network and prepare the HTTPS client.
    // Returns false if WiFi did not connect within the timeout.
    bool initialize(const String &ssid, const String &password,
                    const String &apiKey, const String &apiUrl,
                    const String &birdyId);

    // Single reading (CONTINUOUS mode). batteryVolts >= 0 adds the optional
    // "battery" field (needs migration 009 on the database).
    bool persistData(const BirdyData &data, float batteryVolts = -1.0f);

    // Bulk insert (SPARSE mode): jsonArray is a serialized JSON array of
    // reading objects (see BirdyStore::toJsonArray). Supabase/PostgREST
    // accepts an array body for multi-row insert.
    bool persistBatch(const String &jsonArray);

    // Drop the HTTP connection and turn the modem off (call before sleep).
    void end();

    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

private:
    String apiKey;
    String apiUrl;
    String birdyId;
    bool   httpBegun = false;

    HTTPClient      http;
    WiFiClientSecure client;

    bool post(const String &body);
};
