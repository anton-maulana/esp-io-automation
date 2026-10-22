#include "ApiServer.h"
#include <LittleFS.h>
#include "CapacityLimits.h"
#if defined(ESP32)
#include <mbedtls/sha1.h>
#include <esp_system.h>
#else
#include <Hash.h>
#endif

static String pcfGroupName(const JsonDocument &doc, const String &pcfId);

void ApiServer::begin(ConfigManager &config, ConfigValidator &validator, RelayManager &relays, PinManager &pins, PcfManager &pcf, GpioManager &gpio, TimeManager &time, AutomationEngine &automation, HttpQueue &http)
{
    _config = &config;
    _validator = &validator;
    _relays = &relays;
    _pins = &pins;
    _pcf = &pcf;
    _gpio = &gpio;
    _time = &time;
    _automation = &automation;
    _http = &http;

    _server.onNotFound([this](AsyncWebServerRequest *request)
                       {
        if (!servePublicAsset(request)) request->send(404, "application/json", "{\"error\":\"not_found\"}"); });
    _server.on("/api/auth/login", HTTP_POST, [this](AsyncWebServerRequest *request, JsonVariant &body)
               {
        if (!authenticate(body.as<JsonVariantConst>())) {
            request->send(401, "application/json", "{\"error\":\"invalid_credentials\"}");
            return;
        }
        JsonDocument doc;
        doc["token"] = _sessionToken;
        doc["expiresInMs"] = _config->doc()["access"]["sessionTimeoutMs"] | 3600000UL;
        String output;
        serializeJson(doc, output);
         request->send(200, "application/json", output); });
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/auth/password", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                       {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String currentPassword = body["currentPassword"] | "";
        String newPassword = body["newPassword"] | "";
        if (currentPassword.length() == 0 || newPassword.length() < 8) { request->send(400, "application/json", "{\"error\":\"invalid_password\"}"); return; }
        String currentHash = _config->doc()["access"]["adminPasswordHash"] | "";
        if (sha1Hex(currentPassword) != currentHash) { request->send(403, "application/json", "{\"error\":\"invalid_current_password\"}"); return; }
        JsonDocument candidate;
        candidate.set(_config->doc());
        candidate["access"]["adminPasswordHash"] = sha1Hex(newPassword);
        String error;
        if (!applyConfig(candidate, error)) { request->send(500, "application/json", (String("{\"error\":\"") + error + "\"}").c_str()); return; }
        request->send(200, "application/json", "{\"updated\":true}");
    }));
    _server.on("/api/config/backup", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        JsonDocument backup;
        backup.set(_config->doc());
        backup["network"]["wifi"]["password"] = "[redacted]";
        backup["network"]["server"]["deviceToken"] = "[redacted]";
        backup["access"]["adminPasswordHash"] = "[redacted]";
        String output;
        serializeJsonPretty(backup, output);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
        response->addHeader("Content-Disposition", "attachment; filename=moduswitch-config-backup.json");
        request->send(response); });
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/restore", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                       {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!applyConfig(body, error)) { request->send(400, "application/json", (String("{\"error\":\"") + error + "\"}").c_str()); return; }
        request->send(200, "application/json", "{\"restored\":true}"); }));
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/patch", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                        {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!applyConfigPatch(body, error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); }));
    auto *pinHandler = new AsyncCallbackJsonWebHandler("/api/gpio", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                       {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        bool create = request->method() == HTTP_POST;
        if (!upsertPin(body, create, false, error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); });
    pinHandler->setMethod(HTTP_POST | HTTP_PUT);
    _server.addHandler(pinHandler);
    auto *extendHandler = new AsyncCallbackJsonWebHandler("/api/extend", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                          {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        bool create = request->method() == HTTP_POST;
        if (!upsertPin(body, create, true, error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); });
    extendHandler->setMethod(HTTP_POST | HTTP_PUT);
    _server.addHandler(extendHandler);
    auto *extendSwitchHandler = new AsyncCallbackJsonWebHandler("/api/extend-switch", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                                {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        bool create = request->method() == HTTP_POST;
        if (!upsertExtendSwitch(body, create, error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); });
    extendSwitchHandler->setMethod(HTTP_POST | HTTP_PUT);
    _server.addHandler(extendSwitchHandler);
    auto *relayHandler = new AsyncCallbackJsonWebHandler("/api/relays", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                        {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        bool create = request->method() == HTTP_POST;
        if (!upsertItem("relays", body, create, "relay-", error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); });
    relayHandler->setMethod(HTTP_POST | HTTP_PUT);
    _server.addHandler(relayHandler);
    auto *actionHandler = new AsyncCallbackJsonWebHandler("/api/actions", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                          {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        bool create = request->method() == HTTP_POST;
        if (!upsertItem("automations", body, create, "automation-", error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); });
    actionHandler->setMethod(HTTP_POST | HTTP_PUT);
    _server.addHandler(actionHandler);
    auto *deviceHandler = new AsyncCallbackJsonWebHandler("/api/device", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                          {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!updateDevice(body, error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); });
    deviceHandler->setMethod(HTTP_PUT);
    _server.addHandler(deviceHandler);
    _server.on("/api/gpio", HTTP_DELETE, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!deleteById("pins", request->arg("id").c_str(), error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        request->send(200, "application/json", "{\"deleted\":true}"); });
    _server.on("/api/extend", HTTP_DELETE, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!deleteById("pins", request->arg("id").c_str(), error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        request->send(200, "application/json", "{\"deleted\":true}"); });
    _server.on("/api/extend-switch", HTTP_DELETE, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!deleteExtendSwitch(request->arg("id").c_str(), error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        request->send(200, "application/json", "{\"deleted\":true}"); });
    _server.on("/api/relays", HTTP_DELETE, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!deleteById("relays", request->arg("id").c_str(), error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        request->send(200, "application/json", "{\"deleted\":true}"); });
    _server.on("/api/actions", HTTP_DELETE, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!deleteById("automations", request->arg("id").c_str(), error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        request->send(200, "application/json", "{\"deleted\":true}"); });
    _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        JsonDocument redacted;
        redacted.set(_config->doc());
        redacted["network"]["wifi"]["password"] = "[redacted]";
        redacted["network"]["server"]["deviceToken"] = "[redacted]";
        redacted["access"]["adminPasswordHash"] = "[redacted]";
        sendJson(request, redacted); });
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/validate", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                       {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        if (!body.is<JsonObjectConst>()) { request->send(400, "application/json", "{\"error\":\"config_must_be_object\"}"); return; }
        JsonDocument candidate;
        candidate.set(body);
        sanitizeCandidate(candidate);
        ValidationResult result = _validator->validate(candidate);
        JsonDocument response;
        response["valid"] = result.valid;
        if (!result.valid) response["error"] = result.error;
        sendJson(request, response, result.valid ? 200 : 400); }));


    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/config", [this](AsyncWebServerRequest *request, JsonVariant &body)
                                                       {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String error;
        if (!applyConfig(body, error)) { JsonDocument response; response["error"] = error; sendJson(request, response, 400); return; }
        JsonDocument response;
        response["saved"] = true;
        response["rebootRequired"] = _lastRebootRequired;
        sendJson(request, response); }));

    _server.on("/api/ota", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        if (Update.hasError()) request->send(500, "application/json", "{\"error\":\"update_failed\"}"); else { request->send(200, "application/json", "{\"updated\":true}"); _restartAtMs = millis() + 5000; } }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
               { handleFirmwareUpload(request, filename, index, data, len, final); });
    _server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
        if (!isAuthenticated(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        request->send(202, "application/json", "{\"rebooting\":true}"); _restartAtMs = millis() + 5000; });
    _server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
        JsonDocument doc;
        _time->fillStatus(doc.to<JsonObject>());
        sendJson(request, doc); });
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
        JsonDocument doc;
        uint32_t freeHeap = ESP.getFreeHeap();
