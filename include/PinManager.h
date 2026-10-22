#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PcfManager.h"
#include "GpioManager.h"
#include "CapacityLimits.h"

struct PinConfig {
    String id;
    String name;
    String source;
    String pcfId;
    uint8_t pin;
    uint8_t gpio;
    String mode;
    bool pullup;
    uint8_t initialState;
    uint8_t safeState;
    bool enabled;
    uint16_t debounceMs;
    int8_t pcfIndex;
    bool lastState;
    uint32_t lastChangeMs;
};

class PinManager {
public:
    void configure(JsonArrayConst pins, PcfManager& pcf, GpioManager& gpioMgr);
    int read(uint8_t index, PcfManager& pcf, GpioManager& gpioMgr);
    bool write(uint8_t index, uint8_t value, PcfManager& pcf, GpioManager& gpioMgr);
    void tick(PcfManager& pcf, GpioManager& gpioMgr, void (*onEdge)(uint8_t pinIndex, bool rising));
    uint8_t count() const;
    const PinConfig& pin(uint8_t index) const;
    int findById(const String& id) const;
    String getMode(uint8_t index) const;

private:
    PinConfig _pins[CapacityLimits::MaxPins];
    uint8_t _count = 0;
};
