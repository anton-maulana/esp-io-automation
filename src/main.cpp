#include <Arduino.h>
#include "ConfigManager.h"
#include "ConfigValidator.h"
#include "PcfManager.h"
#include "GpioManager.h"
#include "PinManager.h"
#include "RelayManager.h"
#include "AutomationEngine.h"
#include "HttpQueue.h"
#include "NetworkManager.h"
#include "TimeManager.h"
#include "ApiServer.h"
#include "Diagnostics.h"
#include <ctype.h>

ConfigManager config;
ConfigValidator validator;
PcfManager pcf;
GpioManager gpioMgr;
PinManager pins;
RelayManager relays;
AutomationEngine automation;
HttpQueue http;
NetworkManager network;
TimeManager timeManager;
ApiServer api;

static String serialLine;

void processSerialCommand() {
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c != '\n') {
            if (serialLine.length() < 160) serialLine += c;
            continue;
        }
        serialLine.trim();
        if (serialLine.startsWith("WIFI=")) {
            String credentials = serialLine.substring(5);
            int separator = credentials.indexOf(',');
            if (separator <= 0) {
                Serial.println("[TTL] ERROR: gunakan WIFI=SSID,PASSWORD");
            } else {
                String ssid = credentials.substring(0, separator);
                String password = credentials.substring(separator + 1);
                ssid.trim();
                password.trim();
                if (ssid.length() == 0 || password.length() == 0) {
                    Serial.println("[TTL] ERROR: SSID dan password wajib diisi");
                } else {
                    config.doc()["network"]["wifi"]["ssid"] = ssid;
                    config.doc()["network"]["wifi"]["password"] = password;
                    if (config.save()) {
                        Serial.printf("[TTL] Wi-Fi tersimpan untuk SSID: %s. Tekan reset/reboot.\n", ssid.c_str());
                    } else {
                        Serial.println("[TTL] ERROR: gagal menyimpan Wi-Fi ke config.json");
                    }
                }
            }
        } else if (serialLine.startsWith("SSID=")) {
            String ssid = serialLine.substring(5);
            ssid.trim();
            if (ssid.length() == 0) {
                Serial.println("[TTL] ERROR: SSID kosong");
            } else {
                config.doc()["network"]["wifi"]["ssid"] = ssid;
                if (config.save()) {
                    Serial.printf("[TTL] SSID tersimpan: %s. Tekan reset/reboot.\n", ssid.c_str());
                } else {
                    Serial.println("[TTL] ERROR: gagal menyimpan SSID ke config.json");
                }
            }
        } else if (serialLine.startsWith("PASSWORD=")) {
            String password = serialLine.substring(9);
            password.trim();
            if (password.length() == 0) {
                Serial.println("[TTL] ERROR: password kosong");
            } else {
                config.doc()["network"]["wifi"]["password"] = password;
                if (config.save()) Serial.println("[TTL] Password Wi-Fi tersimpan. Tekan reset/reboot.");
                else Serial.println("[TTL] ERROR: gagal menyimpan password ke config.json");
            }
        } else if (serialLine == "LOG=ON" || serialLine == "LOG=OFF") {
            bool enabled = serialLine == "LOG=ON";
            config.doc()["logging"]["enabled"] = enabled;
            if (config.save()) {
                diagnostics.setEnabled(enabled);
                Serial.printf("[TTL] Logging %s dan tersimpan ke config.json. Tekan reset untuk verifikasi.\n", enabled ? "aktif" : "nonaktif");
            } else {
                Serial.println("[TTL] ERROR: gagal menyimpan logging ke config.json");
            }
        } else if (serialLine == "HELP") {
            Serial.println("[TTL] WIFI=SSID,PASSWORD | SSID=nama_wifi | LOG=ON | LOG=OFF | HELP");
        } else if (serialLine.length() > 0) {
            Serial.println("[TTL] Perintah tidak dikenal. Ketik HELP.");
        }
        serialLine = "";
    }
}

