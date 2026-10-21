#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class ConfigManager {
public:
    bool begin();
    bool load();
    bool save();
    bool backup();
    bool restore();

    JsonDocument& doc();
    const JsonDocument& doc() const;

    bool loadState();
    bool saveState();
    JsonDocument& stateDoc();

private:
    JsonDocument _doc;
    JsonDocument _stateDoc;
    String _configPath = "/config.json";
    String _statePath = "/state.json";
    String _backupPath = "/config.bak";
};
