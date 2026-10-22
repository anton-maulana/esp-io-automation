#include "ConfigValidator.h"
#include "CapacityLimits.h"

static bool validLength(JsonVariantConst value, size_t limit) {
    return !value.is<const char*>() || String(value.as<const char*>()).length() <= limit;
}

bool ConfigValidator::checkUniqueIds(JsonArrayConst arr, const char* field, String& error) {
    for (size_t i = 0; i < arr.size(); i++) {
        const char* value = arr[i][field] | "";
        if (!*value) { error = String("Missing ") + field; return false; }
        for (size_t j = i + 1; j < arr.size(); j++) {
            if (String(arr[j][field] | "") == value) { error = String("Duplicate ") + field + ": " + value; return false; }
        }
    }
    return true;
}

bool ConfigValidator::checkPcfAddresses(JsonArrayConst pcfs, String& error) {
    for (size_t i = 0; i < pcfs.size(); i++) {
        int address = pcfs[i]["address"] | -1;
        if (address < 0x20 || address > 0x3f || (address > 0x27 && address < 0x38)) { error = "Invalid PCF address"; return false; }
        for (size_t j = i + 1; j < pcfs.size(); j++) if ((pcfs[j]["address"] | -1) == address) { error = "Duplicate PCF address"; return false; }
    }
    return true;
}

bool ConfigValidator::checkPinRefs(const JsonDocument& doc, String& error) {
    JsonArrayConst pcfs = doc["hardware"]["pcfs"].as<JsonArrayConst>();
    for (JsonVariantConst pin : doc["pins"].as<JsonArrayConst>()) {
        String source = pin["source"] | "pcf";
        if (source == "gpio") {
            int gpio = pin["gpio"] | -1;
            if (gpio < 0 || gpio > 16) { error = "Invalid ESP8266 GPIO"; return false; }
            continue;
        }
        if (source != "pcf") { error = "Invalid pin source"; return false; }
        int number = pin["pin"] | -1;
        if (number < 0 || number > 7) { error = "Pin must be between 0 and 7"; return false; }
        bool found = false;
        for (JsonVariantConst pcf : pcfs) if (String(pcf["id"] | "") == String(pin["pcfId"] | "")) found = true;
        if (!found) { error = "Pin references an unknown PCF"; return false; }
    }
    JsonArrayConst pins = doc["pins"].as<JsonArrayConst>();
    for (size_t i = 0; i < pins.size(); i++) {
        for (size_t j = i + 1; j < pins.size(); j++) {
            String source = pins[i]["source"] | "pcf";
            String otherSource = pins[j]["source"] | "pcf";
            if (source == "gpio" && otherSource == "gpio" &&
                (pins[i]["gpio"] | -1) == (pins[j]["gpio"] | -2)) { error = "Duplicate GPIO pin"; return false; }
            if (source == "pcf" && otherSource == "pcf" &&
                String(pins[i]["pcfId"] | "") == String(pins[j]["pcfId"] | "") &&
                (pins[i]["pin"] | -1) == (pins[j]["pin"] | -2)) { error = "Duplicate PCF pin"; return false; }
        }
    }
    return true;
}

bool ConfigValidator::checkRelayRefs(const JsonDocument& doc, String& error) {
    JsonArrayConst relays = doc["relays"].as<JsonArrayConst>();
    for (size_t i = 0; i < relays.size(); i++) {
        JsonVariantConst relay = relays[i];
        bool found = false;
        for (JsonVariantConst pin : doc["pins"].as<JsonArrayConst>()) {
            if (String(pin["id"] | "") == String(relay["pinId"] | "")) { found = true; if (String(pin["mode"] | "") != "output") { error = "Relay must use an output pin"; return false; } }
        }
        if (!found) { error = "Relay references an unknown pin"; return false; }
        if (relay["activeLevel"].is<int>()) {
            int activeLevel = relay["activeLevel"] | 1;
            if (activeLevel != 0 && activeLevel != 1) { error = "Relay activeLevel must be 0 or 1"; return false; }
        }
        for (size_t j = i + 1; j < relays.size(); j++)
            if (String(relays[j]["pinId"] | "") == String(relay["pinId"] | "")) {
                error = "Pin is already used by another relay";
                return false;
            }
    }
    return true;
}

