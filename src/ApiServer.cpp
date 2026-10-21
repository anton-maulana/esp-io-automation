#include "ApiServer.h"
#include <LittleFS.h>
#include <Hash.h>

void ApiServer::begin(ConfigManager& config, ConfigValidator& validator, RelayManager& relays, PinManager& pins, PcfManager& pcf, GpioManager& gpio, TimeManager& time) {
    _config = &config;
    _validator = &validator;
    _relays = &relays;
    _pins = &pins;
    _pcf = &pcf;
    _gpio = &gpio;
    _time = &time;

    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    _server.on("/api/auth/login", HTTP_POST, [this](AsyncWebServerRequest* request, JsonVariant& body) {
        if (!authenticate(body.as<JsonVariantConst>())) {
            request->send(401, "application/json", "{\"error\":\"invalid_credentials\"}");
            return;
        }
        JsonDocument doc;
        doc["token"] = _sessionToken;
        doc["expiresInMs"] = _config->doc()["access"]["sessionTimeoutMs"] | 3600000UL;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        JsonDocument redacted;
        redacted.set(_config->doc());
        redacted["network"]["wifi"]["password"] = "[redacted]";
        redacted["network"]["server"]["deviceToken"] = "[redacted]";
        redacted["access"]["adminPasswordHash"] = "[redacted]";
        sendJson(request, redacted);
    });
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/config", [this](AsyncWebServerRequest* request, JsonVariant& body) {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!applyConfig(body, error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        request->send(200, "application/json", "{\"saved\":true,\"rebootRequired\":true}");
    }));
    _server.on("/api/config/validate", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        ValidationResult result = _validator->validate(_config->doc());
        JsonDocument response; response["valid"] = result.valid; if (!result.valid) response["error"] = result.error; sendJson(request, response, result.valid ? 200 : 400);
    });
    _server.on("/api/ota", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        if (Update.hasError()) request->send(500, "application/json", "{\"error\":\"update_failed\"}"); else { request->send(200, "application/json", "{\"updated\":true}"); _restartAtMs = millis() + 5000; }
    }, [this](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) { handleFirmwareUpload(request, filename, index, data, len, final); });
    _server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        request->send(202, "application/json", "{\"rebooting\":true}"); _restartAtMs = millis() + 5000;
    });
    _server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        _time->fillStatus(doc.to<JsonObject>());
        sendJson(request, doc);
    });
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["heap"] = ESP.getFreeHeap();
        doc["relays"] = _relays->count();
        _time->fillStatus(doc["time"].to<JsonObject>());
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    _server.on("/api/public/dashboard", HTTP_GET, [this](AsyncWebServerRequest* request) { sendDashboard(request); });
    _server.on("/api/relays", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        JsonArray items = doc.to<JsonArray>();
        for (uint8_t i = 0; i < _relays->count(); i++) {
            const RelayConfig& relay = _relays->relay(i);
            if (!relay.publicControl && !isAuthenticated(request)) continue;
            JsonObject item = items.add<JsonObject>();
            item["id"] = relay.id;
            item["name"] = relay.name;
            item["room"] = relay.room;
            item["icon"] = relay.icon;
            item["state"] = relay.actualState;
            for (JsonVariantConst pin : _config->doc()["pins"].as<JsonArrayConst>()) {
                if (String(pin["id"] | "") != relay.pinId) continue;
                item["source"] = pin["source"] | "";
                if (String(pin["source"] | "") == "gpio") item["gpio"] = pin["gpio"] | -1;
                else { item["pcfId"] = pin["pcfId"] | ""; item["pin"] = pin["pin"] | -1; }
                break;
            }
        }
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });
    _server.on("/api/relay/toggle", HTTP_POST, [this](AsyncWebServerRequest* request) {
        String id = request->arg("id");
        int index = _relays->findById(id);
        if (index < 0 || (!isAuthenticated(request) && (!anonymousControlEnabled() || !isPublicRelay(id)))) {
            request->send(403, "application/json", "{\"error\":\"forbidden\"}");
            return;
        }
        if (!_relays->toggle(index, *_pins, *_pcf, *_gpio)) {
            request->send(500, "application/json", "{\"error\":\"relay_write_failed\"}");
            return;
        }
        publishRelayState(index);
        request->send(204);
    });
    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
        onWsEvent(server, client, type, arg, data, len);
    });
    _server.addHandler(&_ws);
    _server.begin();
}

