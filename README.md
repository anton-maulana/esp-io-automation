# ModuSwitch

## Project

ModuSwitch is an Arduino/PlatformIO smart-switch firmware for Wemos D1 mini ESP8266 and ESP32-compatible targets. It controls relays through direct GPIO or PCF8574 I2C expanders, reads button inputs, runs local automations, exposes a REST API and WebSocket, and serves an admin Web UI from LittleFS.

## Features

- Direct GPIO and PCF8574 relay/input control.
- Local pin-change and scheduled relay automation.
- REST API and WebSocket relay state updates, including scheduled actions.
- Admin login with session token and authenticated admin-password updates.
- Config validation, redacted backup download, validated restore, atomic save, and safe cleanup of orphaned automations.
- Anonymous relay control can be enabled for public relays.
- NTP fallback when DS3231 is unavailable, with device timezone support.
- Active High and Active Low relay polarity per GPIO or PCF output.
- OTA firmware upload endpoint and authenticated reboot endpoint.
- Admin UI for dashboard, GPIO, expand switch, action, device settings, password update, and backup/restore.

## Requirements

- PlatformIO Core or PlatformIO IDE.
- Node.js LTS and npm for building Web UI assets.
- USB connection to a Wemos D1 mini for serial upload.
- A 2.4 GHz Wi-Fi network for ESP8266.
- PCF8574 powered at 3.3 V when using an I2C expander.
- Relay driver hardware suitable for the relay load.

## First-time setup

1. Create a local configuration:

   ```powershell
   Copy-Item web/config.example.json web/config.local.json
   ```

2. Edit `web/config.local.json` and set the Wi-Fi SSID/password, hostname, device identity, and hardware configuration.
3. Build the Web UI and generate the LittleFS files:

   ```powershell
    npm ci
    npm run build
   ```

4. Upload the filesystem:

   ```powershell
   .\upload.bat fs
   ```

5. Upload firmware:

   ```powershell
   .\upload.bat firmware
   ```

The generated `data/` directory is not a source directory and is ignored by Git. It contains minified web assets and the generated `config.json` used by LittleFS.

## Upload commands

Windows:

```powershell
.\upload.bat firmware
.\upload.bat fs
.\upload.bat all
```

Linux/macOS:

```sh
chmod +x upload.sh
./upload.sh firmware
./upload.sh fs
./upload.sh all
```

Direct PlatformIO commands:

```sh
pio run -e d1_mini
pio run -e d1_mini -t upload
pio run -e d1_mini -t uploadfs
pio run -e esp32dev
```

Serial monitor:

```sh
pio device monitor -e d1_mini
```

## Configuration

Configuration is stored as JSON and validated before it is applied.

- `device`: device ID, display name, and timezone.
- `time`: NTP enablement and optional DS3231 address.
- `network.wifi`: SSID, password, hostname, and reconnect interval.
- `access`: admin username, password hash, session timeout, and anonymous control.
- `hardware.i2c`: SDA/SCL GPIO and clock speed.
- `hardware.pcfs`: PCF8574 IDs and I2C addresses.
- `pins`: the hardware abstraction. GPIO pins use `source: gpio`; PCF pins use `source: pcf`, `pcfId`, and `pin` 0–7.
- `relays`: logical relays refer to hardware only through `pinId`; `activeLevel` is `0` for Active Low or `1` for Active High, and `startupPolicy` controls the initial logical state.
- `automations`: triggers and relay actions refer to logical `pinId` and relay IDs. Schedule triggers use the configured device timezone.

The Device page provides admin password updates and Backup & Restore Config. Backups redact Wi-Fi passwords, server tokens, and the admin password hash. Restore validates the complete configuration and preserves current secrets for redacted values.

Never commit `web/config.local.json`, generated `data/config.json`, Wi-Fi credentials, server tokens, or production secrets.

## API

Admin login:

```sh
curl -X POST http://DEVICE_IP/api/auth/login \
  -H 'Content-Type: application/json' \
  --data '{"username":"admin","password":"admin"}'
```

Authenticated configuration requests use `X-Session-Token`:

```sh
curl http://DEVICE_IP/api/config -H 'X-Session-Token: TOKEN'
```

Useful endpoints:

- `GET /api/public/dashboard`
- `POST /api/auth/login`
- `GET /api/config`
- `PUT /api/config`
- `POST /api/config/validate`
- `GET /api/config/backup` — authenticated redacted backup download.
- `POST /api/config/restore` — authenticated JSON restore with validation.
- `POST /api/auth/password` — authenticated admin password update; verifies the current password.
- `GET /api/time`
- `POST /api/relay/toggle?id=RELAY_ID`
- `POST /api/ota`
- `POST /api/reboot`
- WebSocket `/ws`

## Tests

API tests require the device to be online. Update the IP in `test/api_test.py` if needed:

```sh
python test/api_test.py
python test/gpio_toggle_test.py
```

## Hardware notes

For Wemos D1 mini, reserve GPIO4/D2 and GPIO5/D1 for I2C. Avoid GPIO0/D3, GPIO2/D4, and GPIO15/D8 for relays because they affect boot. Avoid GPIO1/GPIO3 for normal I/O because they are serial pins, and GPIO6–GPIO11 because they are connected to flash.

Use a suitable transistor/relay driver and common ground. Do not drive relay coils directly from an MCU pin.

## License

Add the project license before publishing if this repository will be distributed publicly.
