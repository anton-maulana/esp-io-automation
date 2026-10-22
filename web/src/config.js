import {
  state,
  el,
  escapeHtml,
  setSession,
  clearSession,
  showForm,
  hideForm,
} from "./core.js";
import {
  fetchConfig,
  resourceRequest,
  deleteResource,
  login,
  updatePassword,
  backupConfig,
  restoreConfig,
} from "./api.js";

export function loadConfig() {
  el("gpio-list").textContent = "Loading GPIO Switch...";
  el("extend-list").textContent = "Loading Extend Switch...";
  return fetchConfig().then((config) => {
    fillDeviceForm(config);
    renderConfigLists();
    populateActionOptions();
    return config;
  });
}

function fillDeviceForm(config) {
  const device = config.device || {};
  const wifi = (config.network || {}).wifi || {};
  const time = config.time || {};
  const rtc = time.ds3231 || {};
  const form = el("device-form");
  form.deviceName.value = device.name || "";
  form.deviceId.value = device.id || "";
  form.timezone.value = device.timezone || "Asia/Jakarta";
  form.hostname.value = wifi.hostname || "";
  form.ssid.value = wifi.ssid || "";
  form.reconnectIntervalMs.value = wifi.reconnectIntervalMs || 5000;
  form.timeSource.value = time.source || "auto";
  form.ntpEnabled.checked = time.ntpEnabled !== false;
  form.ds3231Enabled.checked = rtc.enabled !== false;
  form.ds3231Address.value = `0x${Number(rtc.address || 104).toString(16)}`;
  form.anonymousControl.checked = !!(config.access || {}).anonymousControl;
  el("device-status").textContent = "Configuration loaded";
}

function populateActionOptions() {
  const config = state.config;
  const relaySelect = el("action-relay-select");
  relaySelect.innerHTML = "";
  (config.relays || []).forEach((relay) => {
    const option = document.createElement("option");
    option.value = relay.id;
    option.textContent = `${relay.name || relay.id} (${relay.id})`;
    relaySelect.appendChild(option);
  });
  const pinSelect = el("action-trigger-pin");
  pinSelect.innerHTML = "";
  (config.pins || [])
    .filter((pin) => pin.mode === "input")
    .forEach((pin) => {
      const option = document.createElement("option");
      option.value = pin.id;
      option.textContent = pin.name || pin.id;
      pinSelect.appendChild(option);
    });
}

export function addIo(initial = {}) {
  const row = document.createElement("div");
  row.className = "io-row";
  const pin = Number.isInteger(initial.pin) ? initial.pin : "";
  const name = escapeHtml(initial.name || "");
  const level = initial.level ? "high" : "low";
  const mode = initial.mode === "input" ? "input" : "output";
  const activeLevel = initial.activeLevel === 0 ? "low" : "high";
  row.innerHTML =
    `<input name="pin" type="number" min="0" max="7" placeholder="Pin 0-7" required value="${pin}">` +
    `<select name="mode"><option value="output"${mode === "output" ? " selected" : ""}>Output</option><option value="input"${mode === "input" ? " selected" : ""}>Input</option></select>` +
    `<input name="name" placeholder="I/O Name" required value="${name}">` +
    `<select name="activeLevel"><option value="high"${activeLevel === "high" ? " selected" : ""}>Active High (ON = HIGH)</option><option value="low"${activeLevel === "low" ? " selected" : ""}>Active Low (ON = LOW)</option></select>` +
    `<select name="default"><option value="low"${level === "low" ? " selected" : ""}>Low</option><option value="high"${level === "high" ? " selected" : ""}>High</option></select>` +
    `<button type="button" class="plain remove-io">×</button>`;
  row.querySelector(".remove-io").onclick = () => row.remove();
  el("extend-io-list").appendChild(row);
}

