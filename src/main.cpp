#include <Wire.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include "BirdyData.h"
#include "BirdySensor.h"
#include "BirdyServo.h"
#include "BirdyLED.h"
#include "BirdyMode.h"
#include "BirdyBattery.h"
#include "BirdyConfig.h"
#include "BirdyStore.h"
#include "BirdyAPI.h"
#include "BirdySetup.h"

// Deep sleep: the ESP32-C3 wakes from its RTC timer (no GPIO16→RST link needed).
// ULP mode fires one sample per 300 s cycle; wake slightly early to account for boot time.
#define SLEEP_US  290000000ULL  // 290 s — ~10 s margin for boot + measurement

// ULP gives one sample per wake; accuracy builds over many wakes (days).
// Timeout just ensures we sleep even if the BSEC callback never fires (I2C fault etc.).
#define MEASUREMENT_TIMEOUT_MS (30UL * 1000)  // 30 s should be more than enough

#define EEPROM_SAVE_EVERY_N_BOOTS 72  // EEPROM backup roughly every 6 hours

// SPARSE mode: upload the LittleFS buffer every N wakes (300 s cycle:
// 12 = hourly, 24 = every 2 h). Readings accumulate between windows.
#define SPARSE_UPLOAD_EVERY_N_BOOTS 12

// Setup portal stays awake this long waiting for the user, then sleeps.
#define SETUP_TIMEOUT_MS (180UL * 1000)  // ~3 min

// Battery thresholds (on battery only — with USB present VSYS comes from VBUS).
// Below VBAT_SERVO_MIN a servo start can pull VSYS under the LDO dropout and
// brown out the C3 (PCB F25): skip the move, retry on a later wake.
// Below VBAT_CUTOFF the device stops measuring and only checks back hourly
// until the cell is charged (PCB F19; the cell's PCM is the hard cutoff).
#define VBAT_SERVO_MIN   3.5f
#define VBAT_CUTOFF      3.3f
#define CUTOFF_SLEEP_US  3600000000ULL  // 1 h

#define RTC_MAGIC 0xBEEF5EC4u

struct RtcData {
    uint32_t magic;
    uint8_t  lastBand;
    uint8_t  bootCount;          // wraps at 255; used for EEPROM backup + sparse cadence
    uint8_t  lastSavedAccuracy;  // highest accuracy level persisted to EEPROM
    uint8_t  pad;
    uint64_t totalElapsedMs;     // accumulated uptime for BSEC timestamp continuity across sleeps
    uint8_t  bsecState[BSEC_MAX_STATE_BLOB_SIZE]; // 238 bytes
};

// RTC memory: survives deep sleep and software resets (e.g. the reboot after
// setup), like the ESP8266 RTC user memory did. Not initialised at power-on —
// the magic check (plus the reset reason) decides whether it is valid.
RTC_NOINIT_ATTR static RtcData rtcData;

// Pin map — air-quality-pcb v9 (rev I), ESP32-C3-WROOM-02
#define SERVO_PIN      0   // SERVO_PWM, via R17 100 Ω
#define VBUS_SENSE_PIN 1   // ADC1_CH1, R12/R13 divider
//                     2      QT_PWR_N (STEMMA QT power, active low) — unused; R2 keeps the port off
#define VBAT_SENSE_PIN 3   // ADC1_CH3, R20/R21 divider
#define MODE_OFF_PIN   4   // MODE_A: SW4 POS.1 shorts this to GND -> WiFi OFF
#define MODE_CONT_PIN  5   // MODE_B: SW4 POS.3 shorts this to GND -> WiFi CONTINUOUS
#define SDA_PIN        6
#define SCL_PIN        7
#define LED_PIN        8   // STATUS_LED_N, active low (strap, R5 pull-up)
#define BOOT_PIN       9   // BOOT button (SW2), sampled right after boot -> force setup
#define SERVO_EN_PIN  10   // servo rail switch (Q7 → Q6) — HIGH only during a move
#define CHG_DET_PIN   20   // charger STAT: LOW = charging (valid only with VBUS)

BirdyServo birdyServo(SERVO_PIN, SERVO_EN_PIN);
BirdyLED   birdyLED(LED_PIN);
BirdyConfig birdyConfig;
BirdyStore  birdyStore;
BirdyAPI    birdyAPI;
BirdySetup  birdySetup;

static BirdyWifiMode wifiMode     = BirdyWifiMode::OFF;
static bool          forceSetup   = false;
static bool          uploadDue    = false;  // SPARSE: this boot is an upload window
static bool          wifiUp       = false;
static BirdyPower    power;

