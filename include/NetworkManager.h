#pragma once
#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
class NetworkManager {
public:
    void begin(const char* ssid, const char* password, const char* hostname, uint32_t reconnectIntervalMs);
    void tick();
    bool connected() const;
    const String& ssid() const;
    String ipAddress() const;

private:
    String _ssid;
    String _password;
    bool _wasConnected = false;
    uint32_t _lastLogMs = 0;
    uint32_t _lastRetryMs = 0;
    uint32_t _retryIntervalMs = 5000;
    uint8_t _attempt = 0;
#if defined(ESP8266)
    WiFiEventHandler _disconnectHandler;
    int _disconnectReason = 0;
    bool _scanStarted = false;
    bool _scanFinished = false;
#endif
};
