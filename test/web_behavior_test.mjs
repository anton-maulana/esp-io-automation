import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";

function makeClassList() {
  const set = new Set();
  return {
    add(...items) {
      items.forEach((item) => set.add(item));
    },
    remove(...items) {
      items.forEach((item) => set.delete(item));
    },
    toggle(item, force) {
      if (force === undefined) {
        set.has(item) ? set.delete(item) : set.add(item);
      } else if (force) {
        set.add(item);
      } else {
        set.delete(item);
      }
    },
    contains(item) {
      return set.has(item);
    },
  };
}

function makeElement() {
  const base = {
    classList: makeClassList(),
    dataset: {},
    children: [],
    options: [],
    textContent: "",
    innerHTML: "",
    value: "",
    checked: false,
    disabled: false,
    files: [],
    style: {},
    appendChild(child) {
      this.children.push(child);
      return child;
    },
    querySelector() {
      return makeElement();
    },
    querySelectorAll() {
      return [];
    },
    dispatchEvent() {},
    click() {},
    remove() {},
  };
  return new Proxy(base, {
    get(target, prop) {
      if (prop in target) return target[prop];
      if (typeof prop === "symbol") return undefined;
      const child = makeElement();
      target[prop] = child;
      return child;
    },
    set(target, prop, value) {
      target[prop] = value;
      return true;
    },
  });
}

const elements = new Map();
const document = {
  cookie: "",
  getElementById(id) {
    if (!elements.has(id)) elements.set(id, makeElement());
    return elements.get(id);
  },
  querySelectorAll() {
    return [];
  },
  createElement: makeElement,
};

const config = {
  schemaVersion: 1,
  device: { id: "switch-001", name: "TestDevice", timezone: "Asia/Jakarta" },
  network: {
    wifi: { ssid: "ssid", password: "pw", hostname: "host", reconnectIntervalMs: 5000 },
    server: { deviceToken: "" },
  },
  time: { ds3231: { address: 104 } },
  access: { anonymousControl: false, adminUsername: "admin", adminPasswordHash: "x" },
  hardware: { i2c: { sdaGpio: 4, sclGpio: 5 }, pcfs: [] },
  pins: [],
  relays: [],
  automations: [],
};

const requests = [];
let validateResult = { valid: true };
let putBody = null;
let patchBody = null;
let gpioBody = null;
let gpioMethod = null;
let gpioError = null;
let deviceBody = null;
let getConfigOk = true;
const alerts = [];
const fetch = async (url, options = {}) => {
  const method = options.method || "GET";
  requests.push({ url, method, body: options.body });
  if (url === "/api/config/validate") {
    const ok = validateResult.valid;
    return { ok, status: ok ? 200 : 400, json: async () => validateResult };
  }
  if (url === "/api/device" && method === "PUT") {
    deviceBody = JSON.parse(options.body);
    return { ok: true, status: 200, json: async () => ({ saved: true, rebootRequired: true }) };
  }
  if (url === "/api/gpio") {
    gpioBody = JSON.parse(options.body);
    gpioMethod = method;
    const ok = !gpioError;
    return {
      ok,
      status: ok ? 200 : 400,
      json: async () => (ok ? { saved: true } : { error: gpioError }),
    };
  }
  if (url === "/api/config" && method === "PUT") {
    putBody = JSON.parse(options.body);
    config.pins = putBody.pins || [];
    config.relays = putBody.relays || [];
    return { ok: true, status: 200, json: async () => ({ saved: true }) };
  }
  if (url === "/api/config" && method === "GET") {
    if (!getConfigOk) return { ok: false, status: 500, json: async () => ({}) };
    return { ok: true, status: 200, json: async () => config };
  }
  const routes = {
    "/api/status": { board: "esp8266_d1_mini" },
    "/gpio-pins.json": { esp8266_d1_mini: { pins: [{ gpio: 12, label: "D6" }] } },
    "/api/public/dashboard": { deviceName: "TestDevice", relays: [] },
    "/api/time": { source: "rtc", iso: "12:00" },
  };
  if (!Object.hasOwn(routes, url)) throw new Error(`Unexpected request: ${url}`);
  return { ok: true, status: 200, json: async () => routes[url] };
};

function FormDataStub(target) {
  this._data = target && target._data ? target._data : {};
}
FormDataStub.prototype.get = function get(key) {
  return this._data[key] ?? null;
};

