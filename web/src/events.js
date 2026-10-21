import { el, showForm, hideForm, showView } from "./core.js";
import { loadConfig, logout, addIo } from "./config.js";
import { loadDashboard } from "./dashboard.js";
import { backupConfig, rebootDevice, restoreConfig, uploadFirmware } from "./api.js";

export function bindNavigation() {
  el("login-button").onclick = () =>
    el("login-modal").classList.remove("hidden");
  el("close-login").onclick = () => el("login-modal").classList.add("hidden");
  el("menu-button").onclick = () => el("admin-drawer").classList.add("open");
  const closeMenu = document.getElementById("close-menu");
  if (closeMenu && typeof closeMenu.addEventListener === "function") {
    closeMenu.addEventListener("click", () =>
      el("admin-drawer").classList.remove("open"),
    );
  }
  document.querySelectorAll("[data-view]").forEach(
    (button) =>
      (button.onclick = () => {
        showView(button.dataset.view);
        if (button.dataset.view === "dashboard") loadDashboard();
        else loadConfig().catch(() => {});
      }),
  );
  document.querySelectorAll("[data-add]").forEach(
    (button) =>
      (button.onclick = () => {
        showForm(button.dataset.add);
        if (
          button.dataset.add === "extend" &&
          !el("extend-io-list").children.length
        )
          addIo();
      }),
  );
  el("add-extend-io").onclick = () => addIo();
  document
    .querySelectorAll("[data-cancel]")
    .forEach(
      (button) => (button.onclick = () => hideForm(button.dataset.cancel)),
    );
  el("logout-button").onclick = () => {
    logout();
    showView("dashboard");
    loadDashboard();
  };
  el("scan-wifi").onclick = () =>
    alert("Wi-Fi scanning will be available through the configuration endpoint.");
  el("backup-config").onclick = () => {
    backupConfig()
      .then((blob) => {
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = "moduswitch-config-backup.json";
        link.click();
        URL.revokeObjectURL(url);
      })
      .catch((error) => alert(`Backup failed: ${error.message}`));
  };
  const restoreInput = el("restore-config-file");
  el("restore-config").onclick = () => restoreInput.click();
  restoreInput.onchange = () => {
    const file = restoreInput.files[0];
    if (!file || !confirm("Restore configuration from this file?")) return;
    file.text()
      .then((content) => restoreConfig(content))
      .then(() => window.location.reload())
      .catch((error) => alert(`Restore failed: ${error.message}`));
  };

  const firmwareInput = el("firmware-file");
  el("upload-firmware").onclick = () => firmwareInput.click();
  firmwareInput.onchange = () => {
    const file = firmwareInput.files[0];
    if (!file) return;
    if (!confirm(`Upload firmware ${file.name}?`)) {
      firmwareInput.value = "";
      return;
    }
    el("device-status").textContent = "Uploading firmware...";
    uploadFirmware(file)
      .then(() => {
        el("device-status").textContent = "Firmware uploaded; rebooting soon.";
      })
      .catch((error) => {
        el("device-status").textContent = `Upload failed: ${error.message}`;
      })
      .finally(() => {
        firmwareInput.value = "";
      });
  };

  el("reboot-device").onclick = () => {
    if (!confirm("Reboot the device now?")) return;
    el("device-status").textContent = "Rebooting...";
    rebootDevice()
      .then(() => {
        el("device-status").textContent = "Reboot started; reconnecting...";
      })
      .catch((error) => {
        el("device-status").textContent = `Reboot failed: ${error.message}`;
      });
  };
}
