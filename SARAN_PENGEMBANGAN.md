# Saran Pengembangan ModuSwitch

Dokumen ini berisi rekomendasi berdasarkan peninjauan README, Web UI modular,
pipeline build Node.js, script upload, dan implementasi API. Ini merupakan
backlog usulan, bukan pernyataan bahwa seluruh firmware sudah diaudit atau
diuji pada perangkat fisik.

## Status implementasi

Sebagian besar butir P0/P1 dan sebagian P2 telah diterapkan:

- [x] 1. Batasi file yang dapat diakses melalui HTTP — `servePublicAsset` whitelist.
- [x] 2. Alur edit GPIO dan I/O PCF — edit mempertahankan ID pin; I/O PCF disimpan ke `pins`.
- [x] 3. Draft konfigurasi dan validasi — `/api/config/validate` menerima kandidat; Web UI memvalidasi sebelum `PUT`.
- [x] 4. Konfirmasi status relay — `loadDashboard` mengembalikan promise; pending dibersihkan setelah refresh/konfirmasi.
- [x] 5. Session dan hak akses dashboard — dashboard mengirim token; session hanya dibersihkan pada 401/403; perbandingan waktu wrap-safe.
- [x] 6. Kontrak automation — relay target dan input trigger dipilih dari konfigurasi; tipe trigger `pin_change`.
- [x] 7. Katalog pin mengikuti kemampuan perangkat — `board` dari `/api/status`.
- [x] 8. Fitur maintenance — reboot dan upload firmware terhubung ke API.
- [ ] 9. Lanjutkan pemisahan modul berdasarkan fitur — parsial, dapat dilanjutkan.
- [x] 10. Test dan build otomatis — `npm run test:web` + build kedua board.
- [ ] 11. Rilis dan dokumentasi operasional — belum dilakukan.

## Arah utama

Prioritaskan ketepatan konfigurasi, konsistensi state relay, dan pembatasan akses
sebelum menambah fitur. Struktur modular Web UI dan satu pipeline build Node.js
sudah menjadi fondasi yang baik. Pertahankan firmware ringan, loop non-blocking,
dan referensi hardware melalui `pinId`.

Prioritas:

- **P0:** selesaikan sebelum perangkat digunakan di luar lingkungan pengembangan.
- **P1:** perbaikan inti untuk iterasi berikutnya.
- **P2:** peningkatan pemeliharaan dan pengalaman pengguna.

## 1. Batasi file yang dapat diakses melalui HTTP — P0

**Temuan:** `src/ApiServer.cpp` mendaftarkan seluruh root LittleFS melalui
`serveStatic("/", LittleFS, "/")`. Filesystem juga menyimpan konfigurasi dan
backup. Redaksi password pada endpoint `/api/config` tidak melindungi file yang
disajikan langsung melalui static handler.

**Saran:**

- Gunakan daftar asset publik yang eksplisit: HTML, JS, CSS, dan katalog GPIO.
- Jangan sajikan `config.json`, `config.bak`, `state.json`, atau file sementara.
- Uji akses langsung ke nama file sensitif tanpa login.

**Kriteria selesai:** asset UI dapat dimuat, sedangkan file internal tidak dapat
diunduh melalui HTTP, baik sebelum maupun setelah login.

## 2. Perbaiki alur edit GPIO dan I/O PCF — P1

**Temuan:** di `web/src/config.js`, form edit GPIO mengisi `dataset.editId`,
tetapi penyimpanan masih menggunakan alur tambah dan menolak GPIO yang sudah
terdaftar. Editor I/O PCF menampilkan baris input, tetapi `saveExtend()` baru
menyimpan metadata PCF, belum memproses baris tersebut.

**Saran:**

- Bedakan operasi tambah dan edit secara eksplisit.
- Pertahankan ID pin saat edit supaya referensi relay/automation tetap valid.
- Validasi keunikan GPIO serta pasangan `pcfId` dan nomor pin.
- Simpan I/O PCF ke `pins`; relay output hanya menyimpan referensi `pinId`.
- Saat menghapus pin, jelaskan dan tangani seluruh referensi terkait.

**Kriteria selesai:** pengguna dapat menambah, mengedit, memuat ulang, dan
menghapus GPIO maupun I/O PCF tanpa duplikasi atau referensi yang tertinggal.

## 3. Gunakan draft konfigurasi dan validasi sebelum simpan — P1

**Temuan:** beberapa handler mengubah `state.config` sebelum request berhasil.
Jika request gagal, state lokal bisa berbeda dari konfigurasi perangkat.
Endpoint `/api/config/validate` saat ini memvalidasi konfigurasi yang sudah
tersimpan, bukan kandidat JSON yang dikirim pengguna.