export function renderConfigLists() {
  if (!state.config) return;
  const config = state.config;
  const renderList = (id, items, render) => {
    const root = el(id);
    root.innerHTML = "";
    items.forEach((item) => root.appendChild(render(item)));
  };
  renderList(
    "gpio-list",
    (config.pins || []).filter((pin) => pin.source === "gpio"),
    (pin) => {
      const item = document.createElement("div");
      item.className = "config-item";
      item.innerHTML = `<div><strong>GPIO${escapeHtml(String(pin.gpio))} · ${escapeHtml(pin.name)}</strong><span class="config-meta">${escapeHtml(pin.mode)} · default ${pin.initialState ? "High" : "Low"}</span></div><div class="item-actions"><button type="button" class="button ghost edit">Edit</button><button type="button" class="button danger delete">Delete</button></div>`;
      item.querySelector(".edit").onclick = () => {
        showForm("gpio");
        const form = el("gpio-form");
        form.dataset.editId = pin.id;
        form.gpio.value = pin.gpio;
        form.mode.value = pin.mode;
        form.name.value = pin.name;
        form.activeLevel.value =
          (state.config.relays || []).find((relay) => relay.pinId === pin.id)
            ?.activeLevel === 0
            ? "low"
            : "high";
        form.defaultCondition.value = pin.initialState ? "high" : "low";
      };
      item.querySelector(".delete").onclick = () => {
        if (!confirm("Delete this GPIO and its related relay?")) return;
        deleteResource("/api/gpio", pin.id)
          .then(() => refreshConfig())
          .catch((error) => {
            alert(`Failed to delete GPIO: ${error.message}`);
            refreshConfig();
          });
      };
      return item;
    },
  );
  renderList("extend-list", (config.hardware || {}).pcfs || [], (pcf) => {
    const item = document.createElement("div");
    item.className = "config-item";
    item.innerHTML = `<div><strong>${escapeHtml(pcf.id)}</strong><span class="config-meta">Address 0x${Number(pcf.address).toString(16)} · PCF8574</span></div><div class="item-actions"><button type="button" class="button ghost edit">Edit</button><button type="button" class="button danger delete">Delete</button></div>`;
    item.querySelector(".edit").onclick = () => {
      showForm("extend");
      const form = el("extend-form");
      form.dataset.editId = pcf.id;
      form.address.value = `0x${Number(pcf.address).toString(16)}`;
      form.name.value = pcf.name || pcf.id;
      const ioList = el("extend-io-list");
      ioList.innerHTML = "";
      (config.pins || [])
        .filter((pin) => pin.pcfId === pcf.id)
        .forEach((pin) =>
          addIo({
            pin: pin.pin,
            mode: pin.mode,
            name: pin.name,
            level: pin.initialState ? 1 : 0,
            activeLevel:
              (config.relays || []).find((relay) => relay.pinId === pin.id)
                ?.activeLevel ?? 1,
          }),
        );
      if (!ioList.children.length) addIo();
    };
    item.querySelector(".delete").onclick = () => {
      if (
        !confirm(
          "Delete this Extend Switch? Its related pins and relays will also be deleted.",
        )
      )
        return;
      deleteResource("/api/extend-switch", pcf.id)
        .then(() => refreshConfig())
        .catch((error) => {
          alert(`Failed to delete Extend Switch: ${error.message}`);
          refreshConfig();
        });
    };
    return item;
  });
  renderList("action-list", config.automations || [], (action) => {
    const item = document.createElement("div");
    item.className = "config-item";
    item.innerHTML = `<div><strong>${escapeHtml(action.name || action.id)}</strong><span class="config-meta">${escapeHtml((action.trigger && action.trigger.type) || "action")}</span></div><div class="item-actions"><button type="button" class="button ghost edit">Edit</button><button type="button" class="button danger delete">Delete</button></div>`;
    item.querySelector(".edit").onclick = () => {
      showForm("action");
      fillActionForm(action);
    };
    item.querySelector(".delete").onclick = () => {
      if (!confirm("Delete this Action?")) return;
      deleteResource("/api/actions", action.id)
        .then(() => refreshConfig())
        .catch((error) => {
          alert(`Failed to delete Action: ${error.message}`);
          refreshConfig();
        });
    };
    return item;
  });
}

function fillActionForm(action) {
  const form = el("action-form");
  form.dataset.editId = action.id;
  form.dataset.editName = action.name || action.id;
  const trigger = action.trigger || {};
  const act = (action.actions && action.actions[0]) || {};
  el("action-trigger").value = trigger.type || "pin_change";
  el("action-condition").value = trigger.edge === "rising" ? "high" : "low";
  el("action-trigger-pin").value = trigger.pinId || "";
  el("action-relay-select").value = act.relayId || "";
  el("action-action").value = act.type || "toggle_relay";
  el("action-state").value = act.state ? "on" : "off";
  form.querySelector('[name="hour"]').value = trigger.hour ?? "";
  form.querySelector('[name="minute"]').value = trigger.minute ?? "";
  form.querySelector('[name="requestUrl"]').value = trigger.url || "";
  syncActionFields();
}

function refreshConfig() {
  return loadConfig().catch(() => {
    el("sync-status").textContent = "Saved; synchronization failed";
  });
}

export function syncLoginUI() {
  if (!state.token) {
    state.admin = false;
    el("menu-button").classList.add("hidden");
    el("admin-drawer").classList.remove("open");
    el("login-button").classList.remove("hidden");
    return;
  }
  state.admin = true;
  el("login-modal").classList.add("hidden");
  el("login-error").textContent = "";
  el("menu-button").classList.remove("hidden");
  el("admin-drawer").classList.add("open");
  el("login-button").classList.add("hidden");
  loadConfig().catch((error) => {
    if (error.message === "session_expired") {
      clearSession();
      el("menu-button").classList.add("hidden");
      el("admin-drawer").classList.remove("open");
      el("login-button").classList.remove("hidden");
      return;
    }
    if (state.token) {
      el("login-error").textContent =
        "Login succeeded, but the configuration could not be loaded.";
    }
  });
}

