#pragma once
#include <Arduino.h>

class Diagnostics {
public:
    void begin(bool enabled, const String& level);
    void log(const char* level, const String& message);
    bool enabled() const;
    void setEnabled(bool enabled);

private:
    bool _enabled = false;
    String _level = "info";
};

extern Diagnostics diagnostics;
