#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

class TimeManager {
public:
    void begin(JsonObjectConst config, const String& timezone);
    void tick();
    void fillStatus(JsonObject doc) const;

private:
    bool _ds3231Enabled = false;
    bool _ds3231Available = false;
    bool _ds3231Checked = false;
    bool _ntpConfigured = false;
    bool _ntpAvailable = false;
    bool _clockValid = false;
    bool _rtcValid = false;
    bool _rtcSyncPending = false;
    bool _ntpRunning = false;
    String _timezone = "UTC";
    String _mode = "auto";
    String _source = "unsynced";
    uint8_t _ds3231Address = 0x68;
    uint32_t _lastCheckMs = 0;
    time_t _lastNtpSync = 0;

    bool readDs3231(tm& value);
    bool writeDs3231(const tm& value) const;
    bool writeSystemTime(const tm& value) const;
    static bool isValidEpoch(time_t value);
    static time_t tmToUtc(const tm& value);
    static uint8_t fromBcd(uint8_t value);
    static uint8_t toBcd(uint8_t value);
};
