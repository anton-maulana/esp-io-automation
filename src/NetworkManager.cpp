#include "NetworkManager.h"
#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#include <user_interface.h>
#endif

static const uint32_t STATUS_INTERVAL_MS = 5000;
static const uint32_t CONNECTION_TIMEOUT_MS = 12000;

#if defined(ESP8266)
static const char* disconnectReasonName(int reason) {
    switch (reason) {
        case 0: return "waiting for connection";
        case WIFI_DISCONNECT_REASON_NO_AP_FOUND: return "AP not found (check 2.4 GHz/SSID)";
        case WIFI_DISCONNECT_REASON_AUTH_FAIL: return "authentication failed (check password)";
        case WIFI_DISCONNECT_REASON_AUTH_EXPIRE: return "authentication expired (check AP/security)";
        case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT: return "handshake timeout (check password/security)";
        case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT: return "beacon timeout (check signal)";
        case WIFI_DISCONNECT_REASON_ASSOC_FAIL: return "association failed";
        default: return "see reason code";
    }
}
#endif

void NetworkManager::begin(const char* ssid, const char* password, const char* hostname, uint32_t reconnectIntervalMs) {
    _ssid = ssid;
    _password = password;
    _retryIntervalMs = reconnectIntervalMs < 3000 ? 3000 : reconnectIntervalMs;
    Serial.println("[BOOT] Step: starting Wi-Fi");
    if (_ssid.length() == 0) {
        Serial.println("[WIFI] ERROR: SSID is empty. Check /config.json in LittleFS.");
        return;
    }
    Serial.printf("[WIFI] Target SSID: %s\n", _ssid.c_str());
#if defined(ESP32)
    WiFi.setHostname(hostname);
#else
    WiFi.persistent(false);
#endif
    WiFi.mode(WIFI_STA);
#if defined(ESP8266)
    WiFi.hostname(hostname);
    _disconnectHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& event) {
        _disconnectReason = event.reason;
        Serial.printf("[WIFI] Disconnected: reason=%d (%s)\n", _disconnectReason, disconnectReasonName(_disconnectReason));
    });
#endif
    WiFi.setAutoReconnect(true);
    WiFi.begin(_ssid.c_str(), _password.c_str());
    _attempt = 1;
    _lastRetryMs = millis();
    Serial.printf("[WIFI] Connection attempt %u started\n", _attempt);
}

void NetworkManager::tick() {
    if (_attempt == 0) return;
    bool nowConnected = connected();
    uint32_t now = millis();
#if defined(ESP8266)
    if (nowConnected && _scanStarted) {
        WiFi.scanDelete();
        _scanStarted = false;
    }
    if (!nowConnected && !_scanFinished && now - _lastRetryMs >= CONNECTION_TIMEOUT_MS) {
        if (!_scanStarted) {
            Serial.println("[WIFI] Scanning for 2.4 GHz access points...");
            WiFi.scanNetworks(true);
            _scanStarted = true;
        }
        int count = WiFi.scanComplete();
        if (count >= 0) {
            bool found = false;
            for (int i = 0; i < count; i++) {
                if (WiFi.SSID(i) != _ssid) continue;
                found = true;
                Serial.printf("[WIFI] Target visible: channel=%d RSSI=%d dBm encryption=%d\n", WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i));
            }
            if (!found) Serial.printf("[WIFI] Target not found among %d visible 2.4 GHz APs; check SSID, band and signal\n", count);
            WiFi.scanDelete();
            _scanFinished = true;
            _scanStarted = false;
            _lastRetryMs = now - CONNECTION_TIMEOUT_MS;
        } else if (count == WIFI_SCAN_FAILED) {
            Serial.println("[WIFI] Scan failed");
            _scanFinished = true;
            _scanStarted = false;
        }
    }
#endif
    if (nowConnected && !_wasConnected) {
        Serial.printf("[WIFI] Connected to: %s\n", WiFi.SSID().c_str());
        Serial.printf("[WIFI] IP address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WIFI] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());
#if defined(ESP8266)
        _disconnectReason = 0;
#endif
    } else if (!nowConnected && _wasConnected) {
        Serial.printf("[WIFI] Connection lost, status=%d\n", WiFi.status());
        _lastRetryMs = now - _retryIntervalMs;
    }
    if (!nowConnected && now - _lastLogMs >= STATUS_INTERVAL_MS) {
#if defined(ESP8266)
        Serial.printf("[WIFI] Waiting: SSID=%s status=%d sdk=%d reason=%d (%s) attempt=%u\n", _ssid.c_str(), WiFi.status(), wifi_station_get_connect_status(), _disconnectReason, disconnectReasonName(_disconnectReason), _attempt);
#else
        Serial.printf("[WIFI] Waiting: SSID=%s status=%d attempt=%u\n", _ssid.c_str(), WiFi.status(), _attempt);
#endif
        _lastLogMs = now;
    }
    bool failed = WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_WRONG_PASSWORD;
#if defined(ESP8266)
    failed = failed || _disconnectReason != 0;
#endif
    if (!nowConnected && now - _lastRetryMs >= (failed ? _retryIntervalMs : CONNECTION_TIMEOUT_MS)
#if defined(ESP8266)
        && !_scanStarted && _scanFinished
#endif
    ) {
        if (_attempt < 255) _attempt++;
        Serial.printf("[WIFI] Retrying connection to %s (attempt %u)\n", _ssid.c_str(), _attempt);
#if defined(ESP8266)
        _disconnectReason = 0;
#endif
        WiFi.begin(_ssid.c_str(), _password.c_str());
        _lastRetryMs = now;
    }
    _wasConnected = nowConnected;
}

bool NetworkManager::connected() const { return WiFi.status() == WL_CONNECTED; }
const String& NetworkManager::ssid() const { return _ssid; }
String NetworkManager::ipAddress() const { return WiFi.localIP().toString(); }
