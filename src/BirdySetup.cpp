#include "BirdySetup.h"

#include <WiFi.h>

static const char FORM_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Birdy Setup</title>
<style>body{font-family:sans-serif;margin:1.5em}input{width:100%;padding:.5em;margin:.3em 0}
button{padding:.7em 1.5em;font-size:1em}</style></head><body>
<h2>Birdy Setup</h2>
<form method="POST" action="/save">
<label>WiFi SSID</label><input name="ssid" required>
<label>WiFi Password</label><input name="pass" type="password">
<label>API URL</label><input name="apiurl" placeholder="https://...supabase.co/rest/v1/air_quality_data" required>
<label>API Key</label><input name="apikey" required>
<label>Birdy ID (sensor UUID)</label><input name="birdyid" required>
<button type="submit">Save &amp; Reboot</button>
</form></body></html>)rawliteral";

bool BirdySetup::run(BirdyConfig &config, BirdyLED &led, uint32_t timeoutMs)
{
    // Last 3 MAC bytes in printed order, like the ESP8266 chip id the AP
    // name used before (getEfuseMac() holds MAC byte 0 in the lowest byte).
    uint64_t mac    = ESP.getEfuseMac();
    uint32_t chipId = ((uint32_t)(mac >> 24) & 0xFF) << 16 |
                      ((uint32_t)(mac >> 32) & 0xFF) << 8 |
                      ((uint32_t)(mac >> 40) & 0xFF);
    String apName = "Birdy-Setup-" + String(chipId, HEX);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str());
    IPAddress apIp = WiFi.softAPIP();

    dns.start(53, "*", apIp);  // captive portal: every lookup answers with us

    server.on("/", HTTP_GET, [this]() { server.send_P(200, "text/html", FORM_PAGE); });
    server.on("/save", HTTP_POST, [this, &config]()
    {
        bool ok = config.save(server.arg("ssid"), server.arg("pass"),
                              server.arg("apiurl"), server.arg("apikey"),
                              server.arg("birdyid"));
        server.send(200, "text/html",
                    ok ? "<h2>Saved. Birdy reboots now — reconnect to your WiFi.</h2>"
                       : "<h2>Save failed — please try again.</h2>");
        saved = ok;
    });
    server.onNotFound([this]()
    {
        server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    });
    server.begin();

    Serial.printf("[Setup] portal up: %s  ip=%s  timeout=%lu s\n",
                  apName.c_str(), apIp.toString().c_str(), timeoutMs / 1000UL);

    led.setSetupPattern(true);
    unsigned long start = millis();
    while (!saved && millis() - start < timeoutMs)
    {
        dns.processNextRequest();
        server.handleClient();
        led.update();
        yield();
    }
    led.setSetupPattern(false);

    server.stop();
    dns.stop();

    if (saved)
    {
        Serial.println("[Setup] credentials saved — rebooting into selected mode");
        delay(500);  // let the HTTP response reach the phone
        ESP.restart();
    }

    Serial.printf("[Setup] timeout after %lu s — back to deep sleep\n",
                  (millis() - start) / 1000UL);
    return false;
}
