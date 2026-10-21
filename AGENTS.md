# AGENTS.md

## Project

ModuSwitch is PlatformIO firmware for ESP8266 Wemos D1 mini and ESP32-compatible boards. It controls direct GPIO and PCF8574 I2C relay/input hardware, serves a LittleFS Web UI, and exposes REST/WebSocket APIs.

## Repository layout

- `src/`: firmware implementation.
- `include/`: firmware interfaces.
- `web/`: editable Web UI sources and `config.example.json`.
- `data/`: generated, minified LittleFS output. Do not edit manually.
- `tools/build_web.py`: generates `data/` from `web/`.
- `test/`: device/API test scripts.
- `upload.bat`, `upload.sh`: build web assets and upload firmware/filesystem.

## Commands

Run from the repository root:

```sh
pio run -e d1_mini
pio run -e d1_mini -t upload
pio run -e d1_mini -t uploadfs
pio run -e esp32dev
python tools/build_web.py
python test/api_test.py
python test/gpio_toggle_test.py
```

Use `upload.bat firmware|fs|all` on Windows or `./upload.sh firmware|fs|all` on Linux/macOS.

## Configuration and secrets

- Copy `web/config.example.json` to `web/config.local.json` for a local device configuration.
- Never commit `web/config.local.json`, `data/config.json`, Wi-Fi credentials, tokens, passwords, or production identifiers.
- Run `python tools/build_web.py` before a filesystem upload; it copies `config.local.json` when present, otherwise uses the public example.
- Validate configuration before applying it. Keep relay and automation references based on `pinId`, never duplicate physical pin addressing in relay or automation definitions.

## Firmware conventions

- Keep the loop non-blocking; periodic work belongs in `tick()`.
- Do not call HTTP directly from input callbacks; use the queue.
- Public relay control requires both `access.anonymousControl` and the relay `publicControl` flag.
- WebSocket/UI state is pessimistic: display the actual firmware state after confirmation.
- Maintain backup/atomic config saving behavior.
- Avoid large allocations and unnecessary `String` use in hot ESP8266 paths.

## GPIO safety

- ESP8266 I2C normally uses GPIO4/D2 and GPIO5/D1; do not offer them as general GPIO when I2C is active.
- Avoid ESP8266 boot pins GPIO0/D3, GPIO2/D4, and GPIO15/D8 for relay outputs.
- Avoid serial GPIO1/GPIO3 and flash GPIO6–GPIO11.
- Follow the curated UI pin catalogs; update them when supporting new boards.

## Verification

After modifying firmware, run `pio run -e d1_mini`.
After modifying Web UI, run `python tools/build_web.py` and validate JavaScript with `node --check web/app.js` if Node is installed.
Run relevant device/API tests only when the configured device is online.
