import {
  state,
  el,
  escapeHtml,
  setSession,
  clearSession,
  showForm,
  hideForm,
  cloneConfig,
} from "./core.js";
import { configRequest, fetchConfig, validateConfig, login } from "./api.js";

export function loadConfig() {
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
  const rtc = (config.time || {}).ds3231 || {};
  const form = el("device-form");
  form.deviceName.value = device.name || "";
  form.deviceId.value = device.id || "";
  form.timezone.value = device.timezone || "Asia/Jakarta";
  form.hostname.value = wifi.hostname || "";
  form.ssid.value = wifi.ssid || "";
  form.reconnectIntervalMs.value = wifi.reconnectIntervalMs || 5000;
  form.ds3231Address.value = `0x${Number(rtc.address || 104).toString(16)}`;
  form.anonymousControl.checked = !!(config.access || {}).anonymousControl;
  el("device-status").textContent = "Konfigurasi dimuat";
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
  row.innerHTML =
    `<input name="pin" type="number" min="0" max="7" placeholder="Pin 0-7" required value="${pin}">` +
    `<select name="mode"><option value="output"${mode === "output" ? " selected" : ""}>Output</option><option value="input"${mode === "input" ? " selected" : ""}>Input</option></select>` +
    `<input name="name" placeholder="Nama I/O" required value="${name}">` +
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
      item.innerHTML = `<div><strong>GPIO${escapeHtml(String(pin.gpio))} · ${escapeHtml(pin.name)}</strong><span class="config-meta">${escapeHtml(pin.mode)} · default ${pin.initialState ? "High" : "Low"}</span></div><div class="item-actions"><button type="button" class="button ghost edit">Edit</button><button type="button" class="button danger delete">Hapus</button></div>`;
      item.querySelector(".edit").onclick = () => {
        showForm("gpio");
        const form = el("gpio-form");
        form.dataset.editId = pin.id;
        form.gpio.value = pin.gpio;
        form.mode.value = pin.mode;
        form.name.value = pin.name;
        form.defaultCondition.value = pin.initialState ? "high" : "low";
      };
      item.querySelector(".delete").onclick = () => {
        if (!confirm("Hapus GPIO ini dan relay terkait?")) return;
        applyDraft(
          (draft) => {
            draft.pins = (draft.pins || []).filter(
              (entry) => entry.id !== pin.id,
            );
            draft.relays = (draft.relays || []).filter(
              (entry) => entry.pinId !== pin.id,
            );
            return {};
          },
          { label: "Gagal menghapus GPIO" },
        );
      };
      return item;
    },
  );
  renderList("extend-list", (config.hardware || {}).pcfs || [], (pcf) => {
    const item = document.createElement("div");
    item.className = "config-item";
    item.innerHTML = `<div><strong>${escapeHtml(pcf.id)}</strong><span class="config-meta">Address 0x${Number(pcf.address).toString(16)} · PCF8574</span></div><div class="item-actions"><button type="button" class="button ghost edit">Edit</button><button type="button" class="button danger delete">Hapus</button></div>`;
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
          }),
        );
      if (!ioList.children.length) addIo();
    };
    item.querySelector(".delete").onclick = () => {
      if (
        !confirm(
          "Hapus Extend Switch ini? Pin dan relay yang terkait juga akan dihapus.",
        )
      )
        return;
      applyDraft(
        (draft) => {
          const ids = (draft.pins || [])
            .filter((pin) => pin.pcfId === pcf.id)
            .map((pin) => pin.id);
          draft.hardware.pcfs = (draft.hardware.pcfs || []).filter(
            (entry) => entry.id !== pcf.id,
          );
          draft.pins = (draft.pins || []).filter((pin) => pin.pcfId !== pcf.id);
          draft.relays = (draft.relays || []).filter(
            (relay) => !ids.includes(relay.pinId),
          );
          return {};
        },
        { label: "Gagal menghapus Extend Switch" },
      );
    };
    return item;
  });
  renderList("action-list", config.automations || [], (action) => {
    const item = document.createElement("div");
    item.className = "config-item";
    item.innerHTML = `<div><strong>${escapeHtml(action.name || action.id)}</strong><span class="config-meta">${escapeHtml((action.trigger && action.trigger.type) || "action")}</span></div><div class="item-actions"><button type="button" class="button ghost edit">Edit</button><button type="button" class="button danger delete">Hapus</button></div>`;
    item.querySelector(".edit").onclick = () => {
      showForm("action");
      fillActionForm(action);
    };
    item.querySelector(".delete").onclick = () => {
      if (!confirm("Hapus Action ini?")) return;
      applyDraft(
        (draft) => {
          draft.automations = (draft.automations || []).filter(
            (entry) => entry.id !== action.id,
          );
          return {};
        },
        { label: "Gagal menghapus Action" },
      );
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

function applyDraft(mutator, opts) {
  if (!state.config) return alert("Konfigurasi belum dimuat");
  const draft = cloneConfig(state.config);
  const result = mutator(draft);
  if (result && result.error) return alert(result.error);
  return validateConfig(draft)
    .then((validation) => {
      if (!validation.valid) throw Error(validation.error || "invalid");
      return configRequest("PUT", draft);
    })
    .then((saved) => {
      state.config = cloneConfig(draft);
      renderConfigLists();
      populateActionOptions();
      opts.onSaved(saved);
      refreshConfig();
    })
    .catch((error) => {
      alert(`${opts.label}: ${error.message}`);
      refreshConfig();
    });
}

function refreshConfig() {
  return loadConfig().catch(() => {
    el("sync-status").textContent = "Tersimpan; sinkronisasi gagal";
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
        "Login berhasil, tetapi konfigurasi gagal dimuat.";
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
        el("login-error").textContent = "Username atau password salah.";
      });
  };
  el("gpio-form").onsubmit = saveGpio;
  el("extend-form").onsubmit = saveExtend;
  el("action-form").onsubmit = saveAction;
  el("device-form").onsubmit = saveDevice;
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
  const allowed = [...el("gpio-pin-select").options].some(
    (option) => Number(option.value) === gpio,
  );
  if (!allowed) return alert("Pilih GPIO dari daftar pin aman.");
  return applyDraft(
    (draft) => {
      draft.pins = draft.pins || [];
      draft.relays = draft.relays || [];
      const duplicate = draft.pins.some(
        (pin) =>
          pin.source === "gpio" &&
          pin.id !== editing &&
          Number(pin.gpio) === gpio,
      );
      if (duplicate) return { error: "GPIO sudah terdaftar" };
      if (editing) {
        const pin = draft.pins.find((entry) => entry.id === editing);
        if (!pin) return { error: "Pin tidak ditemukan" };
        pin.gpio = gpio;
        pin.mode = mode;
        pin.name = name;
        pin.initialState = level;
        pin.safeState = level;
        pin.debounceMs = mode === "input" ? 50 : 0;
        if (mode === "output") {
          if (!draft.relays.some((relay) => relay.pinId === pin.id)) {
            draft.relays.push({
              id: `relay-${pin.id}`,
              name,
              room: "GPIO Switch",
              icon: "bulb",
              pinId: pin.id,
              activeLevel: 1,
              enabled: true,
              publicControl: true,
              startupPolicy: "off",
            });
          }
        } else {
          draft.relays = draft.relays.filter((relay) => relay.pinId !== pin.id);
        }
      } else {
        const id = `pin-gpio-${gpio}`;
        draft.pins.push({
          id,
          name,
          source: "gpio",
          gpio,
          mode,
          initialState: level,
          safeState: level,
          enabled: true,
          debounceMs: mode === "input" ? 50 : 0,
        });
        if (mode === "output") {
          draft.relays.push({
            id: `relay-${id}`,
            name,
            room: "GPIO Switch",
            icon: "bulb",
            pinId: id,
            activeLevel: 1,
            enabled: true,
            publicControl: true,
            startupPolicy: "off",
          });
        }
      }
      return {};
    },
    {
      label: "Gagal menyimpan GPIO",
      onSaved: (saved) => {
        delete event.target.dataset.editId;
        hideForm("gpio");
        alert(
          saved && saved.rebootRequired
            ? "GPIO tersimpan. Reboot perangkat untuk menerapkan hardware."
            : "GPIO tersimpan.",
        );
      },
    },
  );
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
  }));
  return applyDraft(
    (draft) => {
      draft.hardware = draft.hardware || {};
      draft.hardware.pcfs = draft.hardware.pcfs || [];
      const existing = draft.hardware.pcfs.find((entry) => entry.id === id);
      if (existing) Object.assign(existing, { address, name });
      else
        draft.hardware.pcfs.push({
          id,
          address,
          name,
          type: "PCF8574",
          enabled: true,
        });

      const removedPinIds = (draft.pins || [])
        .filter((pin) => pin.pcfId === id)
        .map((pin) => pin.id);
      draft.pins = (draft.pins || []).filter((pin) => pin.pcfId !== id);
      draft.relays = (draft.relays || []).filter(
        (relay) => !removedPinIds.includes(relay.pinId),
      );
      const seen = new Set();
      rows.forEach((row) => {
        if (seen.has(row.pin)) return;
        seen.add(row.pin);
        const pinId = `pin-${id}-${row.pin}`;
        draft.pins.push({
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
          draft.relays.push({
            id: `relay-${pinId}`,
            name: row.name,
            room: name || "Extend Switch",
            icon: "bulb",
            pinId,
            activeLevel: 1,
            enabled: true,
            publicControl: true,
            startupPolicy: "off",
          });
        }
      });
      return {};
    },
    {
      label: "Gagal menyimpan Extend Switch",
      onSaved: (saved) => {
        delete form.dataset.editId;
        hideForm("extend");
        alert(
          saved && saved.rebootRequired
            ? "Extend Switch tersimpan. Reboot perangkat untuk menerapkan hardware."
            : "Extend Switch tersimpan.",
        );
      },
    },
  );
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
  if (!relayId) return alert("Pilih relay target.");
  const trigger = { type: triggerType };
  if (triggerType === "pin_change") {
    const pinId = data.get("triggerPinId");
    if (!pinId) return alert("Pilih input pin trigger.");
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
  return applyDraft(
    (draft) => {
      draft.automations = draft.automations || [];
      const index = draft.automations.findIndex((entry) => entry.id === id);
      if (index >= 0) draft.automations[index] = automation;
      else draft.automations.push(automation);
      return {};
    },
    {
      label: "Gagal menyimpan Action",
      onSaved: () => {
        delete form.dataset.editId;
        delete form.dataset.editName;
        hideForm("action");
      },
    },
  );
}

function saveDevice(event) {
  event.preventDefault();
  const form = new FormData(event.target);
  if (!state.config) return alert("Konfigurasi belum dimuat");
  const draft = cloneConfig(state.config);
  draft.device.name = form.get("deviceName");
  draft.device.id = form.get("deviceId");
  draft.device.timezone = form.get("timezone");
  draft.network.wifi.hostname = form.get("hostname");
  draft.network.wifi.ssid = form.get("ssid");
  if (form.get("password")) draft.network.wifi.password = form.get("password");
  draft.network.wifi.reconnectIntervalMs = Number(
    form.get("reconnectIntervalMs"),
  );
  draft.time = draft.time || {};
  draft.time.ds3231 = draft.time.ds3231 || {};
  draft.time.ds3231.address = parseInt(form.get("ds3231Address"));
  draft.access = draft.access || {};
  draft.access.anonymousControl = form.get("anonymousControl") === "on";
  validateConfig(draft)
    .then((validation) => {
      if (!validation.valid) throw Error(validation.error || "invalid");
      return configRequest("PUT", draft);
    })
    .then((saved) => {
      state.config = cloneConfig(draft);
      el("device-status").textContent =
        saved && saved.rebootRequired
          ? "Tersimpan; reboot diperlukan"
          : "Tersimpan";
      refreshConfig();
    })
    .catch((error) => {
      el("device-status").textContent = `Gagal: ${error.message}`;
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
