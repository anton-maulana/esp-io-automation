#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class TimeManager {
public:
    void begin(JsonObjectConst config, const String& timezone);
    void tick();
    void fillStatus(JsonObject doc) const;

private:
    bool _ds3231Enabled = false;
    bool _ds3231Available = false;
    bool _ntpConfigured = false;
    bool _ntpAvailable = false;
    String _timezone = "UTC";
    uint8_t _ds3231Address = 0x68;
    uint32_t _lastCheckMs = 0;

    bool readDs3231(tm& value) const;
    bool writeSystemTime(const tm& value) const;
    static uint8_t fromBcd(uint8_t value);
};
