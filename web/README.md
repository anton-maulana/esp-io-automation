# Web UI

Sumber JavaScript menggunakan ES modules:

- `app.js`: inisialisasi aplikasi.
- `src/core.js`: state, session, dan helper DOM.
- `src/api.js`: request API.
- `src/dashboard.js`: dashboard, kontrol relay, dan WebSocket.
- `src/config.js`: render dan penyimpanan konfigurasi serta login.
- `src/events.js`: navigasi dan interaksi halaman.

## Build

Install Node.js, lalu jalankan dari root proyek:

```sh
npm ci
npm run build
```

`tools/build_web.mjs` menggunakan esbuild untuk menyelesaikan import,
melakukan minifikasi yang memahami sintaks JavaScript, dan menghasilkan satu
bundle IIFE `data/app.js`. Modul sumber tidak perlu diunggah ke LittleFS.
Dependency build dikunci dalam `package-lock.json`.

`npm run build` menjadi entry point build untuk script upload Windows dan Linux/macOS.
Seluruh build web berjalan di Node.js: bundling JavaScript, minifikasi CSS/HTML/JSON,
dan penyalinan konfigurasi. CSS diminifikasi dengan parser esbuild.
Seluruh asset diproses sebelum output ditulis agar error bundling tidak
menghasilkan campuran asset lama dan baru. Konfigurasi menggunakan
`web/config.local.json` bila tersedia, selain itu `web/config.example.json`.

```sh
npm run format
node --check web/app.js
node --check data/app.js
npm run test:web
```

Edit sumber dalam `web/`; jangan edit hasil generated di `data/`.