#if defined(ESP8266)
        uint32_t maxFreeBlock = ESP.getMaxFreeBlockSize();
#else
        uint32_t maxFreeBlock = ESP.getMaxAllocHeap();
#endif
        doc["heap"] = freeHeap;
        doc["maxFreeBlock"] = maxFreeBlock;
        doc["minimumHeap"] = _minimumHeap == UINT32_MAX ? freeHeap : _minimumHeap;
        doc["minimumFreeBlock"] = _minimumFreeBlock == UINT32_MAX ? maxFreeBlock : _minimumFreeBlock;
        doc["relays"] = _relays->count();
        doc["pins"] = _pins->count();
        doc["automations"] = _automation->count();
        doc["automationActions"] = _automation->actionCount();
        if (_http) {
            doc["httpQueue"] = _http->size();
            doc["httpQueueRejected"] = _http->rejectedCount();
        }
        doc["board"] = boardName();
        _time->fillStatus(doc["time"].to<JsonObject>());
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output); });
    _server.on("/api/public/dashboard", HTTP_GET, [this](AsyncWebServerRequest *request)
               { sendDashboard(request); });
    _server.on("/api/relays", HTTP_GET, [this](AsyncWebServerRequest *request)
               {
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
                if (String(pin["source"] | "") == "gpio") {
                    item["gpio"] = pin["gpio"] | -1;
                    item["group"] = "GPIO";
                } else {
                    item["pcfId"] = pin["pcfId"] | "";
                    item["pin"] = pin["pin"] | -1;
                    item["group"] = pcfGroupName(_config->doc(), pin["pcfId"] | "");
                }
                break;
            }
        }
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output); });
    _server.on("/api/relay/toggle", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
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
        request->send(204); });
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
                { onWsEvent(server, client, type, arg, data, len); });
    _server.addHandler(&_ws);
    _server.begin();
}

