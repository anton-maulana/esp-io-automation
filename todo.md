# ModuSwitch Todo

## Admin UI

- [x] Dashboard dengan kelompok switch dan status aktual.
- [x] Menu Config: GPIO Switch, Extend Switch, dan Action.
- [x] Form GPIO: pin, mode, nama, dan kondisi awal.
- [x] Form PCF/I2C: alamat switch, nama, dan I/O pin 0–7.
- [x] Form Action: target, kondisi, trigger input/jadwal/request, serta aksi relay.
- [x] Form Device: identitas, timezone, Wi-Fi, DS3231, dan reconnect interval.
- [x] Konfirmasi UI untuk reboot dan firmware maintenance.
- [ ] Ganti alert rancangan dengan notifikasi non-blocking.
- [ ] Muat nilai form melalui API konfigurasi, bukan nilai contoh.
- [ ] Tampilkan validation error per field dari firmware.

## API konfigurasi admin

- [ ] `GET /api/config` hanya untuk sesi admin dan menyamarkan password Wi-Fi.
- [ ] `PUT /api/config/device` untuk device, timezone, dan hostname.
- [ ] `PUT /api/config/network` untuk Wi-Fi serta reconnect interval.
- [ ] `POST /api/config/gpio` dan endpoint edit/hapus GPIO.
- [ ] `POST /api/config/pcf` dan endpoint edit/hapus PCF/I2C switch.
- [ ] `POST /api/config/action` dan endpoint edit/hapus automation.
- [ ] Validasi semua referensi memakai `pinId`; relay/action tidak menyimpan alamat fisik duplikat.
- [ ] Validasi GPIO bootstrap ESP8266 sebelum diterapkan.
- [ ] Simpan konfigurasi setelah validasi, backup, dan atomic restore bila gagal.
- [ ] Terapkan Wi-Fi baru non-blocking serta beri status reconnect ke UI.

## Maintenance dan diagnostik

- [ ] Endpoint Wi-Fi scan yang dibatasi sesi admin.
- [ ] Endpoint reboot dengan konfirmasi request ID.
- [ ] Endpoint upload firmware yang aman dan status progres.
- [ ] Deteksi DS3231 pada alamat I2C yang dikonfigurasi.
- [ ] Halaman diagnostics: heap, uptime, RSSI, IP, I2C scan, dan alasan disconnect Wi-Fi.

## Keamanan dan verifikasi

- [x] Password admin disimpan sebagai hash, bukan teks biasa.
- [ ] Gunakan hash password yang lebih kuat dari SHA-1 pada migrasi berikutnya.
- [ ] Jangan respons password Wi-Fi, token, atau kredensial ke UI/log.
- [ ] Uji request konfigurasi tanpa token harus ditolak.
- [ ] Uji konfigurasi invalid tidak diterapkan ke hardware.
- [ ] Jalankan `pio run -e d1_mini` dan `pio test -e d1_mini` sebelum rilis.