**Saran:**

- Buat draft dari konfigurasi terakhir yang telah dikonfirmasi perangkat.
- Kirim draft ke validasi; jangan mutasi state terkonfirmasi lebih dahulu.
- Perbarui kontrak endpoint validasi agar menerima kandidat konfigurasi.
- Tampilkan kesalahan di field terkait dan pertahankan input ketika gagal.
- Pastikan nilai `[redacted]` tidak menggantikan secret asli saat penyimpanan.
- Pertahankan mekanisme validasi, backup, dan penyimpanan atomik firmware.

**Kriteria selesai:** request gagal tidak mengubah tampilan konfigurasi
terkonfirmasi; secret yang tidak diedit tetap sama; konfigurasi tidak valid
ditolak sebelum diterapkan ke hardware.

## 4. Pastikan status relay benar-benar menunggu konfirmasi — P1

**Temuan:** `toggleRelay()` menunggu `loadDashboard()`, tetapi fungsi tersebut
belum mengembalikan promise fetch dashboard. Status pending bisa dibersihkan
sebelum hasil refresh diterima.

**Saran:**

- Kembalikan promise dari operasi refresh agar dapat ditunggu caller.
- Hapus pending setelah konfirmasi state firmware atau setelah kegagalan yang
  ditangani, bukan sekadar setelah request refresh dimulai.
- Tambahkan timeout command dan resinkronisasi setelah WebSocket tersambung ulang.
- Bila beberapa command dapat berjalan bersamaan, gunakan `requestId` untuk
  mencocokkan konfirmasi.

**Kriteria selesai:** klik berulang saat pending tidak mengirim command ganda;
UI tidak menampilkan keberhasilan semu ketika jaringan terputus.

## 5. Rapikan session dan hak akses dashboard — P1

**Temuan:** request dashboard belum mengirim token admin. Kegagalan apa pun pada
fetch konfigurasi memicu penghapusan session dan reload. Firmware memakai SHA-1
untuk verifikasi password dan perbandingan waktu kedaluwarsa langsung terhadap
`millis()`.

**Saran:**

- Kirim session header untuk request dashboard admin agar relay privat dapat
  ditampilkan sesuai otorisasi firmware.
- Hapus session hanya pada respons autentikasi yang sesuai; error server atau
  jaringan harus menampilkan retry tanpa memaksa logout.
- Bersihkan cache konfigurasi admin saat logout.
- Tambahkan pembatasan percobaan login yang non-blocking.
- Evaluasi hash password bersalt dengan biaya komputasi yang sesuai ESP8266 dan
  token dari sumber acak yang sesuai platform; sediakan migrasi konfigurasi.
- Gunakan perhitungan interval yang aman terhadap wraparound `millis()`.
- Pastikan kontrol anonim selalu memerlukan `access.anonymousControl` sekaligus
  `publicControl` relay.

**Kriteria selesai:** test mencakup admin, anonim, token kedaluwarsa, kegagalan
server, serta batas waktu yang melewati wraparound timer.

## 6. Lengkapi kontrak automation — P1

**Temuan:** form automation memilih relay pertama atau fallback `relay-01`.
Beberapa field edit belum memuat ulang seluruh detail trigger yang tersimpan.

**Saran:**

- Sediakan pilihan relay target dan input trigger dari konfigurasi aktual.
- Hindari fallback ID yang belum tentu ada.
- Muat semua field trigger ketika edit, termasuk jadwal dan URL bila didukung.
- Cocokkan pilihan UI dengan tipe trigger/action yang benar-benar diterima
  validator dan automation engine.
- Jalankan request HTTP automation melalui antrean, bukan callback input.

**Kriteria selesai:** siklus buka–edit–simpan tidak menghilangkan field; seluruh
referensi automation valid; pemicu tidak memblokir loop perangkat.

## 7. Katalog pin mengikuti kemampuan perangkat — P1

**Temuan:** `loadGpioPins()` masih memilih board `esp8266_d1_mini` secara tetap,
sementara proyek juga menyediakan environment ESP32.

**Saran:**

- Ekspos identitas board dan kemampuan perangkat melalui endpoint status.
- Gunakan identitas tersebut untuk memilih katalog pin.
- Filter GPIO4/D2 dan GPIO5/D1 ketika I2C ESP8266 aktif, serta pin lain yang
  dipakai periferal berdasarkan konfigurasi aktual.
- Tetap lakukan validasi di firmware; pembatasan UI bukan satu-satunya pengaman.

**Kriteria selesai:** pilihan pin sesuai board dan konflik periferal ditolak
baik melalui UI maupun request API langsung.

## 8. Hubungkan fitur maintenance dengan API — P2

