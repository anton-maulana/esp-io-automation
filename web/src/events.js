import { el, showForm, hideForm, showView } from "./core.js";
import { loadConfig, logout, addIo } from "./config.js";
import { loadDashboard } from "./dashboard.js";
import { rebootDevice, uploadFirmware } from "./api.js";

export function bindNavigation() {
  el("login-button").onclick = () =>
    el("login-modal").classList.remove("hidden");
  el("close-login").onclick = () => el("login-modal").classList.add("hidden");
  el("menu-button").onclick = () => el("admin-drawer").classList.add("open");
  el("close-menu").onclick = () => el("admin-drawer").classList.remove("open");
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
    alert("Wi-Fi scan akan tersedia pada endpoint konfigurasi.");

  const firmwareInput = el("firmware-file");
  el("upload-firmware").onclick = () => firmwareInput.click();
  firmwareInput.onchange = () => {
    const file = firmwareInput.files[0];
    if (!file) return;
    if (!confirm(`Upload firmware ${file.name}?`)) {
      firmwareInput.value = "";
      return;
    }
    el("device-status").textContent = "Mengunggah firmware...";
    uploadFirmware(file)
      .then(() => {
        el("device-status").textContent = "Firmware diunggah; reboot segera.";
      })
      .catch((error) => {
        el("device-status").textContent = `Gagal upload: ${error.message}`;
      })
      .finally(() => {
        firmwareInput.value = "";
      });
  };

  el("reboot-device").onclick = () => {
    if (!confirm("Reboot perangkat sekarang?")) return;
    el("device-status").textContent = "Me-reboot...";
    rebootDevice()
      .then(() => {
        el("device-status").textContent = "Reboot dimulai; menyambung ulang...";
      })
      .catch((error) => {
        el("device-status").textContent = `Gagal reboot: ${error.message}`;
      });
  };
}