bool ConfigValidator::checkAutomationRefs(const JsonDocument& doc, String& error) {
    for (JsonVariantConst automation : doc["automations"].as<JsonArrayConst>()) {
        JsonObjectConst trigger = automation["trigger"].as<JsonObjectConst>();
        String type = trigger["type"] | "";
        if (type == "pin_change") {
            String pinId = trigger["pinId"] | "";
            bool found = false;
            for (JsonVariantConst pin : doc["pins"].as<JsonArrayConst>()) if (String(pin["id"] | "") == pinId) found = true;
            if (!found) { error = "Automation references an unknown pin"; return false; }
        } else if (type == "schedule") {
            int hour = trigger["hour"] | -1;
            int minute = trigger["minute"] | -1;
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59) { error = "Invalid schedule time"; return false; }
        } else if (type == "request") {
            if (String(trigger["url"] | "").length() == 0) { error = "Request trigger URL is required"; return false; }
        } else { error = "Unsupported trigger"; return false; }
        for (JsonVariantConst action : automation["actions"].as<JsonArrayConst>()) {
            String relayId = action["relayId"] | "";
            bool found = false;
            for (JsonVariantConst relay : doc["relays"].as<JsonArrayConst>()) if (String(relay["id"] | "") == relayId) found = true;
            if (!found) { error = "Automation references an unknown relay"; return false; }
        }
    }
    return true;
}

bool ConfigValidator::checkCapacity(const JsonDocument& doc, String& error) {
    JsonArrayConst pcfs = doc["hardware"]["pcfs"].as<JsonArrayConst>();
    JsonArrayConst pins = doc["pins"].as<JsonArrayConst>();
    JsonArrayConst relays = doc["relays"].as<JsonArrayConst>();
    JsonArrayConst automations = doc["automations"].as<JsonArrayConst>();
    if (pcfs.size() > CapacityLimits::MaxPcf) { error = "Too many PCF chips"; return false; }
    if (pins.size() > CapacityLimits::MaxPins) { error = "Too many pins"; return false; }
    if (relays.size() > CapacityLimits::MaxRelays) { error = "Too many relays"; return false; }
    if (automations.size() > CapacityLimits::MaxAutomations) { error = "Too many automations"; return false; }
    size_t actions = 0;
    for (JsonVariantConst automation : automations) {
        JsonArrayConst items = automation["actions"].as<JsonArrayConst>();
        actions += items.size();
        if (actions > CapacityLimits::MaxActions) { error = "Too many automation actions"; return false; }
        if (!validLength(automation["id"], CapacityLimits::MaxIdLength) ||
            !validLength(automation["name"], CapacityLimits::MaxNameLength)) { error = "Automation text is too long"; return false; }
        for (JsonVariantConst action : items) {
            String type = action["type"] | "";
            if (type == "http" && (!validLength(action["path"], CapacityLimits::MaxHttpPathLength) ||
                                    !validLength(action["body"], CapacityLimits::MaxHttpBodyLength))) {
                error = "HTTP action is too long";
                return false;
            }
        }
    }
    for (JsonVariantConst item : pins) {
        if (!validLength(item["id"], CapacityLimits::MaxIdLength) || !validLength(item["name"], CapacityLimits::MaxNameLength)) { error = "Pin text is too long"; return false; }
    }
    for (JsonVariantConst item : relays) {
        if (!validLength(item["id"], CapacityLimits::MaxIdLength) || !validLength(item["name"], CapacityLimits::MaxNameLength)) { error = "Relay text is too long"; return false; }
    }
    return true;
}

ValidationResult ConfigValidator::validate(const JsonDocument& doc) {
    String error;
    if ((doc["schemaVersion"] | 0) != 1) error = "Unsupported schema version";
    else if (String(doc["time"]["source"] | "auto") != "auto" &&
             String(doc["time"]["source"] | "auto") != "ntp" &&
             String(doc["time"]["source"] | "auto") != "ds3231") error = "Invalid time source";
    else if ((doc["time"]["ds3231"]["address"] | 0x68) != 0x68) error = "DS3231 address must be 104 (0x68)";
    else if (String(doc["time"]["source"] | "auto") == "ds3231" &&
             !(doc["time"]["ds3231"]["enabled"] | true)) error = "DS3231 time source requires DS3231 enabled";
    else if (String(doc["time"]["source"] | "auto") == "ntp" &&
             !(doc["time"]["ntpEnabled"] | true)) error = "NTP time source requires NTP enabled";
    else if (!checkUniqueIds(doc["hardware"]["pcfs"].as<JsonArrayConst>(), "id", error)) {}
    else if (!checkUniqueIds(doc["pins"].as<JsonArrayConst>(), "id", error)) {}
    else if (!checkUniqueIds(doc["relays"].as<JsonArrayConst>(), "id", error)) {}
    else if (!checkUniqueIds(doc["automations"].as<JsonArrayConst>(), "id", error)) {}
    else if (!checkPcfAddresses(doc["hardware"]["pcfs"].as<JsonArrayConst>(), error)) {}
    else if (!checkPinRefs(doc, error)) {}
    else if (!checkRelayRefs(doc, error)) {}
    else if (!checkAutomationRefs(doc, error)) {}
    else if (!checkCapacity(doc, error)) {}
    return { error.length() == 0, error };
}
