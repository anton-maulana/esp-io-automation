import {
  loadGpioPins,
  loadDashboard,
  connectWebSocket,
} from "./src/dashboard.js";
import { syncLoginUI, bindConfigEvents } from "./src/config.js";
import { bindNavigation } from "./src/events.js";

export function init() {
  bindNavigation();
  bindConfigEvents();
  loadGpioPins();
  loadDashboard();
  connectWebSocket();
  syncLoginUI();
}

init();