bool ApiServer::isPublicRelay(const String &relayId) const
{
    int index = _relays->findById(relayId);
    return index >= 0 && _relays->relay(index).publicControl;
}

bool ApiServer::anonymousControlEnabled() const
{
    return _config && (_config->doc()["access"]["anonymousControl"] | false);
}

bool ApiServer::isAuthenticated(AsyncWebServerRequest *request) const
{
    if (_sessionToken.length() == 0)
        return false;
    if (static_cast<int32_t>(millis() - _sessionExpiresAt) > 0)
        return false;
    return request->hasHeader("X-Session-Token") && request->header("X-Session-Token") == _sessionToken;
}

bool ApiServer::authenticate(JsonVariantConst body)
{
    const char *username = _config->doc()["access"]["adminUsername"] | "";
    const char *passwordHash = _config->doc()["access"]["adminPasswordHash"] | "";
    if (!*username || !*passwordHash || String(body["username"] | "") != username || sha1Hex(String(body["password"] | "")) != passwordHash)
        return false;
    uint32_t chipId;
    uint32_t randomPart;
#if defined(ESP32)
    chipId = static_cast<uint32_t>(ESP.getEfuseMac());
    randomPart = esp_random();
#else
    chipId = ESP.getChipId();
    randomPart = ESP.random();
#endif
    _sessionToken = String(chipId, HEX) + String(micros(), HEX) + String(randomPart, HEX);
    uint32_t timeout = _config->doc()["access"]["sessionTimeoutMs"] | 3600000UL;
    if (timeout > 3600000UL)
        timeout = 3600000UL;
    _sessionExpiresAt = millis() + timeout;
    return true;
}

