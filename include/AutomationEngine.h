#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "PinManager.h"
#include "RelayManager.h"
#include "PcfManager.h"
#include "GpioManager.h"
#include "CapacityLimits.h"

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
    uint16_t actionCount() const;

private:
    enum class TriggerType : uint8_t { PinChange, Schedule, Unsupported };
    enum class Edge : uint8_t { Any, Rising, Falling };
    enum class ActionType : uint8_t { SetRelay, ToggleRelay, Http, Unsupported };

    struct Action {
        ActionType type = ActionType::Unsupported;
        int8_t relayIndex = -1;
        bool state = false;
        String method;
        String path;
        String body;
    };

    struct Automation {
        bool enabled = false;
        TriggerType triggerType = TriggerType::Unsupported;
        Edge triggerEdge = Edge::Any;
        int8_t triggerPinIndex = -1;
        int8_t scheduleHour = -1;
        int8_t scheduleMinute = -1;
        int32_t lastScheduleMinute = -1;
        uint16_t firstAction = 0;
        uint16_t actionCount = 0;
    };

    Automation _autos[CapacityLimits::MaxAutomations];
    Action _actions[CapacityLimits::MaxActions];
    uint8_t _count = 0;
    uint16_t _actionCount = 0;
    uint8_t _depth = 0;
    uint8_t _changedRelays[CapacityLimits::MaxChangedRelays] = {};
    uint8_t _changedCount = 0;
    PinManager* _pins = nullptr;
    RelayManager* _relays = nullptr;
    PcfManager* _pcf = nullptr;
    GpioManager* _gpio = nullptr;
    static constexpr uint8_t MAX_DEPTH = 4;

    static TriggerType parseTriggerType(const char* value);
    static Edge parseEdge(const char* value);
    static ActionType parseActionType(const char* value);
    void runEventActions(const Automation& automation, PinManager& pins, RelayManager& relays,
                         PcfManager& pcf, GpioManager& gpio, HttpQueue& http);
    void runScheduleActions(const Automation& automation);
};