**Temuan:** firmware memiliki `/api/reboot` dan `/api/ota`, tetapi tombol Web UI
masih menampilkan pesan placeholder.

**Saran:**

- Hubungkan reboot ke endpoint terautentikasi dan tampilkan status reconnect.
- Implementasikan upload firmware dengan progress serta penanganan error.
- Tampilkan versi firmware, board, uptime, heap, sumber waktu, dan status Wi-Fi.
- Jangan memberi kesan bahwa scan Wi-Fi berfungsi sebelum endpoint tersedia.

**Kriteria selesai:** UI membedakan proses dimulai, berhasil, gagal, dan perangkat
sedang reboot; hasil dapat diverifikasi lewat status perangkat.

## 9. Lanjutkan pemisahan modul berdasarkan fitur — P2

**Saran:**

- Pecah `config.js` ketika berkembang menjadi modul GPIO, PCF, automation,
  device, dan auth.
- Pisahkan state/session, helper DOM, dan transport API bila tanggung jawabnya
  mulai sulit diikuti.
- Buat helper request untuk header, timeout, respons kosong seperti HTTP 204,
  dan pesan error yang konsisten.
- Pertahankan entry point kecil dan arah dependency tanpa circular import.
- Hindari menambah framework besar hanya untuk menggantikan beberapa form.

**Kriteria selesai:** perubahan satu form tidak membutuhkan perubahan pada form
lain, dan modul dapat diuji tanpa menjalankan seluruh UI.

## 10. Perkuat test dan build otomatis — P1

**Kondisi saat ini:** build web menggunakan Node.js/esbuild dan sudah memiliki
smoke test bundle. Test perangkat tersedia terpisah.

**Saran:**

- Jalankan pemeriksaan otomatis untuk setiap perubahan:

  ```sh
  npm ci
  npm run build
  node --check web/app.js
  npm run test:web
  pio run -e d1_mini
  pio run -e esp32dev
  ```

- Tambahkan test perilaku untuk edit GPIO, serialisasi I/O PCF, kegagalan simpan,
  session kedaluwarsa, dan konfirmasi relay terlambat.
- Uji build dengan konfigurasi contoh, konfigurasi lokal sementara, dan JSON
  tidak valid; pastikan build gagal sebelum menulis output yang tidak valid.
- Uji batas akses static files dan relay melalui API ketika perangkat online.
- Pantau ukuran bundle, penggunaan flash, dan heap bebas sebagai budget regresi.
- Pada `upload.bat`, evaluasi exit code dalam blok menggunakan mekanisme yang
  tidak terkena ekspansi `%errorlevel%` terlalu awal.

**Kriteria selesai:** kegagalan build/test/upload menghasilkan exit code gagal
yang benar dan pipeline tidak melanjutkan langkah berikutnya.

## 11. Rilis dan dokumentasi operasional — P2

**Saran:**

- Tetapkan versi firmware dan `configVersion` beserta migrasinya.
- Dokumentasikan kontrak API: method, autentikasi, body, respons, dan error.
- Tambahkan panduan backup/restore dan pemulihan konfigurasi gagal.
- Tentukan kebijakan upload filesystem terhadap konfigurasi perangkat yang
  sudah berubah melalui UI agar tidak tertimpa contoh atau konfigurasi lama.
- Simpan artefak rilis publik tanpa secret atau konfigurasi produksi.
- Tambahkan lisensi sebelum distribusi publik.
- Sinkronkan pernyataan README tentang file generated dengan aturan `.gitignore`
  dan file yang benar-benar dilacak.

**Kriteria selesai:** instalasi, upgrade, dan pemulihan dapat dilakukan dengan
mengikuti dokumentasi tanpa kehilangan konfigurasi pengguna secara tak sengaja.

## Urutan implementasi yang disarankan

| Tahap | Fokus | Hasil yang dituju |
| --- | --- | --- |
| 1 | Pembatasan static files | Konfigurasi internal tidak tersaji melalui HTTP |
| 2 | Edit GPIO/PCF dan draft konfigurasi | CRUD akurat, kegagalan tidak merusak state UI |
| 3 | Konfirmasi relay dan session | Status perangkat dan UI konsisten |
| 4 | Automation dan katalog board | Konfigurasi sesuai kemampuan hardware |
| 5 | Test otomatis dan build kedua board | Regresi terdeteksi sebelum upload |
| 6 | Maintenance, refactor lanjutan, dokumentasi rilis | Operasional dan pemeliharaan lebih mudah |

Mulai dari tahap 1–3. Ketiganya berdampak langsung pada pembatasan akses,
keutuhan konfigurasi, dan keandalan kontrol relay.