void ApiServer::sendDashboard(AsyncWebServerRequest *request) const
{
    JsonDocument doc;
    doc["deviceName"] = _config->doc()["device"]["name"] | "ModuSwitch";
    doc["online"] = true;
    JsonArray relays = doc["relays"].to<JsonArray>();
    for (uint8_t i = 0; i < _relays->count(); i++)
    {
        const RelayConfig &relay = _relays->relay(i);
        if (!relay.publicControl && !isAuthenticated(request))
            continue;
        JsonObject item = relays.add<JsonObject>();
        item["id"] = relay.id;
        item["name"] = relay.name;
        item["room"] = relay.room;
        item["icon"] = relay.icon;
        item["state"] = relay.actualState;
        for (JsonVariantConst pin : _config->doc()["pins"].as<JsonArrayConst>())
        {
            if (String(pin["id"] | "") != relay.pinId)
                continue;
            item["source"] = pin["source"] | "";
            if (String(pin["source"] | "") == "gpio")
            {
                item["gpio"] = pin["gpio"] | -1;
                item["group"] = "GPIO";
            }
            else
            {
                item["pcfId"] = pin["pcfId"] | "";
                item["pin"] = pin["pin"] | -1;
                item["group"] = pcfGroupName(_config->doc(), pin["pcfId"] | "");
            }
            break;
        }
    }
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

void ApiServer::sendRelayState(AsyncWebSocketClient *client, uint8_t relayIndex, const String &requestId) const
{
    const RelayConfig &relay = _relays->relay(relayIndex);
    JsonDocument doc;
    doc["type"] = "relay.state";
    if (requestId.length())
        doc["requestId"] = requestId;
    doc["relayId"] = relay.id;
    doc["desiredState"] = relay.desiredState;
    doc["actualState"] = relay.actualState;
    doc["timestamp"] = millis();
    String output;
    serializeJson(doc, output);
    client->text(output);
}

void ApiServer::publishRelayState(uint8_t relayIndex, const String &requestId)
{
    const RelayConfig &relay = _relays->relay(relayIndex);
    Serial.printf("[RELAY] %s -> %s (actual=%s)\n", relay.id.c_str(), relay.desiredState ? "ON" : "OFF", relay.actualState ? "ON" : "OFF");
    JsonDocument doc;
    doc["type"] = "relay.state";
    if (requestId.length())
        doc["requestId"] = requestId;
    doc["relayId"] = relay.id;
    doc["desiredState"] = relay.desiredState;
    doc["actualState"] = relay.actualState;
    doc["timestamp"] = millis();
    String output;
    serializeJson(doc, output);
    _ws.textAll(output);
}

void ApiServer::onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type != WS_EVT_DATA)
        return;
    if (len == 0 || len > 384)
        return;
    JsonDocument doc;
    if (deserializeJson(doc, data, len))
        return;
    if (String(doc["type"] | "") != "relay.command")
        return;
    String relayId = doc["relayId"] | "";
    String command = doc["command"] | "";
    String requestId = doc["requestId"] | "";
    int index = _relays->findById(relayId);
    if (index < 0 || !anonymousControlEnabled() || !isPublicRelay(relayId))
    {
        Serial.printf("[WS] Rejected relay command: %s\n", relayId.c_str());
        return;
    }
    Serial.printf("[WS] Relay command: %s (%s)\n", relayId.c_str(), command.c_str());
    bool ok = command == "toggle" ? _relays->toggle(index, *_pins, *_pcf, *_gpio) : false;
    if (ok)
    {
        publishRelayState(index, requestId);
    }
    else
    {
        client->text("{\"type\":\"command.error\",\"code\":\"relay_write_failed\"}");
    }
}

void ApiServer::sendJson(AsyncWebServerRequest *request, JsonDocument &doc, int code) const
{
    String output;
    serializeJson(doc, output);
    request->send(code, "application/json", output);
}

static bool sameJson(JsonVariantConst a, JsonVariantConst b)
{
    return a == b;
}

static String pcfGroupName(const JsonDocument &doc, const String &pcfId)
{
    for (JsonVariantConst pcf : doc["hardware"]["pcfs"].as<JsonArrayConst>()) {
        if (String(pcf["id"] | "") != pcfId) continue;
        String name = pcf["name"] | "";
        return name.length() ? name : pcfId;
    }
    return pcfId;
}

static uint32_t relayIdHash(const String &id)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < id.length(); i++)
    {
        hash ^= static_cast<uint8_t>(id[i]);
        hash *= 16777619u;
    }
    return hash;
}

bool ApiServer::applyConfig(JsonVariantConst body, String &error)
{
    if (!body.is<JsonObjectConst>())
    {
        error = "config_must_be_object";
        return false;
    }
    JsonDocument candidate;
    candidate.set(body);
    return applyConfigDocument(candidate, error);
}

