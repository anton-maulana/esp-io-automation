#include "PinManager.h"

void PinManager::configure(JsonArrayConst pins, PcfManager& pcf, GpioManager& gpioMgr) {
    _count = 0;
    for (JsonVariantConst v : pins) {
        if (_count >= 64) break;
        PinConfig& p = _pins[_count++];
        p.id = v["id"] | "";
        p.name = v["name"] | "";
        p.source = v["source"] | "pcf";
        p.pcfId = v["pcfId"] | "";
        p.pin = v["pin"] | 0;
        p.gpio = v["gpio"] | 0;
        p.mode = v["mode"] | "output";
        p.pullup = v["pullup"] | false;
        p.initialState = v["initialState"] | 1;
        p.safeState = v["safeState"] | 1;
        p.enabled = v["enabled"] | true;
        p.debounceMs = v["debounceMs"] | 0;
        p.pcfIndex = p.source == "pcf" ? pcf.findById(p.pcfId) : -1;
        p.lastState = false;
        p.lastChangeMs = 0;
        if (p.source == "gpio") gpioMgr.configure(p.gpio, p.mode, p.pullup, p.initialState);
    }
    for (uint8_t i = 0; i < _count; i++) {
        PinConfig& p = _pins[i];
        if (!p.enabled) continue;
        if (p.source == "pcf" && p.pcfIndex >= 0) {
            if (p.mode == "input" && p.pullup) pcf.writeBit(p.pcfIndex, p.pin, true);
            else if (p.mode == "output") pcf.writeBit(p.pcfIndex, p.pin, p.initialState);
        }
    }
}

int PinManager::read(uint8_t index, PcfManager& pcf, GpioManager& gpioMgr) {
    if (index >= _count) return -1;
    const PinConfig& p = _pins[index];
    if (p.source == "gpio") return gpioMgr.read(p.gpio);
    if (p.pcfIndex < 0) return -1;
    return bitRead(pcf.readPort(p.pcfIndex), p.pin);
}

bool PinManager::write(uint8_t index, uint8_t value, PcfManager& pcf, GpioManager& gpioMgr) {
    if (index >= _count || _pins[index].mode != "output") return false;
    PinConfig& p = _pins[index];
    if (p.source == "gpio") return gpioMgr.write(p.gpio, value);
    return p.pcfIndex >= 0 && pcf.writeBit(p.pcfIndex, p.pin, value);
}

void PinManager::tick(PcfManager& pcf, GpioManager& gpioMgr, void (*onEdge)(uint8_t, bool)) {
    for (uint8_t i = 0; i < _count; i++) {
        PinConfig& p = _pins[i];
        if (!p.enabled || p.mode != "input") continue;
        int value = read(i, pcf, gpioMgr);
        if (value < 0) continue;
        bool current = value != 0;
        uint32_t now = millis();
        if (current != p.lastState && now - p.lastChangeMs >= p.debounceMs) {
            p.lastState = current;
            p.lastChangeMs = now;
            if (onEdge) onEdge(i, current);
        }
    }
}

uint8_t PinManager::count() const { return _count; }
const PinConfig& PinManager::pin(uint8_t index) const { return _pins[index]; }
int PinManager::findById(const String& id) const { for (uint8_t i = 0; i < _count; i++) if (_pins[i].id == id) return i; return -1; }
String PinManager::getMode(uint8_t index) const { return _pins[index].mode; }