export function bindConfigEvents() {
  el("login-form").onsubmit = (event) => {
    event.preventDefault();
    const form = new FormData(event.target);
    login(form.get("username"), form.get("password"))
      .then((data) => {
        setSession(data.token, data.expiresInMs);
        syncLoginUI();
      })
      .catch(() => {
        el("login-error").textContent = "Invalid username or password.";
      });
  };
  el("gpio-form").onsubmit = saveGpio;
  el("extend-form").onsubmit = saveExtend;
  el("action-form").onsubmit = saveAction;
  el("device-form").onsubmit = saveDevice;
  el("password-form").onsubmit = savePassword;
  el("action-trigger").onchange = syncActionFields;
  el("action-action").onchange = syncActionFields;
}

function syncActionFields() {
  const trigger = el("action-trigger").value;
  const actionType = el("action-action").value;
  el("schedule-fields").classList.toggle("hidden", trigger !== "schedule");
  el("request-field").classList.toggle("hidden", trigger !== "request");
  el("trigger-pin-field").classList.toggle("hidden", trigger !== "pin_change");
  el("action-condition-field").classList.toggle(
    "hidden",
    trigger !== "pin_change",
  );
  el("action-state-field").classList.toggle(
    "hidden",
    actionType !== "set_relay",
  );
}

function saveGpio(event) {
  event.preventDefault();
  const form = new FormData(event.target);
  const editing = event.target.dataset.editId;
  const gpio = Number(form.get("gpio"));
  const mode = form.get("mode");
  const name = form.get("name");
  const level = form.get("defaultCondition") === "high" ? 1 : 0;
  const activeLevel = form.get("activeLevel") === "low" ? 0 : 1;
  const allowed = [...el("gpio-pin-select").options].some(
    (option) => Number(option.value) === gpio,
  );
  if (!allowed) return alert("Choose a GPIO from the safe pin list.");
  const duplicate = (state.config.pins || []).some(
    (pin) =>
      pin.source === "gpio" && pin.id !== editing && Number(pin.gpio) === gpio,
  );
  if (duplicate) return alert("GPIO is already registered");
  const id = editing || `pin-gpio-${gpio}`;
  const pin = {
    id,
    name,
    source: "gpio",
    gpio,
    mode,
    initialState: level,
    safeState: level,
    enabled: true,
    debounceMs: mode === "input" ? 50 : 0,
  };
  if (mode === "output") {
    pin.relay = {
      id: `relay-${id}`,
      name,
      room: "GPIO Switch",
      icon: "bulb",
      pinId: id,
      activeLevel,
      enabled: true,
      publicControl: true,
      startupPolicy: level ? "on" : "off",
    };
  } else {
    pin.deleteRelay = true;
  }
  return resourceRequest("/api/gpio", editing ? "PUT" : "POST", pin)
    .then((saved) => {
      delete event.target.dataset.editId;
      hideForm("gpio");
      alert(
        saved && saved.rebootRequired
          ? "GPIO saved. Reboot the device to apply hardware changes."
          : "GPIO saved.",
      );
      return refreshConfig();
    })
    .catch((error) => {
      alert(`Failed to save GPIO: ${error.message}`);
      refreshConfig();
    });
}