bool ApiServer::applyConfigDocument(JsonDocument &candidate, String &error)
{
    sanitizeCandidate(candidate);
    if (candidate.overflowed())
    {
        error = "config_out_of_memory";
        return false;
    }
    ValidationResult result = _validator->validate(candidate);
    if (!result.valid)
    {
        error = result.error;
        return false;
    }
    const JsonDocument &previous = _config->doc();
    bool i2cChanged = !sameJson(previous["hardware"]["i2c"], candidate["hardware"]["i2c"]);
    bool networkChanged = !sameJson(previous["network"], candidate["network"]);
    bool timeChanged = !sameJson(previous["time"], candidate["time"]) ||
                       !sameJson(previous["device"]["timezone"], candidate["device"]["timezone"]);
    bool hardwareChanged = !sameJson(previous["pins"], candidate["pins"]) ||
                           !sameJson(previous["relays"], candidate["relays"]) ||
                           !sameJson(previous["automations"], candidate["automations"]) ||
                           !sameJson(previous["hardware"]["pcfs"], candidate["hardware"]["pcfs"]);
    if (!_config->save(candidate))
    {
        error = String("config_save_failed:") + _config->lastSaveError();
        return false;
    }
    _config->doc() = std::move(candidate);
    _lastRebootRequired = i2cChanged || networkChanged || timeChanged;
    if (hardwareChanged && !i2cChanged) _reloadPending = true;
    return true;
}

static void mergeJsonObject(JsonObject target, JsonObjectConst changes)
{
    for (JsonPairConst item : changes) {
        JsonObjectConst nested = item.value().as<JsonObjectConst>();
        if (!nested.isNull()) mergeJsonObject(target[item.key()].to<JsonObject>(), nested);
        else target[item.key()] = item.value();
    }
}

bool ApiServer::applyConfigPatch(JsonVariantConst body, String& error)
{
    if (!body.is<JsonObjectConst>()) { error = "patch_must_be_object"; return false; }
    JsonObjectConst changes = body["changes"].as<JsonObjectConst>();
    if (changes.isNull()) { error = "patch_changes_required"; return false; }
    JsonDocument candidate;
    candidate.set(_config->doc());
    for (JsonPairConst section : changes) {
        String name = section.key().c_str();
        if (name != "device" && name != "network" && name != "time" && name != "access") {
            error = "unsupported_patch_section";
            return false;
        }
        JsonObjectConst values = section.value().as<JsonObjectConst>();
        if (values.isNull()) { error = "patch_section_must_be_object"; return false; }
        mergeJsonObject(candidate[name].to<JsonObject>(), values);
    }
    return applyConfigDocument(candidate, error);
}

static int findIndexById(JsonArray array, const String& id)
{
    for (size_t i = 0; i < array.size(); i++)
        if (String(array[i]["id"] | "") == id) return static_cast<int>(i);
    return -1;
}

bool ApiServer::upsertPin(JsonVariantConst body, bool create, bool extend, String& error)
{
    if (!body.is<JsonObjectConst>()) { error = "pin_must_be_object"; return false; }
    String id = body["id"] | "";
    if (!id.length() && create) {
        id = extend ? String("pin-") + String(body["pcfId"] | "") + "-" + String(body["pin"] | 0)
                    : String("pin-gpio-") + String(body["gpio"] | 0);
    }
    if (!id.length()) { error = "pin_id_required"; return false; }
    JsonDocument pinBody;
    pinBody.set(body);
    pinBody.remove("relay");
    pinBody.remove("deleteRelay");
    JsonDocument candidate;
    candidate.set(_config->doc());
    JsonArray pins = candidate["pins"].as<JsonArray>();
    int index = findIndexById(pins, id);
    if (create && index >= 0) { error = "pin_id_exists"; return false; }
    if (!create && index < 0) { error = "pin_not_found"; return false; }
    JsonObject item = index >= 0 ? pins[index].to<JsonObject>() : pins.add<JsonObject>();
    item.clear();
    item.set(pinBody.as<JsonObjectConst>());
    item["id"] = id;
    item["source"] = extend ? "pcf" : "gpio";
    if (extend) item.remove("gpio");
    else { item.remove("pcfId"); item.remove("pin"); }
    JsonArray relays = candidate["relays"].as<JsonArray>();
    if (body["deleteRelay"] | false) {
        for (int i = static_cast<int>(relays.size()) - 1; i >= 0; i--)
            if (String(relays[i]["pinId"] | "") == id) relays.remove(i);
    } else {
        JsonObjectConst relay = body["relay"].as<JsonObjectConst>();
        if (!relay.isNull()) {
            String relayId = relay["id"] | "";
            if (!relayId.length()) relayId = String("relay-") + id;
            int relayIndex = findIndexById(relays, relayId);
            JsonObject target = relayIndex >= 0 ? relays[relayIndex].to<JsonObject>() : relays.add<JsonObject>();
            target.clear();
            target.set(relay);
            target["id"] = relayId;
            target["pinId"] = id;
        }
    }
    return applyConfigDocument(candidate, error);
}

