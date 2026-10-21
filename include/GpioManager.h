#pragma once
#include <Arduino.h>

class GpioManager {
public:
    bool configure(uint8_t gpio, const String& mode, bool pullup, uint8_t initialState);
    int read(uint8_t gpio) const;
    bool write(uint8_t gpio, uint8_t value);
    bool valid(uint8_t gpio) const;
};
