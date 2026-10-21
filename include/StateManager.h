#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
class StateManager {
public:
    bool load(JsonDocument& doc);
    bool save(const JsonDocument& doc);
    void markDirty();
    bool dirty() const;
private: bool _dirty=false;
};
