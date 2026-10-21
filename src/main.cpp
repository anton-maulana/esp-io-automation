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
    api.begin(config, validator, relays, pins, pcf, gpioMgr, timeManager, automation);
    Serial.println("[API] HTTP server: port 80");
    Serial.println("[API] WebSocket: /ws");

    Serial.println("[BOOT] Step 10/10: startup complete");
    Serial.println("[READY] ModuSwitch ready");
}

void loop() {
    timeManager.tick();
    network.tick();
    pins.tick(pcf, gpioMgr, onPinEdge);
    automation.tick();
    uint8_t changedRelay;
    while (automation.consumeRelayChange(changedRelay)) api.publishRelayState(changedRelay);
    http.tick();
    api.tick();
}
