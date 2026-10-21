#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class HttpQueue {
public:
    bool enqueue(const String& method, const String& path, const String& body);
    void tick();
    uint8_t size() const;
private:
    struct Item { String method; String path; String body; uint8_t retries; };
    Item _items[8]; uint8_t _head=0, _tail=0, _size=0;
};
