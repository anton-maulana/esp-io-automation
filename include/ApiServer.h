#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include <AsyncTCP.h>
#else
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#if defined(ESP8266)
#include <Updater.h>
#else
#include <Update.h>
#endif
#include "ConfigManager.h"
#include "ConfigValidator.h"
#include "RelayManager.h"
#include "PcfManager.h"
#include "PinManager.h"
#include "GpioManager.h"
#include "TimeManager.h"
#include "AutomationEngine.h"

class ApiServer {
public:
    void begin(ConfigManager& config, ConfigValidator& validator, RelayManager& relays, PinManager& pins, PcfManager& pcf, GpioManager& gpio, TimeManager& time, AutomationEngine& automation);
    void publishRelayState(uint8_t relayIndex, const String& requestId = "");
    void tick();

private:
    AsyncWebServer _server{80};
    AsyncWebSocket _ws{"/ws"};
    ConfigManager* _config = nullptr;
    ConfigValidator* _validator = nullptr;
    RelayManager* _relays = nullptr;
    PinManager* _pins = nullptr;
    PcfManager* _pcf = nullptr;
    GpioManager* _gpio = nullptr;
    TimeManager* _time = nullptr;
    AutomationEngine* _automation = nullptr;
    String _sessionToken;
    uint32_t _sessionExpiresAt = 0;
    uint32_t _restartAtMs = 0;
    bool _reloadPending = false;
    bool _lastRebootRequired = false;

    bool isPublicRelay(const String& relayId) const;
    bool anonymousControlEnabled() const;
    bool isAuthenticated(AsyncWebServerRequest* request) const;
    bool authenticate(JsonVariantConst body);
    void sendDashboard(AsyncWebServerRequest* request) const;
    void sendRelayState(AsyncWebSocketClient* client, uint8_t relayIndex, const String& requestId) const;
    void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient*, AwsEventType, void*, uint8_t*, size_t);
    void sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int code = 200) const;
    bool applyConfig(JsonVariantConst body, String& error);
    void handleFirmwareUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final);
    void restoreRedacted(JsonDocument& candidate) const;
    void sanitizeCandidate(JsonDocument& candidate) const;
    bool servePublicAsset(AsyncWebServerRequest* request) const;
    const char* boardName() const;
    String sha1Hex(const String& input) const;
    void reloadRuntime();
};
