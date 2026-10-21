#include "AutomationEngine.h"
#include "HttpQueue.h"

void AutomationEngine::configure(JsonArrayConst automations, PinManager& pins, RelayManager&) {
    _count = 0;
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

void AutomationEngine::tick() {}
uint8_t AutomationEngine::count() const { return _count; }
