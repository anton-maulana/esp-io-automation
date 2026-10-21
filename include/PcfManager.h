#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>

struct PcfChip {
    String id;
    uint8_t address;
    bool enabled;
    bool detected;
    uint8_t lastRead;
    uint8_t currentWrite;
};

class PcfManager {
public:
    bool begin(uint8_t sdaGpio, uint8_t sclGpio, uint32_t clockHz);
    void configure(JsonArrayConst pcfs);
    bool scan();
    uint8_t readPort(uint8_t index);
    bool writePort(uint8_t index, uint8_t value);
    bool writeBit(uint8_t index, uint8_t bit, bool value);
    uint8_t count() const;
    const PcfChip& chip(uint8_t index) const;
    int findById(const String& id) const;

private:
    PcfChip _chips[8];
    uint8_t _count = 0;
};
