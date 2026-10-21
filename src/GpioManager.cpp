#include "GpioManager.h"

bool GpioManager::configure(uint8_t gpio, const String& mode, bool pullup, uint8_t initialState) {
    if (!valid(gpio)) return false;
    if (mode == "output") {
        digitalWrite(gpio, initialState);
        pinMode(gpio, OUTPUT);
    } else {
        pinMode(gpio, pullup ? INPUT_PULLUP : INPUT);
    }
    return true;
}

int GpioManager::read(uint8_t gpio) const {
    return valid(gpio) ? digitalRead(gpio) : -1;
}

bool GpioManager::write(uint8_t gpio, uint8_t value) {
    if (!valid(gpio)) return false;
    digitalWrite(gpio, value);
    return true;
}

bool GpioManager::valid(uint8_t gpio) const {
#if defined(ESP8266)
    return gpio <= 16;
#else
    return gpio <= 39;
#endif
}
