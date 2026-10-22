#include "TimeManager.h"
#include <Wire.h>
#include <time.h>
#include <sys/time.h>
#include <atomic>
#ifdef ESP8266
#include <coredecls.h>
#include <lwip/apps/sntp.h>
#else
#include <esp_sntp.h>
#endif
#include "Diagnostics.h"

namespace {
std::atomic<bool> ntpSynced{false};
}

uint8_t TimeManager::fromBcd(uint8_t value) { return (value >> 4) * 10 + (value & 0x0f); }
uint8_t TimeManager::toBcd(uint8_t value) { return static_cast<uint8_t>(((value / 10) << 4) | (value % 10)); }

bool TimeManager::isValidEpoch(time_t value) {
    return value >= 1704067200; // 2024-01-01 UTC
}

time_t TimeManager::tmToUtc(const tm& value) {
    // timegm() is not exposed by the ESP8266 Arduino toolchain. Convert a
    // Gregorian calendar date to UTC without depending on the configured TZ.
    int year = value.tm_year + 1900;
    unsigned month = static_cast<unsigned>(value.tm_mon + 1);
    unsigned day = static_cast<unsigned>(value.tm_mday);
    year -= month <= 2;
    const long era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned monthPrime = static_cast<unsigned>(static_cast<int>(month) + (month > 2 ? -3 : 9));
    const unsigned dayOfYear = (153 * monthPrime + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    const long days = era * 146097 + static_cast<long>(dayOfEra) - 719468;
    return static_cast<time_t>(days) * 86400 + value.tm_hour * 3600 + value.tm_min * 60 + value.tm_sec;
}

void TimeManager::begin(JsonObjectConst config, const String& timezone) {
    sntp_stop();
    ntpSynced.store(false);
    _timezone = timezone;
    _mode = config["source"] | "auto";
    JsonObjectConst ds3231 = config["ds3231"].as<JsonObjectConst>();
    _ds3231Address = ds3231["address"] | 0x68;
    _ds3231Enabled = ds3231["enabled"] | true;
    _ntpConfigured = config["ntpEnabled"] | true;
    diagnostics.log("RTC", String("DS3231 init: enabled=") + (_ds3231Enabled ? "true" : "false") + " address=0x" + String(_ds3231Address, HEX) + " (decimal " + String(_ds3231Address) + ")");
    const char* tz = "UTC0";
    if (_timezone == "Asia/Jakarta") tz = "WIB-7";
    else if (_timezone == "Asia/Makassar") tz = "WITA-8";
    else if (_timezone == "Asia/Jayapura") tz = "WIT-9";
    setenv("TZ", tz, 1);
    tzset(); // also required when NTP is disabled
    _clockValid = _clockValid && isValidEpoch(time(nullptr));
    if (!_clockValid) _source = "unsynced";
    _ntpAvailable = false;
    _lastNtpSync = 0;
    _rtcSyncPending = false;
    _ds3231Checked = false;
    _ntpRunning = false;
    _lastCheckMs = millis() - 10000;
    tick(); // RTC bootstrap before Wi-Fi, without waiting for the network
    if (!_clockValid) diagnostics.log("TIME", "No valid clock; schedule automations disabled, other functions remain available");
#ifdef ESP8266
    settimeofday_cb([](bool fromSntp) { if (fromSntp) ntpSynced.store(true); });
    if (_ntpConfigured) configTime(tz, "pool.ntp.org", "time.nist.gov");
#else
    sntp_set_time_sync_notification_cb([](timeval*) { ntpSynced.store(true); });
    if (_ntpConfigured) configTzTime(tz, "pool.ntp.org", "time.nist.gov");
#endif
    _ntpRunning = _ntpConfigured;
}

bool TimeManager::readDs3231(tm& value) {
    _rtcValid = false;
    if (!_ds3231Enabled) return false;
    Wire.beginTransmission(_ds3231Address);
    Wire.write(0);
    if (Wire.endTransmission() != 0 || Wire.requestFrom((int)_ds3231Address, 7) != 7) return false;
    uint8_t raw[7];
    for (auto& byte : raw) byte = Wire.read();
    Wire.beginTransmission(_ds3231Address);
    Wire.write(0x0f);
    if (Wire.endTransmission() != 0 || Wire.requestFrom((int)_ds3231Address, 1) != 1) return false;
    if (Wire.read() & 0x80) return true; // oscillator-stop: present but untrusted
    auto bcdValid = [](uint8_t b) { return (b & 15) <= 9 && (b >> 4) <= 9; };
    if (!bcdValid(raw[0]) || !bcdValid(raw[1]) || !bcdValid(raw[2] & 0x3f) ||
        !bcdValid(raw[4]) || !bcdValid(raw[5] & 0x1f) || !bcdValid(raw[6])) return true;
    value = {};
    value.tm_sec = fromBcd(raw[0]);
    value.tm_min = fromBcd(raw[1]);
    if (raw[2] & 0x40) {
        int hour = fromBcd(raw[2] & 0x1f);
        if (hour < 1 || hour > 12) return true;
        value.tm_hour = hour % 12 + ((raw[2] & 0x20) ? 12 : 0);
    } else value.tm_hour = fromBcd(raw[2] & 0x3f);
    value.tm_mday = fromBcd(raw[4]);
    value.tm_mon = fromBcd(raw[5] & 0x1f) - 1;
    value.tm_year = fromBcd(raw[6]) + 100 + ((raw[5] & 0x80) ? 100 : 0);
    if (value.tm_mon < 0 || value.tm_mon > 11 || value.tm_mday < 1 || value.tm_mday > 31 ||
        value.tm_hour > 23 || value.tm_min > 59 || value.tm_sec > 59) return true;
    time_t epoch = tmToUtc(value);
    tm check{};
    gmtime_r(&epoch, &check);
    _rtcValid = isValidEpoch(epoch) && check.tm_year == value.tm_year &&
                check.tm_mon == value.tm_mon && check.tm_mday == value.tm_mday;
    return true;
}

bool TimeManager::writeDs3231(const tm& value) const {
    if (!_ds3231Enabled || value.tm_year < 100 || value.tm_year >= 300) return false;
    Wire.beginTransmission(_ds3231Address);
    Wire.write(0);
    Wire.write(toBcd(value.tm_sec));
    Wire.write(toBcd(value.tm_min));
    Wire.write(toBcd(value.tm_hour)); // 24-hour mode
    Wire.write(toBcd(static_cast<uint8_t>(value.tm_wday + 1))); // DS3231: Sunday=1
    Wire.write(toBcd(value.tm_mday));
    Wire.write(static_cast<uint8_t>(toBcd(value.tm_mon + 1) | (value.tm_year >= 200 ? 0x80 : 0)));
    Wire.write(toBcd(static_cast<uint8_t>((value.tm_year + 1900) % 100)));
    if (Wire.endTransmission() != 0) return false;
    Wire.beginTransmission(_ds3231Address);
    Wire.write(0x0f);
    if (Wire.endTransmission() != 0 || Wire.requestFrom((int)_ds3231Address, 1) != 1) return false;
    uint8_t status = Wire.read();
    Wire.beginTransmission(_ds3231Address);
    Wire.write(0x0f);
    Wire.write(static_cast<uint8_t>(status & ~0x80)); // clear OSF only after successful write
    return Wire.endTransmission() == 0;
}

bool TimeManager::writeSystemTime(const tm& value) const {
    time_t epoch = tmToUtc(value);
    if (!isValidEpoch(epoch)) return false;
    timeval tv{epoch, 0};
    return settimeofday(&tv, nullptr) == 0;
}

void TimeManager::tick() {
    uint32_t now = millis();
#ifdef ESP8266
    // Callback runs cooperatively on this core; no yield between load/store.
    bool synced = ntpSynced.load();
    ntpSynced.store(false);
#else
    bool synced = ntpSynced.exchange(false);
#endif
    if (synced && _ntpConfigured && isValidEpoch(time(nullptr))) {
        _clockValid = true;
        _source = "ntp";
        _ntpAvailable = true;
        _lastNtpSync = time(nullptr);
        _rtcSyncPending = _ds3231Enabled;
        _lastCheckMs = now - 10000;
        diagnostics.log("TIME", "NTP synchronization completed");
    }
    if (now - _lastCheckMs < 10000) return;
    _lastCheckMs = now;
    tm value{};
    bool previousAvailable = _ds3231Available;
    _ds3231Available = readDs3231(value);
    if (_ds3231Available) {
        if (!previousAvailable) diagnostics.log("RTC", String("DS3231 detected at address=0x") + String(_ds3231Address, HEX) + " (decimal " + String(_ds3231Address) + ")");
        _ds3231Checked = true;
        if (!_clockValid && _mode != "ntp" && _rtcValid) {
            if (writeSystemTime(value)) {
                _clockValid = true;
                _source = "ds3231";
                diagnostics.log("RTC", "System clock initialized from DS3231");
            }
        }
        if (_rtcSyncPending && _clockValid) {
            time_t current = time(nullptr);
            double difference = _rtcValid ? difftime(current, tmToUtc(value)) : 0;
            tm corrected{};
            gmtime_r(&current, &corrected);
            if ((_rtcValid && difference >= -2 && difference <= 2) || writeDs3231(corrected)) {
                _rtcSyncPending = false;
                _rtcValid = true;
                diagnostics.log("RTC", "DS3231 synchronized with NTP (UTC)");
            }
        }
        if (_mode == "ds3231" && _ntpAvailable && !_rtcSyncPending && _rtcValid && _ntpRunning) {
            sntp_stop(); // one NTP calibration per configuration/boot in RTC mode
            _ntpRunning = false;
            _source = "ds3231";
        }
    } else {
        if (!_ds3231Checked || previousAvailable) diagnostics.log("RTC", String("DS3231 not detected at address=0x") + String(_ds3231Address, HEX) + " (decimal " + String(_ds3231Address) + ")");
        _ds3231Checked = true;
    }

    _clockValid = _clockValid && isValidEpoch(time(nullptr));
}

void TimeManager::fillStatus(JsonObject doc) const {
    time_t current = time(nullptr);
    doc["source"] = _source;
    doc["clockValid"] = _clockValid && isValidEpoch(current);
    doc["mode"] = _mode;
    doc["ds3231Valid"] = _rtcValid;
    doc["lastNtpSync"] = (uint32_t)_lastNtpSync;
    doc["ntpRunning"] = _ntpRunning;
    doc["ds3231Enabled"] = _ds3231Enabled;
    doc["ds3231Available"] = _ds3231Available;
    doc["ds3231Address"] = _ds3231Address;
    doc["ntpEnabled"] = _ntpConfigured;
    doc["ntpAvailable"] = _ntpAvailable;
    doc["epoch"] = (uint32_t)current;
    String iso = String(ctime(&current));
    iso.trim();
    doc["iso"] = iso;
}
