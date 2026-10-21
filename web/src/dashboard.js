import { state, el, escapeHtml, authHeaders } from "./core.js";
import { loadTime } from "./api.js";

export function loadGpioPins() {
  const status = fetch("/api/status")
    .then((response) => response.json())
    .catch(() => ({ board: "esp8266_d1_mini" }));
  const catalog = fetch("/gpio-pins.json").then((response) => response.json());
  Promise.all([status, catalog])
    .then(([deviceStatus, pinCatalog]) => {
      state.board = deviceStatus.board || "esp8266_d1_mini";
      const board = pinCatalog[state.board] || pinCatalog.esp8266_d1_mini;
      const select = el("gpio-pin-select");
      select.innerHTML = "";
      (board.pins || []).forEach((pin) => {
        const option = document.createElement("option");
        option.value = pin.gpio;
        option.textContent = pin.label;
        select.appendChild(option);
      });
      select.dataset.pinCatalog = state.board;
    })
    .catch(() => {
      el("gpio-pin-select").innerHTML =
        '<option value="">Daftar pin gagal dimuat</option>';
    });
}

export function renderDashboard() {
  const groups = {};
  state.relays.forEach((relay) =>
    (groups[relay.group || "GPIO Switch"] ||= []).push(relay),
  );
  state.groups = groups;
  const root = el("switch-groups");
  root.innerHTML = "";
  Object.entries(groups).forEach(([name, relays]) => {
    const section = document.createElement("section");
    section.className = "switch-group";
    section.innerHTML = `<h3>${escapeHtml(name)}</h3><div class="switch-grid"></div>`;
    const grid = section.querySelector(".switch-grid");
    relays.forEach((relay) => {
      const pending = state.pending[relay.id];
      const active = relay.state;
      const io =
        relay.source === "gpio"
          ? `GPIO ${relay.gpio}`
          : `${relay.pcfId || "Extend Switch"} · P${relay.pin}`;
      const card = document.createElement("article");
      card.className = `switch-card ${active ? "on " : ""}${pending ? "pending" : ""}`;
      card.innerHTML = `<div class="switch-main"><div class="switch-icon ${active ? "active" : ""}">${active ? "☀" : "○"}</div><div><div class="switch-name">${escapeHtml(relay.name)}</div><div class="switch-room">${escapeHtml(io)} · ${escapeHtml(relay.room || "Switch")}</div></div></div><button class="toggle ${active ? "active" : ""}" ${pending ? "disabled" : ""}><span class="toggle-dot"></span>${pending ? "Processing..." : active ? "ON" : "OFF"}</button>`;
      card.querySelector("button").onclick = () => toggleRelay(relay);
      grid.appendChild(card);
    });
    root.appendChild(section);
  });
  const active = state.relays.filter((relay) => relay.state).length;
  el("summary").textContent =
    `${state.relays.length} action tersedia · ${active} sedang menyala`;
}

export function loadDashboard() {
  const dashboard = fetch("/api/public/dashboard", { headers: authHeaders() })
    .then((response) => response.json())
    .then((data) => {
      el("device-name").textContent = data.deviceName || "ModuSwitch";
      state.relays = data.relays || [];
      renderDashboard();
      el("sync-status").textContent = "Tersinkron";
    })
    .catch(() => {
      el("sync-status").textContent = "Tidak dapat memuat";
    });
  loadTime();
  return dashboard;
}

function toggleRelay(relay) {
  if (state.pending[relay.id]) return;
  state.pending[relay.id] = true;
  renderDashboard();
  fetch(`/api/relay/toggle?id=${encodeURIComponent(relay.id)}`, {
    method: "POST",
    headers: authHeaders(),
  })
    .then((response) => {
      if (!response.ok) throw Error("toggle_failed");
      return loadDashboard();
    })
    .catch(() => alert("Switch command failed"))
    .finally(() => {
      delete state.pending[relay.id];
      renderDashboard();
    });
}

export function connectWebSocket() {
  const url = `${location.protocol === "https:" ? "wss://" : "ws://"}${location.host}/ws`;
  state.ws = new WebSocket(url);
  state.ws.onopen = () => {
    el("connection").textContent = "Online";
    el("connection").classList.add("online");
  };
  state.ws.onclose = () => {
    el("connection").textContent = "Menyambungkan ulang";
    el("connection").classList.remove("online");
    setTimeout(connectWebSocket, 2500);
  };
  state.ws.onmessage = (event) => {
    try {
      const message = JSON.parse(event.data);
      if (message.type !== "relay.state") return;
      const relay = state.relays.find((item) => item.id === message.relayId);
      if (relay) {
        relay.state = message.actualState;
        delete state.pending[message.relayId];
        renderDashboard();
      }
    } catch (_) {}
  };
}
