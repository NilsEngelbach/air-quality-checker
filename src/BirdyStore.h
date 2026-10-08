#pragma once

#include <Arduino.h>
#include "BirdyData.h"

// Sparse-mode reading buffer (v6+). Between the hourly upload windows, each
// 5-min reading is appended to /readings.log as one JSON object per line
// (JSONL). At the upload window the whole file is wrapped into a JSON array
// and POSTed in a single request (Supabase bulk insert); on success the file
// is deleted.
class BirdyStore
{
public:
    // Append one reading. birdyId is stored per line so the file is
    // self-contained. batteryVolts >= 0 adds the optional "battery" field.
    // Returns false if LittleFS is unavailable or the buffer cap is hit
    // (oldest data is kept; the new line is dropped).
    bool append(const BirdyData &data, const String &birdyId,
                float batteryVolts = -1.0f);

    // Build "[ {...}, {...}, ... ]" from the JSONL file. Returns an empty
    // string when the buffer is empty or unavailable.
    String toJsonArray();

    // Delete the buffer file (call only after a confirmed upload).
    void clear();

    // Number of buffered lines (for logging).
    uint16_t count();

    // Size cap so a long WiFi outage cannot grow the POST body past what the
    // heap/TLS stack can handle (~24 KB ≈ 2 weeks of 5-min readings).
    static const uint32_t MAX_BYTES = 24000;

private:
    static const char *STORE_PATH;
};