bool ApiServer::upsertItem(const char* key, JsonVariantConst body, bool create, const char* idPrefix, String& error)
{
    if (!body.is<JsonObjectConst>()) { error = "item_must_be_object"; return false; }
    String id = body["id"] | "";
    if (!id.length() && create) id = String(idPrefix) + String(millis(), HEX);
    if (!id.length()) { error = "id_required"; return false; }
    JsonDocument candidate;
    candidate.set(_config->doc());
    JsonArray items = candidate[key].as<JsonArray>();
    int index = findIndexById(items, id);
    if (create && index >= 0) { error = "id_exists"; return false; }
    if (!create && index < 0) { error = "item_not_found"; return false; }
    JsonObject item = index >= 0 ? items[index].to<JsonObject>() : items.add<JsonObject>();
    item.clear();
    item.set(body);
    item["id"] = id;
    return applyConfigDocument(candidate, error);
}

bool ApiServer::deleteById(const char* key, const char* id, String& error)
{
    if (!id || !*id) { error = "id_required"; return false; }
    JsonDocument candidate;
    candidate.set(_config->doc());
    JsonArray items = candidate[key].as<JsonArray>();
    int index = findIndexById(items, id);
    if (index < 0) { error = "item_not_found"; return false; }
    if (String(key) == "pins") {
        JsonArray relays = candidate["relays"].as<JsonArray>();
        for (int i = static_cast<int>(relays.size()) - 1; i >= 0; i--)
            if (String(relays[i]["pinId"] | "") == id) relays.remove(i);
    }
    items.remove(index);
    return applyConfigDocument(candidate, error);
}

bool ApiServer::updateDevice(JsonVariantConst body, String& error)
{
    if (!body.is<JsonObjectConst>()) { error = "device_must_be_object"; return false; }
    JsonDocument candidate;
    candidate.set(_config->doc());
    for (JsonPairConst section : body.as<JsonObjectConst>()) {
        String name = section.key().c_str();
        if (name != "device" && name != "network" && name != "time" && name != "access") {
            error = "unsupported_device_section";
            return false;
        }
        JsonObjectConst values = section.value().as<JsonObjectConst>();
        if (values.isNull()) { error = "device_section_must_be_object"; return false; }
        mergeJsonObject(candidate[name].to<JsonObject>(), values);
    }
    return applyConfigDocument(candidate, error);
}

