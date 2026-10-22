#pragma once
#include <ArduinoJson.h>

struct ValidationResult {
    bool valid;
    String error;
};

class ConfigValidator {
public:
    ValidationResult validate(const JsonDocument& doc);

private:
    bool checkUniqueIds(JsonArrayConst arr, const char* field, String& error);
    bool checkPcfAddresses(JsonArrayConst pcfs, String& error);
    bool checkPinRefs(const JsonDocument& doc, String& error);
    bool checkRelayRefs(const JsonDocument& doc, String& error);
    bool checkAutomationRefs(const JsonDocument& doc, String& error);
    bool checkCapacity(const JsonDocument& doc, String& error);
};
