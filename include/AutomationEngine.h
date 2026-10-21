#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PinManager.h"
#include "RelayManager.h"
#include "PcfManager.h"
#include "GpioManager.h"

enum class EventType : uint8_t {
    PinChanged,
    RelayChanged,
    HttpCompleted,
    HttpFailed
};

struct DeviceEvent {
    EventType type;
    uint8_t pinIndex;
    int8_t value;
    uint32_t timestamp;
};

class HttpQueue;

class AutomationEngine {
public:
    void configure(JsonArrayConst automations, PinManager& pins, RelayManager& relays);
    void onEvent(const DeviceEvent& event, PinManager& pins,
                 RelayManager& relays, PcfManager& pcf, GpioManager& gpio, HttpQueue& http);
    void tick();
    uint8_t count() const;

private:
    struct Automation {
        String id;
        String name;
        bool enabled;
        String triggerType;
        String triggerPinId;
        String triggerEdge;
        int8_t triggerPinIndex;
        JsonDocument actions;
    };

    Automation _autos[20];
    uint8_t _count = 0;
    uint8_t _depth = 0;
    static constexpr uint8_t MAX_DEPTH = 4;
};
