#include "ConfigManager.h"
#include <LittleFS.h>

bool ConfigManager::begin() { return LittleFS.begin(); }

bool ConfigManager::load() {
    File file = LittleFS.open(_configPath, "r");
    if (!file) return false;
    DeserializationError error = deserializeJson(_doc, file);
    file.close();
    if (error) return false;
    JsonArrayConst pins = _doc["pins"].as<JsonArrayConst>();
    JsonArrayConst relays = _doc["relays"].as<JsonArrayConst>();
    JsonArray automations = _doc["automations"].as<JsonArray>();
    for (int i = static_cast<int>(automations.size()) - 1; i >= 0; i--) {
        String triggerType = automations[i]["trigger"]["type"] | "";
        bool valid = true;
        if (triggerType == "pin_change") {
            String pinId = automations[i]["trigger"]["pinId"] | "";
            valid = false;
            for (JsonVariantConst pin : pins) if (String(pin["id"] | "") == pinId) { valid = true; break; }
        }
        for (JsonVariantConst action : automations[i]["actions"].as<JsonArrayConst>()) {
            String relayId = action["relayId"] | "";
            bool found = false;
            for (JsonVariantConst relay : relays) if (String(relay["id"] | "") == relayId) { found = true; break; }
            if (!found) { valid = false; break; }
        }
        if (!valid) automations.remove(i);
    }
    return true;
}

bool ConfigManager::save() {
    return save(_doc);
}

bool ConfigManager::save(const JsonDocument& candidate) {
    _lastSaveError = "";
    if (candidate.overflowed()) { _lastSaveError = "json_overflow"; return false; }
#ifdef ESP8266
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxBlock = ESP.getMaxFreeBlockSize();
    if (freeHeap < 4096 || maxBlock < 2048) {
        _lastSaveError = String("low_memory:") + freeHeap + "/" + maxBlock;
        return false;
    }
#endif
    const String temporaryPath = _configPath + ".tmp";
    LittleFS.remove(temporaryPath);
    File file = LittleFS.open(temporaryPath, "w");
    if (!file) { _lastSaveError = "open_failed"; return false; }
    const size_t expected = measureJson(candidate);
    bool ok = serializeJson(candidate, file) == expected && expected > 0;
    file.close();
    if (!ok) { _lastSaveError = "write_failed"; LittleFS.remove(temporaryPath); return false; }
    if (!backup()) { _lastSaveError = "backup_failed"; LittleFS.remove(temporaryPath); return false; }
    if (LittleFS.rename(temporaryPath, _configPath)) return true;
    _lastSaveError = "rename_failed";
    restore();
    LittleFS.remove(temporaryPath);
    return false;
}

const String& ConfigManager::lastSaveError() const { return _lastSaveError; }

bool ConfigManager::backup() {
    if (!LittleFS.exists(_configPath)) return true;
    LittleFS.remove(_backupPath);
    return LittleFS.rename(_configPath, _backupPath);
}

bool ConfigManager::restore() {
    if (!LittleFS.exists(_backupPath)) return false;
    LittleFS.remove(_configPath);
    return LittleFS.rename(_backupPath, _configPath);
}

JsonDocument& ConfigManager::doc() { return _doc; }
const JsonDocument& ConfigManager::doc() const { return _doc; }

bool ConfigManager::loadState() {
    File file = LittleFS.open(_statePath, "r");
    if (!file) return false;
    DeserializationError error = deserializeJson(_stateDoc, file);
    file.close();
    return !error;
}

bool ConfigManager::saveState() {
    File file = LittleFS.open(_statePath, "w");
    if (!file) return false;
    bool ok = serializeJson(_stateDoc, file) > 0;
    file.close();
    return ok;
}

JsonDocument& ConfigManager::stateDoc() { return _stateDoc; }
