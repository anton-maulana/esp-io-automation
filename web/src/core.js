export const state = {
  relays: [],
  groups: {},
  pending: {},
  ws: null,
  admin: false,
  board: "esp8266_d1_mini",
  token: (() => {
    const cookie = document.cookie
      .split(";")
      .map((item) => item.trim())
      .find((item) => item.indexOf("modu_session=") === 0);
    if (!cookie) return "";
    try {
      return decodeURIComponent(cookie.slice("modu_session=".length));
    } catch {
      return "";
    }
  })(),
  config: null,
};

export const el = (id) => document.getElementById(id);

export function escapeHtml(value) {
  return String(value || "").replace(
    /[&<>"']/g,
    (character) =>
      ({
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;",
      })[character],
  );
}

export function setSession(token, expiresInMs = 3600000) {
  state.token = token;
  const maxAge = Math.max(1, Math.floor(expiresInMs / 1000));
  document.cookie = `modu_session=${encodeURIComponent(token)}; Max-Age=${maxAge}; Path=/; SameSite=Strict`;
}

export function clearSession() {
  state.token = "";
  state.admin = false;
  document.cookie = "modu_session=; Max-Age=0; Path=/; SameSite=Strict";
}

export function authHeaders(extra = {}) {
  const headers = { ...extra };
  if (state.token) headers["X-Session-Token"] = state.token;
  return headers;
}

export function cloneConfig(config) {
  return JSON.parse(JSON.stringify(config));
}

export function showForm(type) {
  el(`${type}-form`)?.classList.remove("hidden");
}

export function hideForm(type) {
  el(`${type}-form`)?.classList.add("hidden");
}

export function showView(view) {
  document
    .querySelectorAll(".view")
    .forEach((item) => item.classList.add("hidden"));
  el(`${view}-view`).classList.remove("hidden");
}