bool ApiServer::upsertExtendSwitch(JsonVariantConst body, bool create, String& error)
{
    if (!body.is<JsonObjectConst>()) { error = "extend_must_be_object"; return false; }
    JsonObjectConst pcfBody = body["pcf"].as<JsonObjectConst>();
    JsonArrayConst pinList = body["pins"].as<JsonArrayConst>();
    JsonArrayConst relayList = body["relays"].as<JsonArrayConst>();
    if (pcfBody.isNull()) { error = "pcf_required"; return false; }
    String pcfId = pcfBody["id"] | "";
    if (!pcfId.length() && create) pcfId = String("pcf-") + String(millis(), HEX);
    if (!pcfId.length()) { error = "pcf_id_required"; return false; }
    JsonDocument candidate;
    candidate.set(_config->doc());
    JsonArray pcfs = candidate["hardware"]["pcfs"].as<JsonArray>();
    int pcfIndex = findIndexById(pcfs, pcfId);
    if (create && pcfIndex >= 0) { error = "pcf_id_exists"; return false; }
    if (!create && pcfIndex < 0) { error = "pcf_not_found"; return false; }
    JsonArray pins = candidate["pins"].as<JsonArray>();
    JsonArray relays = candidate["relays"].as<JsonArray>();
    for (int i = static_cast<int>(pins.size()) - 1; i >= 0; i--) {
        if (String(pins[i]["pcfId"] | "") != pcfId) continue;
        String pinId = pins[i]["id"] | "";
        pins.remove(i);
        for (int j = static_cast<int>(relays.size()) - 1; j >= 0; j--)
            if (String(relays[j]["pinId"] | "") == pinId) relays.remove(j);
    }
    for (JsonVariantConst pin : pinList) {
        JsonObject target = pins.add<JsonObject>();
        target.set(pin);
        target["source"] = "pcf";
        target["pcfId"] = pcfId;
        target.remove("gpio");
    }
    for (JsonVariantConst relay : relayList) {
        JsonObject target = relays.add<JsonObject>();
        target.set(relay);
    }
    JsonObject pcf = pcfIndex >= 0 ? pcfs[pcfIndex].to<JsonObject>() : pcfs.add<JsonObject>();
    pcf.clear();
    pcf.set(pcfBody);
    pcf["id"] = pcfId;
    return applyConfigDocument(candidate, error);
}

bool ApiServer::deleteExtendSwitch(const char* id, String& error)
{
    if (!id || !*id) { error = "id_required"; return false; }
    JsonDocument candidate;
    candidate.set(_config->doc());
    JsonArray pcfs = candidate["hardware"]["pcfs"].as<JsonArray>();
    int index = findIndexById(pcfs, id);
    if (index < 0) { error = "pcf_not_found"; return false; }
    JsonArray pins = candidate["pins"].as<JsonArray>();
    JsonArray relays = candidate["relays"].as<JsonArray>();
    for (int i = static_cast<int>(pins.size()) - 1; i >= 0; i--) {
        if (String(pins[i]["pcfId"] | "") != id) continue;
        String pinId = pins[i]["id"] | "";
        pins.remove(i);
        for (int j = static_cast<int>(relays.size()) - 1; j >= 0; j--)
            if (String(relays[j]["pinId"] | "") == pinId) relays.remove(j);
    }
    pcfs.remove(index);
    return applyConfigDocument(candidate, error);
}

void ApiServer::reloadRuntime()
{
    JsonDocument &doc = _config->doc();
    uint32_t oldHashes[64];
    bool oldStates[64];
    uint8_t snapshot = _relays->count() > 64 ? 64 : _relays->count();
    for (uint8_t i = 0; i < snapshot; i++)
    {
        oldHashes[i] = relayIdHash(_relays->relay(i).id);
        oldStates[i] = _relays->relay(i).desiredState;
    }
    _pcf->configure(doc["hardware"]["pcfs"].as<JsonArrayConst>());
    _pcf->scan();
    _pins->configure(doc["pins"].as<JsonArrayConst>(), *_pcf, *_gpio);
    _relays->configure(doc["relays"].as<JsonArrayConst>(), *_pins);
    for (uint8_t i = 0; i < _relays->count(); i++)
    {
        const RelayConfig &relay = _relays->relay(i);
        bool restored = false;
        uint32_t hash = relayIdHash(relay.id);
        for (uint8_t j = 0; j < snapshot; j++)
        {
            if (oldHashes[j] != hash) continue;
            _relays->setState(i, relay.startupPolicy == "on", *_pins, *_pcf, *_gpio);
            restored = true;
            break;
        }
        if (!restored) _relays->setState(i, relay.startupPolicy == "on", *_pins, *_pcf, *_gpio);
    }
    if (_automation) _automation->configure(doc["automations"].as<JsonArrayConst>(), *_pins, *_relays, *_pcf, *_gpio);
}

