import { state, clearSession, el, authHeaders } from "./core.js";

export function configRequest(method, body) {
  return fetch("/api/config", {
    method,
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: body ? JSON.stringify(body) : undefined,
  }).then((response) =>
    response.json().then((data) => {
      if (!response.ok) throw Error(data.error || "config_error");
      return data;
    }),
  );
}

export function resourceRequest(path, method, body) {
  return fetch(path, {
    method,
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: body ? JSON.stringify(body) : undefined,
  }).then((response) =>
    response.json().then((data) => {
      if (!response.ok) throw Error(data.error || "resource_error");
      return data;
    }),
  );
}

export function deleteResource(path, id) {
  return fetch(`${path}?id=${encodeURIComponent(id)}`, {
    method: "DELETE",
    headers: authHeaders(),
  }).then((response) =>
    response.json().then((data) => {
      if (!response.ok) throw Error(data.error || "delete_failed");
      return data;
    }),
  );
}

export function patchConfig(changes) {
  return fetch("/api/config/patch", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({ changes }),
  }).then((response) =>
    response.json().then((data) => {
      if (!response.ok) throw Error(data.error || "config_patch_failed");
      return data;
    }),
  );
}

export function validateConfig(config) {
  return fetch("/api/config/validate", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify(config),
  }).then((response) =>
    response.json().then((data) => {
      if (!response.ok) throw Error(data.error || "validation_error");
      return data;
    }),
  );
}

export function fetchConfig() {
  return fetch("/api/config", { headers: authHeaders() })
    .then((response) => {
      if (response.status === 401 || response.status === 403) {
        clearSession();
        throw Error("session_expired");
      }
      if (!response.ok) throw Error("config_load_failed");
      return response.json();
    })
    .then((config) => {
      state.config = config;
      return config;
    });
}

export function backupConfig() {
  return fetch("/api/config/backup", { headers: authHeaders() }).then((response) => {
    if (!response.ok) throw Error("backup_failed");
    return response.blob();
  });
}

export function restoreConfig(content) {
  return fetch("/api/config/restore", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: content,
  }).then((response) =>
    response.json().then((data) => {
      if (!response.ok) throw Error(data.error || "restore_failed");
      return data;
    }),
  );
}

export function updatePassword(currentPassword, newPassword) {
  return fetch("/api/auth/password", {
    method: "POST",
    headers: { "Content-Type": "application/json", ...authHeaders() },
    body: JSON.stringify({ currentPassword, newPassword }),
  }).then((response) =>
    response.json().then((data) => {
      if (!response.ok) throw Error(data.error || "password_update_failed");
      return data;
    }),
  );
}

export function login(username, password) {
  return fetch("/api/auth/login", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ username, password }),
  }).then((response) => {
    if (!response.ok) throw Error("login_failed");
    return response.json();
  });
}

export function fetchStatus() {
  return fetch("/api/status").then((response) => response.json());
}

export function rebootDevice() {
  return fetch("/api/reboot", { method: "POST", headers: authHeaders() }).then(
    (response) => {
      if (!response.ok) throw Error("reboot_failed");
    },
  );
}

export function uploadFirmware(file) {
  return fetch("/api/ota", {
    method: "POST",
    headers: authHeaders(),
    body: file,
  }).then((response) => {
    if (!response.ok) throw Error("firmware_upload_failed");
  });
}

let clockEpochMs = 0;
let clockUpdatedAtMs = 0;
let clockSource = "";

function renderClock() {
  if (clockEpochMs < 946684800000) {
    el("clock-status").textContent = "Time not synchronized";
    return;
  }
  const current = new Date(clockEpochMs + Date.now() - clockUpdatedAtMs);
  el("clock-status").textContent = `Time ${clockSource.toUpperCase()}: ${current.toLocaleString("id-ID")}`;
}

if (typeof setInterval === "function") setInterval(renderClock, 1000);

export function loadTime() {
  return fetch("/api/time")
    .then((response) => {
      if (!response.ok) throw Error("time_load_failed");
      return response.json();
    })
    .then((time) => {
      clockEpochMs = Number(time.epoch) * 1000;
      clockUpdatedAtMs = Date.now();
      clockSource = time.source || "unknown";
      renderClock();
    })
    .catch(() => {
      el("clock-status").textContent = "Time unavailable";
    });
}
