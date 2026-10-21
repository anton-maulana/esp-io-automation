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
    void configure(JsonArrayConst automations, PinManager& pins, RelayManager& relays,
                   PcfManager& pcf, GpioManager& gpio);
    void onEvent(const DeviceEvent& event, PinManager& pins,
                 RelayManager& relays, PcfManager& pcf, GpioManager& gpio, HttpQueue& http);
    void tick();
    bool consumeRelayChange(uint8_t& relayIndex);
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
        int8_t scheduleHour;
        int8_t scheduleMinute;
        int32_t lastScheduleMinute = -1;
        JsonDocument actions;
    };

    Automation _autos[20];
    uint8_t _count = 0;
    uint8_t _depth = 0;
    uint8_t _changedRelays[20] = {};
    uint8_t _changedCount = 0;
    PinManager* _pins = nullptr;
    RelayManager* _relays = nullptr;
    PcfManager* _pcf = nullptr;
    GpioManager* _gpio = nullptr;
    static constexpr uint8_t MAX_DEPTH = 4;
};