void ApiServer::restoreRedacted(JsonDocument &candidate) const
{
    if (candidate["network"]["wifi"]["password"] == "[redacted]")
        candidate["network"]["wifi"]["password"] = _config->doc()["network"]["wifi"]["password"] | "";
    if (candidate["network"]["server"]["deviceToken"] == "[redacted]")
        candidate["network"]["server"]["deviceToken"] = _config->doc()["network"]["server"]["deviceToken"] | "";
    if (candidate["access"]["adminPasswordHash"] == "[redacted]")
        candidate["access"]["adminPasswordHash"] = _config->doc()["access"]["adminPasswordHash"] | "";
}

void ApiServer::sanitizeCandidate(JsonDocument &candidate) const
{
    restoreRedacted(candidate);
    JsonArray pins = candidate["pins"].as<JsonArray>();
    JsonArray relays = candidate["relays"].as<JsonArray>();
    JsonArray automations = candidate["automations"].as<JsonArray>();
    for (int i = static_cast<int>(automations.size()) - 1; i >= 0; i--)
    {
        JsonObject automation = automations[i].as<JsonObject>();
        bool valid = true;
        String triggerType = automation["trigger"]["type"] | "";
        if (triggerType == "pin_change")
        {
            String pinId = automation["trigger"]["pinId"] | "";
            valid = false;
            for (JsonVariantConst pin : pins)
                if (String(pin["id"] | "") == pinId)
                {
                    valid = true;
                    break;
                }
        }
        JsonArray actions = automation["actions"].as<JsonArray>();
        for (JsonVariantConst action : actions)
        {
            String relayId = action["relayId"] | "";
            bool relayFound = false;
            for (JsonVariantConst relay : relays)
                if (String(relay["id"] | "") == relayId)
                {
                    relayFound = true;
                    break;
                }
            if (!relayFound)
            {
                valid = false;
                break;
            }
        }
        if (!valid || actions.isNull() || actions.size() == 0)
            automations.remove(i);
    }
}

bool ApiServer::servePublicAsset(AsyncWebServerRequest *request) const
{
    String uri = request->url();
    if (uri.startsWith("/api/"))
        return false;
    if (uri == "/" || uri == "/index.html")
    {
        request->send(LittleFS, "/index.html", "text/html");
        return true;
    }
    if (uri == "/app.js")
    {
        request->send(LittleFS, "/app.js", "application/javascript");
        return true;
    }
    if (uri == "/style.css")
    {
        request->send(LittleFS, "/style.css", "text/css");
        return true;
    }
    if (uri == "/gpio-pins.json")
    {
        request->send(LittleFS, "/gpio-pins.json", "application/json");
        return true;
    }
    return false;
}

const char *ApiServer::boardName() const
{
#if defined(ESP32)
    return "esp32_devkit";
#else
    return "esp8266_d1_mini";
#endif
}

String ApiServer::sha1Hex(const String &input) const
{
#if defined(ESP32)
    unsigned char hash[20];
    mbedtls_sha1_ret(reinterpret_cast<const unsigned char *>(input.c_str()), input.length(), hash);
    String output;
    output.reserve(40);
    for (int i = 0; i < 20; i++)
    {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", hash[i]);
        output += hex;
    }
    return output;
#else
    return sha1(input);
#endif
}

void ApiServer::handleFirmwareUpload(AsyncWebServerRequest *request, String, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!isAuthenticated(request))
        return;
    if (index == 0)
    {
        if (!Update.begin(ESP.getFreeSketchSpace()))
        {
            Update.printError(Serial);
            return;
        }
    }
    if (len && Update.write(data, len) != len)
        Update.printError(Serial);
    if (final && !Update.end(true))
        Update.printError(Serial);
}

void ApiServer::tick()
{
    uint32_t freeHeap = ESP.getFreeHeap();
#if defined(ESP8266)
    uint32_t maxFreeBlock = ESP.getMaxFreeBlockSize();
#else
    uint32_t maxFreeBlock = ESP.getMaxAllocHeap();
#endif
    if (freeHeap < _minimumHeap) _minimumHeap = freeHeap;
    if (maxFreeBlock < _minimumFreeBlock) _minimumFreeBlock = maxFreeBlock;
    _ws.cleanupClients();
    if (_reloadPending)
    {
        _reloadPending = false;
        reloadRuntime();
    }
    if (_restartAtMs && millis() >= _restartAtMs)
        ESP.restart();
}
