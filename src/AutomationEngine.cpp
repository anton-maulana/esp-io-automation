#include "AutomationEngine.h"
#include "HttpQueue.h"
#include <time.h>

void AutomationEngine::configure(JsonArrayConst automations, PinManager& pins, RelayManager& relays,
                                 PcfManager& pcf, GpioManager& gpio) {
    _pins = &pins;
    _relays = &relays;
    _pcf = &pcf;
    _gpio = &gpio;
    _count = 0;
    _changedCount = 0;
    for (JsonVariantConst v : automations) {
        if (_count >= 20) break;
        Automation& a = _autos[_count++];
        a.id = v["id"] | "";
        a.name = v["name"] | "";
        a.enabled = v["enabled"] | true;
        a.triggerType = v["trigger"]["type"] | "";
        a.triggerPinId = v["trigger"]["pinId"] | "";
        a.triggerEdge = v["trigger"]["edge"] | "any";
        a.triggerPinIndex = pins.findById(a.triggerPinId);
        a.scheduleHour = v["trigger"]["hour"] | -1;
        a.scheduleMinute = v["trigger"]["minute"] | -1;
        a.lastScheduleMinute = -1;
        a.actions.clear();
        a.actions.set(v["actions"]);
    }
}

void AutomationEngine::onEvent(const DeviceEvent& event, PinManager& pins,
                                RelayManager& relays, PcfManager& pcf, GpioManager& gpio, HttpQueue& http) {
    if (_depth >= MAX_DEPTH) return;
    _depth++;
    for (uint8_t i = 0; i < _count; i++) {
        Automation& a = _autos[i];
        if (!a.enabled || a.triggerType != "pin_change" || event.type != EventType::PinChanged || a.triggerPinIndex != event.pinIndex) continue;
        bool rising = event.value > 0;
        if (a.triggerEdge == "rising" && !rising) continue;
        if (a.triggerEdge == "falling" && rising) continue;
        for (JsonVariantConst action : a.actions.as<JsonArrayConst>()) {
            String type = action["type"] | "";
            int idx = relays.findById(action["relayId"] | "");
            if (type == "toggle_relay" && idx >= 0) relays.toggle(idx, pins, pcf, gpio);
            else if (type == "set_relay" && idx >= 0) relays.setState(idx, action["state"] | false, pins, pcf, gpio);
            else if (type == "http") http.enqueue(action["method"] | "POST", action["path"] | "/", action["body"].as<String>());
        }
    }
    _depth--;
}

void AutomationEngine::tick() {
    if (!_pins || !_relays || !_pcf || !_gpio) return;
    time_t now = time(nullptr);
    if (now < 946684800) return;
    tm localTime;
    localtime_r(&now, &localTime);
    int32_t minuteKey = static_cast<int32_t>(now / 60);
    for (uint8_t i = 0; i < _count; i++) {
        Automation& a = _autos[i];
        if (!a.enabled || a.triggerType != "schedule" ||
            a.scheduleHour != localTime.tm_hour || a.scheduleMinute != localTime.tm_min ||
            a.lastScheduleMinute == minuteKey) continue;
        a.lastScheduleMinute = minuteKey;
        for (JsonVariantConst action : a.actions.as<JsonArrayConst>()) {
            String type = action["type"] | "";
            int relayIndex = _relays->findById(action["relayId"] | "");
            bool changed = false;
            if (type == "toggle_relay" && relayIndex >= 0) {
                changed = _relays->toggle(relayIndex, *_pins, *_pcf, *_gpio);
            } else if (type == "set_relay" && relayIndex >= 0) {
                changed = _relays->setState(relayIndex, action["state"] | false, *_pins, *_pcf, *_gpio);
            }
            if (changed && _changedCount < sizeof(_changedRelays)) {
                _changedRelays[_changedCount++] = static_cast<uint8_t>(relayIndex);
            }
        }
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