void onPinEdge(uint8_t pinIndex, bool rising) {
    Serial.printf("[INPUT] Pin %u changed: %s\n", pinIndex, rising ? "HIGH" : "LOW");
    automation.onEvent({EventType::PinChanged, pinIndex, static_cast<int8_t>(rising), millis()}, pins, relays, pcf, gpioMgr, http);
    for (uint8_t i = 0; i < relays.count(); i++) {
        if (relays.relay(i).pinIndex == static_cast<int8_t>(pinIndex)) api.publishRelayState(i);
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("========================================");
    Serial.println("ModuSwitch booting");
    Serial.println("========================================");

    Serial.println("[BOOT] Step 1/10: mounting LittleFS");
    if (!config.begin()) { Serial.println("[ERROR] LittleFS mount failed"); return; }
    Serial.println("[BOOT] LittleFS mounted");

    Serial.println("[BOOT] Step 2/10: loading config.json");
    if (!config.load()) { Serial.println("[ERROR] Configuration load failed"); return; }
    Serial.println("[BOOT] Configuration loaded");

    Serial.println("[BOOT] Step 3/10: validating configuration");
    ValidationResult result = validator.validate(config.doc());
    if (!result.valid) { Serial.printf("[ERROR] Config invalid: %s\n", result.error.c_str()); return; }
    Serial.println("[BOOT] Configuration valid");

    JsonObjectConst root = config.doc().as<JsonObjectConst>();
    diagnostics.begin(root["logging"]["enabled"] | false, root["logging"]["level"] | "info");
    diagnostics.log("DEBUG", "Configuration logging enabled");
    JsonObjectConst i2c = root["hardware"]["i2c"];
    Serial.println("[BOOT] Step 4/10: starting I2C and PCF manager");
    pcf.begin(i2c["sdaGpio"] | 4, i2c["sclGpio"] | 5, i2c["clockHz"] | 100000UL);
    pcf.configure(root["hardware"]["pcfs"].as<JsonArrayConst>());
    if (i2c["scanOnBoot"] | true) {
        Serial.println("[PCF] Scanning I2C addresses");
        pcf.scan();
    }

    Serial.println("[BOOT] Step 5/10: configuring pins PCF/GPIO");
    pins.configure(root["pins"].as<JsonArrayConst>(), pcf, gpioMgr);
    Serial.printf("[PIN] Configured %u pins\n", pins.count());

    Serial.println("[BOOT] Step 6/10: configuring relays");
    relays.configure(root["relays"].as<JsonArrayConst>(), pins);
    relays.applyStartup(pins, pcf, gpioMgr);
    Serial.printf("[RELAY] Configured %u relays\n", relays.count());

    Serial.println("[BOOT] Step 7/10: configuring automation");
    automation.configure(root["automations"].as<JsonArrayConst>(), pins, relays, pcf, gpioMgr);
    Serial.printf("[AUTOMATION] Configured %u rules\n", automation.count());

    Serial.println("[BOOT] Step 8/10: starting clock");
    timeManager.begin(root["time"].as<JsonObjectConst>(), root["device"]["timezone"] | "UTC");

    Serial.println("[BOOT] Step 9/10: starting Wi-Fi");
    network.begin(root["network"]["wifi"]["ssid"] | "", root["network"]["wifi"]["password"] | "", root["network"]["wifi"]["hostname"] | "modu-switch", root["network"]["wifi"]["reconnectIntervalMs"] | 5000UL);

    Serial.println("[BOOT] Step 10/10: starting REST API and WebSocket");
    api.begin(config, validator, relays, pins, pcf, gpioMgr, timeManager, automation, http);
    Serial.println("[API] HTTP server: port 80");
    Serial.println("[API] WebSocket: /ws");

    Serial.println("[BOOT] Step 10/10: startup complete");
    Serial.println("[READY] ModuSwitch ready");
}

void loop() {
    processSerialCommand();
    timeManager.tick();
    network.tick();
    pins.tick(pcf, gpioMgr, onPinEdge);
    automation.tick();
    uint8_t changedRelay;
    while (automation.consumeRelayChange(changedRelay)) api.publishRelayState(changedRelay);
    http.tick();
    api.tick();
}
