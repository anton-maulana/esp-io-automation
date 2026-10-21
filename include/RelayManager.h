#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PinManager.h"
#include "PcfManager.h"
#include "GpioManager.h"

struct RelayConfig {
    String id;
    String name;
    String room;
    String icon;
    String pinId;
    uint8_t activeLevel;
    bool enabled;
    bool publicControl;
    String startupPolicy;
    int8_t pinIndex;
    bool desiredState;
    bool actualState;
};

class RelayManager {
public:
    void configure(JsonArrayConst relays, PinManager& pins);
    bool setState(uint8_t index, bool on, PinManager& pins, PcfManager& pcf, GpioManager& gpio);
    bool toggle(uint8_t index, PinManager& pins, PcfManager& pcf, GpioManager& gpio);
    bool getState(uint8_t index) const;
    uint8_t count() const;
    const RelayConfig& relay(uint8_t index) const;
    int findById(const String& id) const;
    void applyStartup(PinManager& pins, PcfManager& pcf, GpioManager& gpio);

private:
    RelayConfig _relays[64];
    uint8_t _count = 0;
};
