#include "AutomationEngine.h"
#include "HttpQueue.h"
#include <time.h>

AutomationEngine::TriggerType AutomationEngine::parseTriggerType(const char* value) {
    if (String(value) == "pin_change") return TriggerType::PinChange;
    if (String(value) == "schedule") return TriggerType::Schedule;
    return TriggerType::Unsupported;
}

AutomationEngine::Edge AutomationEngine::parseEdge(const char* value) {
    if (String(value) == "rising") return Edge::Rising;
    if (String(value) == "falling") return Edge::Falling;
    return Edge::Any;
}

AutomationEngine::ActionType AutomationEngine::parseActionType(const char* value) {
    if (String(value) == "set_relay") return ActionType::SetRelay;
    if (String(value) == "toggle_relay") return ActionType::ToggleRelay;
    if (String(value) == "http") return ActionType::Http;
    return ActionType::Unsupported;
}

void AutomationEngine::configure(JsonArrayConst automations, PinManager& pins, RelayManager& relays,
                                 PcfManager& pcf, GpioManager& gpio) {
    _pins = &pins;
    _relays = &relays;
    _pcf = &pcf;
    _gpio = &gpio;
    _count = 0;
    _actionCount = 0;
    _changedCount = 0;
    for (JsonVariantConst v : automations) {
        if (_count >= CapacityLimits::MaxAutomations) break;
        Automation& a = _autos[_count++];
        a = Automation{};
        a.enabled = v["enabled"] | true;
        a.triggerType = parseTriggerType(v["trigger"]["type"] | "");
        a.triggerPinIndex = pins.findById(v["trigger"]["pinId"] | "");
        a.triggerEdge = parseEdge(v["trigger"]["edge"] | "any");
        a.scheduleHour = v["trigger"]["hour"] | -1;
        a.scheduleMinute = v["trigger"]["minute"] | -1;
        a.firstAction = _actionCount;
        for (JsonVariantConst action : v["actions"].as<JsonArrayConst>()) {
            if (_actionCount >= CapacityLimits::MaxActions) break;
            Action& item = _actions[_actionCount++];
            item = Action{};
            item.type = parseActionType(action["type"] | "");
            item.relayIndex = relays.findById(action["relayId"] | "");
            item.state = action["state"] | false;
            if (item.type == ActionType::Http) {
                item.method = action["method"] | "POST";
                item.path = action["path"] | "/";
                item.body = action["body"] | "";
            }
            a.actionCount++;
        }
    }
}

void AutomationEngine::runEventActions(const Automation& automation, PinManager& pins, RelayManager& relays,
                                       PcfManager& pcf, GpioManager& gpio, HttpQueue& http) {
    for (uint16_t i = automation.firstAction; i < automation.firstAction + automation.actionCount; i++) {
        Action& action = _actions[i];
        if (action.type == ActionType::ToggleRelay && action.relayIndex >= 0) relays.toggle(action.relayIndex, pins, pcf, gpio);
        else if (action.type == ActionType::SetRelay && action.relayIndex >= 0) relays.setState(action.relayIndex, action.state, pins, pcf, gpio);
        else if (action.type == ActionType::Http) http.enqueue(action.method, action.path, action.body);
    }
}

void AutomationEngine::onEvent(const DeviceEvent& event, PinManager& pins,
                               RelayManager& relays, PcfManager& pcf, GpioManager& gpio, HttpQueue& http) {
    if (_depth >= MAX_DEPTH) return;
    _depth++;
    for (uint8_t i = 0; i < _count; i++) {
        Automation& a = _autos[i];
        if (!a.enabled || a.triggerType != TriggerType::PinChange || event.type != EventType::PinChanged || a.triggerPinIndex != event.pinIndex) continue;
        bool rising = event.value > 0;
        if (a.triggerEdge == Edge::Rising && !rising) continue;
        if (a.triggerEdge == Edge::Falling && rising) continue;
        runEventActions(a, pins, relays, pcf, gpio, http);
    }
    _depth--;
}

void AutomationEngine::runScheduleActions(const Automation& automation) {
    for (uint16_t i = automation.firstAction; i < automation.firstAction + automation.actionCount; i++) {
        Action& action = _actions[i];
        bool changed = false;
        if (action.type == ActionType::ToggleRelay && action.relayIndex >= 0) changed = _relays->toggle(action.relayIndex, *_pins, *_pcf, *_gpio);
        else if (action.type == ActionType::SetRelay && action.relayIndex >= 0) changed = _relays->setState(action.relayIndex, action.state, *_pins, *_pcf, *_gpio);
        if (changed && _changedCount < CapacityLimits::MaxChangedRelays) _changedRelays[_changedCount++] = static_cast<uint8_t>(action.relayIndex);
    }
}

void AutomationEngine::tick() {
    if (!_pins || !_relays || !_pcf || !_gpio) return;
    time_t now = time(nullptr);
    if (now < 1704067200) return;
    tm localTime;
    localtime_r(&now, &localTime);
    int32_t minuteKey = static_cast<int32_t>(now / 60);
    for (uint8_t i = 0; i < _count; i++) {
        Automation& a = _autos[i];
        if (!a.enabled || a.triggerType != TriggerType::Schedule || a.scheduleHour != localTime.tm_hour ||
            a.scheduleMinute != localTime.tm_min || a.lastScheduleMinute == minuteKey) continue;
        a.lastScheduleMinute = minuteKey;
        runScheduleActions(a);
    }
}

bool AutomationEngine::consumeRelayChange(uint8_t& relayIndex) {
    if (!_changedCount) return false;
    relayIndex = _changedRelays[0];
    for (uint8_t i = 1; i < _changedCount; i++) _changedRelays[i - 1] = _changedRelays[i];
    _changedCount--;
    return true;
}

uint8_t AutomationEngine::count() const { return _count; }
uint16_t AutomationEngine::actionCount() const { return _actionCount; }
