#include "RelayManager.h"

void RelayManager::configure(JsonArrayConst relays, PinManager& pins) {
    _count = 0;
    for (JsonVariantConst v : relays) {
        if (_count >= CapacityLimits::MaxRelays) break;
        RelayConfig& r = _relays[_count++];
        r.id = v["id"] | "";
        r.name = v["name"] | "";
        r.room = v["room"] | "";
        r.icon = v["icon"] | "switch";
        r.pinId = v["pinId"] | "";
        r.activeLevel = v["activeLevel"] | 0;
        r.enabled = v["enabled"] | true;
        r.publicControl = v["publicControl"] | false;
        r.startupPolicy = v["startupPolicy"] | "off";
        r.pinIndex = pins.findById(r.pinId);
        r.desiredState = false;
        r.actualState = false;
    }
}

bool RelayManager::setState(uint8_t index, bool on, PinManager& pins, PcfManager& pcf, GpioManager& gpio) {
    if (index >= _count || !_relays[index].enabled || _relays[index].pinIndex < 0) return false;
    _relays[index].desiredState = on;
    uint8_t pinVal = on ? _relays[index].activeLevel : !_relays[index].activeLevel;
    bool ok = pins.write(_relays[index].pinIndex, pinVal, pcf, gpio);
    if (ok) _relays[index].actualState = on;
    return ok;
}

bool RelayManager::toggle(uint8_t index, PinManager& pins, PcfManager& pcf, GpioManager& gpio) {
    return index < _count && setState(index, !_relays[index].actualState, pins, pcf, gpio);
}

bool RelayManager::getState(uint8_t index) const { return index < _count && _relays[index].actualState; }
uint8_t RelayManager::count() const { return _count; }
const RelayConfig& RelayManager::relay(uint8_t index) const { return _relays[index]; }
int RelayManager::findById(const String& id) const { for (uint8_t i = 0; i < _count; i++) if (_relays[i].id == id) return i; return -1; }

void RelayManager::applyStartup(PinManager& pins, PcfManager& pcf, GpioManager& gpio) {
    for (uint8_t i = 0; i < _count; i++) {
        if (_relays[i].enabled) setState(i, _relays[i].startupPolicy == "on", pins, pcf, gpio);
    }
}
