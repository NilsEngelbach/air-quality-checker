#pragma once

#include <Arduino.h>

// Runtime credentials. Credentials are NEVER compiled in: WiFi SSID/password
// and the API URL/key/birdy-id all live in /config.json on LittleFS.
//
// (The ESP8266 build kept WiFi in the SDK flash sector. On the ESP32-C3 the
// stored SDK config can only be read after starting the WiFi driver, which
// would cost power on every SPARSE boot just to check "provisioned?" — so the
// WiFi stack runs with persistent(false) and never stores anything.)
//
// Entered once via the setup-mode captive portal (BirdySetup); survives
// firmware flashes; only a full flash erase wipes it.
class BirdyConfig
{
public:
    // Mount LittleFS (formatting it on first boot) and load /config.json.
    // Safe to call on every boot.
    bool begin();

    bool hasWifiCredentials() const;
    bool hasApiConfig() const;
    bool isProvisioned() const { return hasWifiCredentials() && hasApiConfig(); }

    // Persist everything to /config.json. Called from the setup portal right
    // before the reboot.
    bool save(const String &ssid, const String &password,
              const String &apiUrl, const String &apiKey, const String &birdyId);

    String wifiSsid;
    String wifiPassword;
    String apiUrl;
    String apiKey;
    String birdyId;

private:
    static const char *CONFIG_PATH;

    bool loadFromFile();
    bool saveToFile();
};