static bool      measurementDone = false;
static BirdyData latestData;

void onSensorData(const BirdyData &data)
{
    latestData      = data;
    measurementDone = true;
}

BirdySensor birdySensor(onSensorData);

static const char *resetReasonLabel(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_EXT:       return "reset pin";
    default:                return "other";
    }
}

// Shared shutdown path: persist RTC, lights out, servo rail off, modem off, sleep.
static void goToSleep(uint64_t sleepUs = SLEEP_US)
{
    // Accumulate this wake's uptime + the upcoming sleep. Next boot passes the
    // total to BirdySensor::initialize, which feeds it to BSEC's millis callback
    // so the timeline stays monotonic across deep-sleep wakes.
    rtcData.totalElapsedMs += (uint64_t)millis() + (sleepUs / 1000ULL);
    rtcData.magic = RTC_MAGIC;

    Serial.printf("[Loop] done in %lu ms — sleeping %llu s\n",
                  millis(), sleepUs / 1000000ULL);
    Serial.flush();

    birdyLED.off();
    birdyServo.detach();  // also drives SERVO_EN LOW — never HIGH across sleep
    birdyAPI.end();       // modem off (no-op if WiFi was never started)
    esp_sleep_enable_timer_wakeup(sleepUs);
    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);
    delay(100); // let USB CDC settle (the console only exists while awake)
    EEPROM.begin(512);
    Wire.begin(SDA_PIN, SCL_PIN);

    // BOOT button: sampled early (and once more below) — pressed shortly after
    // RESET forces setup mode for re-provisioning. (Holding it *during* reset
    // lands in the ROM download mode, so the firmware can only catch presses
    // within the first few hundred ms after boot.)
    pinMode(BOOT_PIN, INPUT_PULLUP);
    forceSetup = (digitalRead(BOOT_PIN) == LOW);

    esp_reset_reason_t resetReason = esp_reset_reason();

    Serial.println("\n========================================");
    Serial.println("  Air Quality Checker (PCB v9)");
    Serial.printf("  Reset reason : %s\n", resetReasonLabel(resetReason));
    Serial.printf("  Free heap    : %u bytes\n", ESP.getFreeHeap());
    Serial.printf("  CPU freq     : %u MHz\n", ESP.getCpuFreqMHz());
    Serial.println("========================================");

    // RTC state survives deep sleep and software resets, but is random after
    // power-on and not trustworthy after a brownout.
    bool rtcValid = (rtcData.magic == RTC_MAGIC) &&
                    resetReason != ESP_RST_POWERON &&
                    resetReason != ESP_RST_BROWNOUT;

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
    birdyServo.initialize(rtcData.lastBand);  // drives SERVO_EN LOW

    // Battery, USB and charge state — read before WiFi so TX load cannot skew VBAT.
    power = BirdyBattery::read(VBAT_SENSE_PIN, VBUS_SENSE_PIN, CHG_DET_PIN);
    Serial.printf("[Boot] VBAT=%.2f V  USB=%s%s\n", power.vbat,
                  power.usb ? "yes" : "no",
                  power.usb ? (power.charging ? " (charging)" : " (charge done)") : "");

    if (!power.usb && power.vbat < VBAT_CUTOFF)
    {
        Serial.printf("[Boot] VBAT below %.1f V cutoff — sleeping until charged\n", VBAT_CUTOFF);
        birdyLED.flash(200);
        goToSleep(CUTOFF_SLEEP_US);
    }

    // WiFi mode from the slide switch, read once with internal pull-ups.
    wifiMode = BirdyMode::read(MODE_OFF_PIN, MODE_CONT_PIN);
    Serial.printf("[Boot] WiFi mode switch: %s\n", BirdyMode::label(wifiMode));

    // Mount LittleFS + load the stored credentials; sets WiFi.persistent(false)
    // so no WiFi call ever writes the driver's own flash copy.
    birdyConfig.begin();

    forceSetup = forceSetup || (digitalRead(BOOT_PIN) == LOW);
    if (forceSetup)
        Serial.println("[Boot] BOOT button held — forcing setup mode");

    // --- WiFi decision, before the sensor starts, so a setup-mode reboot
    // --- never wastes a sensor init. The ESP32 radio stays off unless started,
    // --- so OFF boots and non-upload SPARSE boots simply never touch WiFi.
    if (wifiMode == BirdyWifiMode::OFF)
    {
        Serial.println("[Boot] mode OFF — modem stays off");
    }
    else if (forceSetup || !birdyConfig.isProvisioned())
    {
        Serial.println("[Boot] no credentials (or forced) — entering setup mode");
        rtcData.bootCount++;          // a wake is a wake, even in setup
        birdySetup.run(birdyConfig, birdyLED, SETUP_TIMEOUT_MS);
        // Saved -> run() reboots. Timeout -> fall through to sleep.
        goToSleep();
    }
    else
    {
        // SPARSE uploads every N wakes; CONTINUOUS uploads every wake.
        uploadDue = (wifiMode == BirdyWifiMode::CONTINUOUS) ||
                    (rtcData.bootCount % SPARSE_UPLOAD_EVERY_N_BOOTS == 0);

        if (uploadDue)
        {
            Serial.printf("[Boot] %s upload window — connecting\n",
                          BirdyMode::label(wifiMode));
            wifiUp = birdyAPI.initialize(birdyConfig.wifiSsid, birdyConfig.wifiPassword,
                                         birdyConfig.apiKey, birdyConfig.apiUrl,
                                         birdyConfig.birdyId);
        }
        else
        {
            Serial.printf("[Boot] SPARSE — next upload in %d wakes, modem off\n",
                          SPARSE_UPLOAD_EVERY_N_BOOTS -
                              (rtcData.bootCount % SPARSE_UPLOAD_EVERY_N_BOOTS));
        }
    }

    // ULP 300s mode. State blob from a previous LP run is incompatible — if upgrading
    // from LP firmware, clear EEPROM and omit RTC magic so this cold-starts cleanly.
    bool hasState = rtcValid;
    Serial.printf("[Boot] saved state: %s  elapsed: %llu ms\n",
                  hasState ? "yes (RTC)" : "no (cold start)", rtcData.totalElapsedMs);
    birdySensor.initialize(hasState ? rtcData.bsecState : nullptr,
                           (int64_t)rtcData.totalElapsedMs);

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
            Serial.printf("[Loop] SENSOR TIMEOUT — no sample in %lu ms, sleeping without upload\n", millis());
            goToSleep();
        }
        if (millis() - lastHeartbeat >= 5000)
        {
            Serial.printf("[Loop] waiting for ULP sample... elapsed=%lu s\n", millis() / 1000);
            lastHeartbeat = millis();
        }
        return;
    }

    measurementDone = false;

    // Battery voltage was read in setup() (before any WiFi load).
    float batteryVolts = power.vbat;

    Serial.printf("[Sensor] IAQ=%.1f  accuracy=%d  temp=%.1f C  hum=%.1f%%  pres=%.1f hPa  batt=%.2f V\n",
                  latestData.iaq, latestData.accuracy,
                  latestData.temperature, latestData.humidity, latestData.pressure,
                  batteryVolts);

    birdyLED.setAccuracy(latestData.accuracy);

    // The move happens before any upload, so it never overlaps a WiFi TX burst.
    if (latestData.accuracy >= BSEC_ACCURACY_LOW)
    {
        if (power.usb || power.vbat >= VBAT_SERVO_MIN)
            rtcData.lastBand = birdyServo.setIaq(latestData.iaq);
        else
            Serial.printf("[Servo] VBAT %.2f V < %.1f V on battery — move skipped\n",
                          power.vbat, VBAT_SERVO_MIN);
    }

    // --- Per-mode data handling -------------------------------------------
    switch (wifiMode)
    {
    case BirdyWifiMode::OFF:
        break;

    case BirdyWifiMode::CONTINUOUS:
        if (wifiUp)
        {
            Serial.printf("[Loop] uploading accuracy=%d iaq=%.1f batt=%.2f V\n",
                          latestData.accuracy, latestData.iaq, batteryVolts);
            birdyAPI.persistData(latestData, batteryVolts);
        }
        else
        {
            Serial.println("[Loop] WiFi down — reading lost (CONTINUOUS)");
        }
        break;

    case BirdyWifiMode::SPARSE:
        if (!birdyStore.append(latestData, birdyConfig.birdyId, batteryVolts))
            Serial.println("[Loop] buffer append failed");
        if (uploadDue)
        {
            Serial.printf("[Loop] SPARSE upload window — %d buffered reading(s)\n",
                          birdyStore.count());
            if (wifiUp && birdyAPI.persistBatch(birdyStore.toJsonArray()))
                birdyStore.clear();
            else
                Serial.println("[Loop] upload failed — readings stay buffered");
        }
        break;
    }

    birdySensor.getState(rtcData.bsecState);
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

    goToSleep();
}
