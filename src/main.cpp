#include <Wire.h>
#include <EEPROM.h>
#include "BirdyData.h"
#include "BirdySensor.h"
#include "BirdyServo.h"
#include "BirdyLED.h"

#ifdef WIFI_ENABLED
#include "secrets.h"
#include "BirdyAPI.h"
#endif

// Deep sleep: GPIO16 must be wired to RST
// ULP mode fires one sample per 300 s cycle; wake slightly early to account for boot time.
#define SLEEP_US  290000000UL  // 290 s — ~10 s margin for boot + measurement

// ULP gives one sample per wake; accuracy builds over many wakes (days).
// Timeout just ensures we sleep even if the BSEC callback never fires (I2C fault etc.).
#define MEASUREMENT_TIMEOUT_MS (30UL * 1000)  // 30 s should be more than enough

#define EEPROM_SAVE_EVERY_N_BOOTS 72  // EEPROM backup roughly every 6 hours

#define RTC_MAGIC 0xBEEF5EC2u

struct RtcData {
    uint32_t magic;
    uint8_t  lastBand;
    uint8_t  bootCount;          // wraps at 255; used for EEPROM backup cadence
    uint8_t  lastSavedAccuracy;  // highest accuracy level persisted to EEPROM
    uint8_t  pad;
    uint8_t  bsecState[BSEC_MAX_STATE_BLOB_SIZE]; // 238 bytes
    uint8_t  pad2[2];            // keep struct size a multiple of 4: 4+4+238+2 = 248
} __attribute__((aligned(4)));

static RtcData rtcData;

#define SERVO_PIN 13
#define LED_PIN   2

BirdyServo birdyServo(SERVO_PIN);
BirdyLED   birdyLED(LED_PIN);

static bool      measurementDone = false;
static BirdyData latestData;

void onSensorData(const BirdyData &data)
{
    latestData      = data;
    measurementDone = true;
}

BirdySensor birdySensor(onSensorData);

#ifdef WIFI_ENABLED
BirdyAPI birdyAPI(WIFI_SSID, WIFI_PASSWORD, API_KEY, API_URL, BIRDY_ID);
#endif

void setup()
{
    Serial.begin(115200);
    delay(100); // let USB-serial settle
    EEPROM.begin(512);
    Wire.begin();

    Serial.println("\n========================================");
    Serial.println("  Air Quality Checker");
    Serial.printf("  Reset reason : %s\n", ESP.getResetReason().c_str());
    Serial.printf("  Free heap    : %u bytes\n", ESP.getFreeHeap());
    Serial.printf("  CPU freq     : %u MHz\n", ESP.getCpuFreqMHz());
    Serial.println("========================================");

    // Restore RTC state (survives deep sleep, lost on full power cycle)
    ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcData, sizeof(rtcData));
    bool rtcValid = (rtcData.magic == RTC_MAGIC);

    if (!rtcValid)
    {
        memset(&rtcData, 0, sizeof(rtcData));
        Serial.println("[Boot] cold start — BSEC accuracy will build over many wakes");
    }
    else
    {
        Serial.printf("[Boot] RTC valid — band=%d  boot=%d\n",
                      rtcData.lastBand, rtcData.bootCount);
    }

    birdyLED.initialize();
    birdyServo.initialize(rtcData.lastBand);

    // ULP 300s mode. State blob from a previous LP run is incompatible — if upgrading
    // from LP firmware, clear EEPROM and omit RTC magic so this cold-starts cleanly.
    bool hasState = rtcValid;
    Serial.printf("[Boot] saved state: %s\n", hasState ? "yes (RTC)" : "no (cold start)");
    birdySensor.initialize(hasState ? rtcData.bsecState : nullptr);

#ifdef WIFI_ENABLED
    Serial.println("[Boot] WiFi init");
    birdyAPI.initialize();
#else
    Serial.println("[Boot] WiFi disabled");
#endif

    Serial.printf("[Boot] setup done in %lu ms\n", millis());
    Serial.println("----------------------------------------");
}

static unsigned long lastHeartbeat = 0;

void loop()
{
    birdySensor.update();
    birdyLED.update();

    // ULP fires one sample per 300 s BSEC cycle. With setState() restoring
    // next_call=0, the callback fires on the very first sensor.run() that
    // crosses the internal timestamp. Should happen within seconds of boot.
    if (!measurementDone)
    {
        if (millis() > MEASUREMENT_TIMEOUT_MS)
        {
            Serial.printf("[Loop] timeout — no sample in %lu ms, sleeping anyway\n", millis());
            goto sleep;
        }
        if (millis() - lastHeartbeat >= 5000)
        {
            Serial.printf("[Loop] waiting for ULP sample... elapsed=%lu s\n", millis() / 1000);
            lastHeartbeat = millis();
        }
        return;
    }

    measurementDone = false;

    Serial.printf("[Sensor] IAQ=%.1f  accuracy=%d  temp=%.1f C  hum=%.1f%%  pres=%.1f hPa\n",
                  latestData.iaq, latestData.accuracy,
                  latestData.temperature, latestData.humidity, latestData.pressure);

    if (latestData.accuracy >= BSEC_ACCURACY_LOW)
        rtcData.lastBand = birdyServo.setIaq(latestData.iaq);

#ifdef WIFI_ENABLED
    birdyAPI.persistData(latestData);
#endif

    birdySensor.getState(rtcData.bsecState);
    rtcData.magic = RTC_MAGIC;
    rtcData.bootCount++;

    if (latestData.accuracy > rtcData.lastSavedAccuracy)
    {
        Serial.printf("[Loop] accuracy increased to %d — EEPROM saved\n", latestData.accuracy);
        birdySensor.saveStateToEeprom();
        rtcData.lastSavedAccuracy = latestData.accuracy;
    }
    else if (rtcData.bootCount % EEPROM_SAVE_EVERY_N_BOOTS == 0)
    {
        Serial.println("[Loop] EEPROM periodic backup");
        birdySensor.saveStateToEeprom();
    }

    ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));

sleep:
    Serial.printf("[Loop] done in %lu ms — sleeping %lu s\n",
                  millis(), SLEEP_US / 1000000UL);
    Serial.flush();

    birdyLED.off();
    birdyServo.detach();
    ESP.deepSleep(SLEEP_US);
}
