#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "CapacityLimits.h"

class HttpQueue {
public:
    bool enqueue(const String& method, const String& path, const String& body);
    void tick();
    uint8_t size() const;
    uint32_t rejectedCount() const;
private:
    struct Item { String method; String path; String body; uint8_t retries; };
    Item _items[CapacityLimits::MaxHttpQueue];
    uint8_t _head = 0;
    uint8_t _tail = 0;
    uint8_t _size = 0;
    uint32_t _rejectedCount = 0;
};
