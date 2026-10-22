#include "Diagnostics.h"

Diagnostics diagnostics;

void Diagnostics::begin(bool enabled, const String& level) {
    _enabled = enabled;
    _level = level;
}

void Diagnostics::log(const char* level, const String& message) {
    if (!_enabled) return;
    Serial.printf("[%s] %s\n", level, message.c_str());
}

bool Diagnostics::enabled() const {
    return _enabled;
}

void Diagnostics::setEnabled(bool enabled) {
    _enabled = enabled;
}