bool ApiServer::isPublicRelay(const String& relayId) const {
    int index = _relays->findById(relayId);
    return index >= 0 && _relays->relay(index).publicControl;
}

bool ApiServer::anonymousControlEnabled() const {
    return _config && (_config->doc()["access"]["anonymousControl"] | false);
}

bool ApiServer::isAuthenticated(AsyncWebServerRequest* request) const {
    if (_sessionToken.length() == 0 || millis() > _sessionExpiresAt) return false;
    return request->hasHeader("X-Session-Token") && request->header("X-Session-Token") == _sessionToken;
}

bool ApiServer::authenticate(JsonVariantConst body) {
    const char* username = _config->doc()["access"]["adminUsername"] | "";
    const char* passwordHash = _config->doc()["access"]["adminPasswordHash"] | "";
    if (!*username || !*passwordHash || String(body["username"] | "") != username || sha1(String(body["password"] | "")) != passwordHash) return false;
    _sessionToken = String(ESP.getChipId(), HEX) + String(micros(), HEX) + String(ESP.random(), HEX);
    uint32_t timeout = _config->doc()["access"]["sessionTimeoutMs"] | 3600000UL;
    if (timeout > 3600000UL) timeout = 3600000UL;
    _sessionExpiresAt = millis() + timeout;
    return true;
}

void ApiServer::sendDashboard(AsyncWebServerRequest* request) const {
    JsonDocument doc;
    doc["deviceName"] = _config->doc()["device"]["name"] | "ModuSwitch";
    doc["online"] = true;
    JsonArray relays = doc["relays"].to<JsonArray>();
    for (uint8_t i = 0; i < _relays->count(); i++) {
        const RelayConfig& relay = _relays->relay(i);
        if (!relay.publicControl && !isAuthenticated(request)) continue;
        JsonObject item = relays.add<JsonObject>();
        item["id"] = relay.id;
        item["name"] = relay.name;
        item["room"] = relay.room;
        item["icon"] = relay.icon;
        item["state"] = relay.actualState;
        for (JsonVariantConst pin : _config->doc()["pins"].as<JsonArrayConst>()) {
            if (String(pin["id"] | "") != relay.pinId) continue;
            item["source"] = pin["source"] | "";
            if (String(pin["source"] | "") == "gpio") item["gpio"] = pin["gpio"] | -1;
            else { item["pcfId"] = pin["pcfId"] | ""; item["pin"] = pin["pin"] | -1; }
            break;
        }
    }
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void ApiServer::sendRelayState(AsyncWebSocketClient* client, uint8_t relayIndex, const String& requestId) const {
    const RelayConfig& relay = _relays->relay(relayIndex);
    JsonDocument doc;
    doc["type"] = "relay.state";
    if (requestId.length()) doc["requestId"] = requestId;
    doc["relayId"] = relay.id;
    doc["desiredState"] = relay.desiredState;
    doc["actualState"] = relay.actualState;
    doc["timestamp"] = millis();
    String output;
    serializeJson(doc, output);
    client->text(output);
}

void ApiServer::publishRelayState(uint8_t relayIndex, const String& requestId) {
    const RelayConfig& relay = _relays->relay(relayIndex);
    Serial.printf("[RELAY] %s -> %s (actual=%s)\n", relay.id.c_str(), relay.desiredState ? "ON" : "OFF", relay.actualState ? "ON" : "OFF");
    JsonDocument doc;
    doc["type"] = "relay.state";
    if (requestId.length()) doc["requestId"] = requestId;
    doc["relayId"] = relay.id;
    doc["desiredState"] = relay.desiredState;
    doc["actualState"] = relay.actualState;
    doc["timestamp"] = millis();
    String output;
    serializeJson(doc, output);
    _ws.textAll(output);
}

void ApiServer::onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type != WS_EVT_DATA) return;
    if (len == 0 || len > 384) return;
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;
    if (String(doc["type"] | "") != "relay.command") return;
    String relayId = doc["relayId"] | "";
    String command = doc["command"] | "";
    String requestId = doc["requestId"] | "";
    int index = _relays->findById(relayId);
    if (index < 0 || !anonymousControlEnabled() || !isPublicRelay(relayId)) {
        Serial.printf("[WS] Rejected relay command: %s\n", relayId.c_str());
        return;
    }
    Serial.printf("[WS] Relay command: %s (%s)\n", relayId.c_str(), command.c_str());
    bool ok = command == "toggle" ? _relays->toggle(index, *_pins, *_pcf, *_gpio) : false;
    if (ok) {
        publishRelayState(index, requestId);
    } else {
        client->text("{\"type\":\"command.error\",\"code\":\"relay_write_failed\"}");
    }
}

