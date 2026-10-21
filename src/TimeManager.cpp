#include "TimeManager.h"
#include <Wire.h>
#include <time.h>

uint8_t TimeManager::fromBcd(uint8_t value) { return (value >> 4) * 10 + (value & 0x0f); }

void TimeManager::begin(JsonObjectConst config, const String& timezone) {
    _timezone = timezone;
    JsonObjectConst ds3231 = config["ds3231"].as<JsonObjectConst>();
    _ds3231Address = ds3231["address"] | 0x68;
    _ds3231Enabled = ds3231["enabled"] | true;
    _ntpConfigured = config["ntpEnabled"] | true;
    long utcOffset = 0;
    if (_timezone == "Asia/Jakarta") utcOffset = 7 * 3600;
    else if (_timezone == "Asia/Makassar") utcOffset = 8 * 3600;
    else if (_timezone == "Asia/Jayapura") utcOffset = 9 * 3600;
    if (_ntpConfigured) configTime(utcOffset, 0, "pool.ntp.org", "time.nist.gov");
    _lastCheckMs = 0;
}

bool TimeManager::readDs3231(tm& value) const {
    if (!_ds3231Enabled) return false;
    Wire.beginTransmission(_ds3231Address);
    Wire.write(0);
    if (Wire.endTransmission() != 0 || Wire.requestFrom((int)_ds3231Address, 7) != 7) return false;
    value = {};
    value.tm_sec = fromBcd(Wire.read() & 0x7f);
    value.tm_min = fromBcd(Wire.read() & 0x7f);
    value.tm_hour = fromBcd(Wire.read() & 0x3f);
    Wire.read();
    value.tm_mday = fromBcd(Wire.read() & 0x3f);
    value.tm_mon = fromBcd(Wire.read() & 0x1f) - 1;
    value.tm_year = fromBcd(Wire.read()) + 100;
    return true;
}

bool TimeManager::writeSystemTime(const tm& value) const {
    time_t epoch = mktime(const_cast<tm*>(&value));
    if (epoch < 0) return false;
    return true;
}

void TimeManager::tick() {
    uint32_t now = millis();
    if (now - _lastCheckMs < 10000) return;
    _lastCheckMs = now;
    tm value;
    _ds3231Available = readDs3231(value);
    if (_ds3231Available) {
        writeSystemTime(value);
        return;
    }
    if (_ntpConfigured) _ntpAvailable = time(nullptr) >= 946684800;
}

void TimeManager::fillStatus(JsonObject doc) const {
    time_t current = time(nullptr);
    bool ntpAvailable = _ntpAvailable || (_ntpConfigured && current >= 946684800);
    doc["source"] = _ds3231Available ? "ds3231" : (ntpAvailable ? "ntp" : "unsynced");
    doc["ds3231Enabled"] = _ds3231Enabled;
    doc["ds3231Available"] = _ds3231Available;
    doc["ds3231Address"] = _ds3231Address;
    doc["ntpEnabled"] = _ntpConfigured;
    doc["ntpAvailable"] = _ntpAvailable;
    doc["epoch"] = (uint32_t)current;
    doc["iso"] = String(ctime(&current));
}