const context = vm.createContext({
  document,
  location: { protocol: "http:", host: "device.test" },
  fetch,
  FormData: FormDataStub,
  WebSocket: class {
    constructor(url) {
      this.url = url;
    }
  },
  setTimeout() {},
  clearTimeout() {},
  confirm() {
    return true;
  },
  alert(message) {
    alerts.push(String(message));
  },
  console,
});

document.cookie = "modu_session=testtoken";
vm.runInContext(
  readFileSync(new URL("../data/app.js", import.meta.url), "utf8"),
  context,
);

async function flush() {
  for (let i = 0; i < 30; i++) await new Promise((resolve) => setImmediate(resolve));
}

await flush();
assert.equal(document.getElementById("device-form").deviceName.value, "TestDevice");

const gpioForm = document.getElementById("gpio-form");
elements.get("gpio-pin-select").options = [
  { value: "12" },
  { value: "13" },
  { value: "14" },
];
gpioForm._data = {
  gpio: "12",
  mode: "output",
  name: "Lampu Teras",
  defaultCondition: "high",
};
gpioForm.dataset.editId = "";
gpioForm.onsubmit({ preventDefault() {}, target: gpioForm });
await flush();

assert.equal(gpioMethod, "POST", "new GPIO uses resource POST");
assert.equal(gpioBody.id, "pin-gpio-12");
assert.equal(gpioBody.relay.id, "relay-pin-gpio-12");
assert.ok(alerts.some((message) => message.includes("GPIO saved")));
assert.ok(
  !alerts.some((message) => message.includes("Failed to save GPIO")),
  "successful save must not report failure",
);

alerts.length = 0;
gpioBody = null;
getConfigOk = false;
gpioForm.dataset.editId = "";
gpioForm._data = {
  gpio: "13",
  mode: "output",
  name: "Lampu Lain",
  defaultCondition: "low",
};
gpioForm.onsubmit({ preventDefault() {}, target: gpioForm });
await flush();

assert.ok(gpioBody, "save still succeeds when refresh fails");
assert.ok(
  !alerts.some((message) => message.includes("Failed to save GPIO")),
  "refresh failure must not be reported as save failure",
);
assert.equal(
  document.getElementById("sync-status").textContent,
  "Saved; synchronization failed",
);

getConfigOk = true;
alerts.length = 0;
gpioBody = null;
gpioError = "Duplicate pin id";
gpioForm.dataset.editId = "";
gpioForm._data = {
  gpio: "14",
  mode: "output",
  name: "Lampu Gagal",
  defaultCondition: "low",
};
gpioForm.onsubmit({ preventDefault() {}, target: gpioForm });
await flush();

assert.ok(gpioBody, "resource request still reaches the backend");
assert.ok(
  !alerts.some((message) => message.includes("GPIO saved")),
  "rejected GPIO must not report success",
);
assert.ok(
  alerts.some((message) => message.includes("Failed to save GPIO")),
  "validation failure must be reported",
);

alerts.length = 0;
gpioError = null;
deviceBody = null;
const deviceForm = document.getElementById("device-form");
deviceForm._data = {
  deviceName: "Patched Device",
  deviceId: "switch-002",
  timezone: "Asia/Makassar",
  hostname: "patched-host",
  ssid: "patched-ssid",
  password: "",
  reconnectIntervalMs: "7000",
  timeSource: "ds3231",
  ntpEnabled: "on",
  ds3231Enabled: "on",
  ds3231Address: "0x68",
  anonymousControl: "on",
};
deviceForm.onsubmit({ preventDefault() {}, target: deviceForm });
await flush();

assert.ok(deviceBody, "expected a device update");
assert.ok(!("pins" in deviceBody), "device update must not resend pins");
assert.ok(!("relays" in deviceBody), "device update must not resend relays");
assert.equal(deviceBody.device.name, "Patched Device");
assert.equal(deviceBody.device.timezone, "Asia/Makassar");
assert.equal(deviceBody.time.source, "ds3231");
assert.equal(deviceBody.time.ntpEnabled, true);
assert.equal(deviceBody.access.anonymousControl, true);
assert.equal(deviceBody.network.wifi.reconnectIntervalMs, 7000);
assert.equal(
  document.getElementById("device-status").textContent,
  "Saved; reboot required",
);

console.log(
  "Web behavior test passed: resource CRUD flow, save/refresh decoupling, rejection handling, and device update.",
);
