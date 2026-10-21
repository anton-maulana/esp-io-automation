export const state = {
  relays: [],
  groups: {},
  pending: {},
  ws: null,
  admin: false,
  board: "esp8266_d1_mini",
  token:
    document.cookie
      .split("; ")
      .find((item) => item.indexOf("modu_session=") === 0)
      ?.split("=")[1] || "",
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

export function setSession(token) {
  state.token = token;
  document.cookie = `modu_session=${encodeURIComponent(token)}; Max-Age=3600; Path=/; SameSite=Strict`;
}

export function clearSession() {
  state.token = "";
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
  el("admin-drawer").classList.remove("open");
}
