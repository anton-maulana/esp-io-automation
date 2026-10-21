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
        window.location.reload();
        return new Promise(() => {});
      }
      if (!response.ok) throw Error("config_load_failed");
      return response.json();
    })
    .then((config) => {
      state.config = config;
      return config;
    });
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

export function loadTime() {
  return fetch("/api/time")
    .then((response) => response.json())
    .then((time) => {
      el("clock-status").textContent =
        `Waktu ${(time.source || "tidak diketahui").toUpperCase()}: ${String(time.iso || "belum tersedia").trim()}`;
    })
    .catch(() => {
      el("clock-status").textContent = "Waktu tidak tersedia";
    });
}
