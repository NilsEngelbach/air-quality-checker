#include "BirdyStore.h"

#include <LittleFS.h>
#include <ArduinoJson.h>

const char *BirdyStore::STORE_PATH = "/readings.log";

bool BirdyStore::append(const BirdyData &data, const String &birdyId,
                        float batteryVolts)
{
    File f = LittleFS.open(STORE_PATH, "a");
    if (!f)
    {
        Serial.println("[Store] append failed (LittleFS?)");
        return false;
    }
    if (f.size() >= MAX_BYTES)
    {
        f.close();
        Serial.printf("[Store] buffer full (%u bytes) — dropping new reading\n",
                      (unsigned)f.size());
        return false;
    }

    // Same field names as the direct upload (BirdyAPI) so both paths insert
    // identical rows.
    JsonDocument doc;
    doc["sensor_id"]   = birdyId;
    doc["iaq"]         = data.iaq;
    doc["co2"]         = data.co2;
    doc["voc"]         = data.voc;
    doc["temperature"] = data.temperature;
    doc["pressure"]    = data.pressure;
    doc["humidity"]    = data.humidity;
    doc["accuracy"]    = data.accuracy;
    if (batteryVolts >= 0.0f)
        doc["battery"] = batteryVolts;

    serializeJson(doc, f);
    f.write('\n');
    f.close();
    return true;
}

String BirdyStore::toJsonArray()
{
    File f = LittleFS.open(STORE_PATH, "r");
    if (!f || f.size() == 0)
    {
        if (f) f.close();
        return String();
    }

    String out;
    out.reserve(f.size() + 16);
    out = "[";
    bool first = true;
    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;
        if (!first)
            out += ',';
        out += line;
        first = false;
    }
    out += "]";
    f.close();
    return first ? String() : out;
}

void BirdyStore::clear()
{
    LittleFS.remove(STORE_PATH);
    Serial.println("[Store] buffer cleared after upload");
}

uint16_t BirdyStore::count()
{
    File f = LittleFS.open(STORE_PATH, "r");
    if (!f)
        return 0;

    uint16_t lines = 0;
    while (f.available())
    {
        if (f.readStringUntil('\n').length() > 0)
            lines++;
    }
    f.close();
    return lines;
}
