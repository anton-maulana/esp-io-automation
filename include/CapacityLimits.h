#pragma once
#include <Arduino.h>

namespace CapacityLimits {
#if defined(ESP8266)
static constexpr uint8_t MaxPcf = 8;
static constexpr uint8_t MaxPins = 64;
static constexpr uint8_t MaxRelays = 64;
static constexpr uint8_t MaxAutomations = 20;
static constexpr uint16_t MaxActions = 80;
static constexpr uint8_t MaxHttpQueue = 8;
static constexpr uint8_t MaxChangedRelays = 20;
#else
static constexpr uint8_t MaxPcf = 16;
static constexpr uint8_t MaxPins = 120;
static constexpr uint8_t MaxRelays = 120;
static constexpr uint8_t MaxAutomations = 64;
static constexpr uint16_t MaxActions = 256;
static constexpr uint8_t MaxHttpQueue = 16;
static constexpr uint8_t MaxChangedRelays = 64;
#endif
static constexpr uint16_t MaxIdLength = 48;
static constexpr uint16_t MaxNameLength = 96;
static constexpr uint16_t MaxHttpPathLength = 192;
static constexpr uint16_t MaxHttpBodyLength = 512;
static constexpr size_t MaxConfigBytes = 24576;
}