function saveExtend(event) {
  event.preventDefault();
  const form = event.target;
  const data = new FormData(form);
  const editing = form.dataset.editId;
  const id = editing || `pcf-${Date.now()}`;
  const address = parseInt(String(data.get("address")).replace(/^0x/i, ""), 16);
  const name = data.get("name");
  const rows = [...form.querySelectorAll(".io-row")].map((row) => ({
    pin: Number(row.querySelector('[name="pin"]').value),
    mode: row.querySelector('[name="mode"]').value,
    name: row.querySelector('[name="name"]').value,
    level: row.querySelector('[name="default"]').value === "high" ? 1 : 0,
    activeLevel: row.querySelector('[name="activeLevel"]').value === "low" ? 0 : 1,
  }));
  const pins = [];
  const relays = [];
  const seen = new Set();
  rows.forEach((row) => {
    if (seen.has(row.pin)) return;
    seen.add(row.pin);
    const pinId = `pin-${id}-${row.pin}`;
    pins.push({
      id: pinId,
      name: row.name,
      source: "pcf",
      pcfId: id,
      pin: row.pin,
      mode: row.mode,
      initialState: row.level,
      safeState: row.level,
      enabled: true,
      debounceMs: row.mode === "input" ? 50 : 0,
    });
    if (row.mode === "output") {
      relays.push({
        id: `relay-${pinId}`,
        name: row.name,
        room: name || "Extend Switch",
        icon: "bulb",
        pinId,
        activeLevel: row.activeLevel,
        enabled: true,
        publicControl: true,
        startupPolicy: row.level ? "on" : "off",
      });
    }
  });
  return resourceRequest("/api/extend-switch", editing ? "PUT" : "POST", {
    pcf: { id, address, name, type: "PCF8574", enabled: true },
    pins,
    relays,
  })
    .then((saved) => {
      delete form.dataset.editId;
      hideForm("extend");
      alert(
        saved && saved.rebootRequired
          ? "Extend Switch saved. Reboot the device to apply hardware changes."
          : "Extend Switch saved.",
      );
      return refreshConfig();
    })
    .catch((error) => {
      alert(`Failed to save Extend Switch: ${error.message}`);
      refreshConfig();
    });
}
function saveAction(event) {
  event.preventDefault();
  const form = event.target;
  const data = new FormData(form);
  const editing = form.dataset.editId;
  const id = editing || `auto-${Date.now()}`;
  const triggerType = data.get("trigger");
  const actionType = data.get("action");
  const relayId = data.get("relayId");
  if (!relayId) return alert("Choose a relay target.");
  const trigger = { type: triggerType };
  if (triggerType === "pin_change") {
    const pinId = data.get("triggerPinId");
    if (!pinId) return alert("Choose a trigger input pin.");
    trigger.pinId = pinId;
    trigger.edge = data.get("condition") === "high" ? "rising" : "falling";
  } else if (triggerType === "schedule") {
    trigger.hour = Number(data.get("hour") || 0);
    trigger.minute = Number(data.get("minute") || 0);
  } else if (triggerType === "request") {
    trigger.url = data.get("requestUrl") || "";
  }
  const actionObj = { type: actionType, relayId };
  if (actionType === "set_relay")
    actionObj.state = data.get("actionState") === "on";
  const automation = {
    id,
    name: form.dataset.editName || `Action ${id}`,
    enabled: true,
    trigger,
    actions: [actionObj],
  };
  return resourceRequest("/api/actions", editing ? "PUT" : "POST", automation)
    .then(() => {
      delete form.dataset.editId;
      delete form.dataset.editName;
      hideForm("action");
      return refreshConfig();
    })
    .catch((error) => {
      alert(`Failed to save Action: ${error.message}`);
      refreshConfig();
    });
}
function savePassword(event) {
  event.preventDefault();
  const form = new FormData(event.target);
  const currentPassword = form.get("currentPassword");
  const newPassword = form.get("newPassword");
  const confirmPassword = form.get("confirmPassword");
  const status = el("password-status");
  if (newPassword !== confirmPassword) {
    status.textContent = "Password confirmation does not match.";
    return;
  }
  status.textContent = "Updating password...";
  updatePassword(currentPassword, newPassword)
    .then(() => {
      event.target.reset();
      status.textContent = "Password updated successfully.";
    })
    .catch((error) => {
      status.textContent =
        error.message === "invalid_current_password"
          ? "The current password is incorrect."
          : `Failed: ${error.message}`;
    });
}

function saveDevice(event) {
  event.preventDefault();
  const form = new FormData(event.target);
  if (!state.config) return alert("Configuration not loaded");
  const wifi = {
    ...state.config.network.wifi,
    hostname: form.get("hostname"),
    ssid: form.get("ssid"),
    reconnectIntervalMs: Number(form.get("reconnectIntervalMs")),
  };
  if (form.get("password")) wifi.password = form.get("password");
  const changes = {
    device: {
      ...state.config.device,
      name: form.get("deviceName"),
      id: form.get("deviceId"),
      timezone: form.get("timezone"),
    },
    network: { wifi },
    time: {
      ...(state.config.time || {}),
      source: form.get("timeSource"),
      ntpEnabled: form.get("ntpEnabled") === "on",
      ds3231: {
        ...((state.config.time || {}).ds3231 || {}),
        enabled: form.get("ds3231Enabled") === "on",
        address: parseInt(form.get("ds3231Address")),
      },
    },
    access: {
      ...(state.config.access || {}),
      anonymousControl: form.get("anonymousControl") === "on",
    },
  };
  return resourceRequest("/api/device", "PUT", changes)
    .then((saved) =>
      refreshConfig().then(() => {
        el("device-status").textContent =
          saved && saved.rebootRequired
            ? "Saved; reboot required"
            : "Saved";
      }),
    )
    .catch((error) => {
      el("device-status").textContent = `Failed: ${error.message}`;
      refreshConfig();
    });
}
export function logout() {
  state.admin = false;
  state.config = null;
  clearSession();
  el("menu-button").classList.add("hidden");
  el("login-button").classList.remove("hidden");
}
