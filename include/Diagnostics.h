#pragma once
#include <Arduino.h>
class Diagnostics {
public:
    void begin(bool enabled, const String& level);
    void log(const char* level, const String& message);
};