void ApiServer::sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int code) const {
    String output;
    serializeJson(doc, output);
    request->send(code, "application/json", output);
}

bool ApiServer::applyConfig(JsonVariantConst body, String& error) {
    if (!body.is<JsonObjectConst>()) { error = "config_must_be_object"; return false; }
    JsonDocument candidate;
    candidate.set(body);
    if (candidate["network"]["wifi"]["password"] == "[redacted]") candidate["network"]["wifi"]["password"] = _config->doc()["network"]["wifi"]["password"] | "";
    if (candidate["network"]["server"]["deviceToken"] == "[redacted]") candidate["network"]["server"]["deviceToken"] = _config->doc()["network"]["server"]["deviceToken"] | "";
    if (candidate["access"]["adminPasswordHash"] == "[redacted]") candidate["access"]["adminPasswordHash"] = _config->doc()["access"]["adminPasswordHash"] | "";
    JsonArray pins = candidate["pins"].as<JsonArray>();
    JsonArray relays = candidate["relays"].as<JsonArray>();
    JsonArray automations = candidate["automations"].as<JsonArray>();
    for (int i = static_cast<int>(automations.size()) - 1; i >= 0; i--) {
        JsonObject automation = automations[i].as<JsonObject>();
        bool valid = true;
        String triggerType = automation["trigger"]["type"] | "";
        if (triggerType == "pin_change") {
            String pinId = automation["trigger"]["pinId"] | "";
            valid = false;
            for (JsonVariantConst pin : pins) if (String(pin["id"] | "") == pinId) { valid = true; break; }
        }
        JsonArray actions = automation["actions"].as<JsonArray>();
        for (JsonVariantConst action : actions) {
            String relayId = action["relayId"] | "";
            bool relayFound = false;
            for (JsonVariantConst relay : relays) if (String(relay["id"] | "") == relayId) { relayFound = true; break; }
            if (!relayFound) { valid = false; break; }
        }
        if (!valid || actions.isNull() || actions.size() == 0) automations.remove(i);
    }
    ValidationResult result = _validator->validate(candidate);
    if (!result.valid) { error = result.error; return false; }
    JsonDocument previous;
    previous.set(_config->doc());
    _config->doc().set(candidate);
    if (!_config->save()) { _config->doc().set(previous); error = "config_save_failed"; return false; }
    return true;
}

void ApiServer::handleFirmwareUpload(AsyncWebServerRequest* request, String, size_t index, uint8_t* data, size_t len, bool final) {
    if (!isAuthenticated(request)) return;
    if (index == 0) {
        if (!Update.begin(ESP.getFreeSketchSpace())) { Update.printError(Serial); return; }
    }
    if (len && Update.write(data, len) != len) Update.printError(Serial);
    if (final && !Update.end(true)) Update.printError(Serial);
}

void ApiServer::tick() { _ws.cleanupClients(); if (_restartAtMs && millis() >= _restartAtMs) ESP.restart(); }
