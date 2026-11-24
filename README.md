# Pengukur Kecepatan Angin Anemometer — ESP32 + Reed Switch + LCD I2C

Angin kencang tidak terlihat, tapi alat ini mengukurnya. Tiga mangkuk
anemometer berputar tertiup angin; setiap satu putaran penuh, magnet di
poros melewati reed switch dan mengirim satu pulsa ke ESP32. ESP32
menghitung pulsa lewat interupsi dan mengubahnya jadi kecepatan angin
dalam km/jam, ditampilkan di LCD bersama hembusan terkuat yang pernah
tercatat. Begitu angin mencapai 40 km/jam, buzzer berbunyi dan LED merah
menyala sebagai peringatan.

Video penjelasan (60 detik):
link menyusul setelah upload

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Anemometer mangkuk 3 cup + reed switch | 1 pulsa per putaran, sinyal pulsa ke GPIO4 |
| LCD 16x2 I2C (alamat 0x27) | Menampilkan kecepatan angin + hembusan terkuat |
| Buzzer aktif | Berbunyi saat angin mencapai 40 km/jam |
| LED merah + resistor 220 ohm | Menyala saat angin kencang (≥ 40 km/jam) |
| LED hijau + resistor 220 ohm | Menyala saat angin normal (di bawah 40 km/jam) |
| Tombol reset | Menolkan hembusan terkuat yang tercatat |
| Catu USB 5V | Adaptor atau port laptop |

## Wiring

| Dari | Ke |
|---|---|
| Anemometer SIG | ESP32 GPIO4 (interupsi, aktif LOW, 1 pulsa = 1 putaran) |
| Anemometer VCC | 5V |
| Anemometer GND | GND |
| Tombol reset (kaki 1) | GND |
| Tombol reset (kaki 2) | ESP32 GPIO32 (INPUT_PULLUP) |
| LCD SDA | ESP32 GPIO21 |
| LCD SCL | ESP32 GPIO22 |
| LCD VCC | 5V |
| LCD GND | GND |
| Buzzer + | ESP32 GPIO26 (aktif HIGH) |
| Buzzer - | GND |
| LED merah anoda | ESP32 GPIO14, lewat resistor 220 ohm ke GND |
| LED hijau anoda | ESP32 GPIO12, lewat resistor 220 ohm ke GND |
| USB 5V | ESP32 VIN (lewat konektor USB board) |
| Semua GND | Digabung (anemometer, tombol, LCD, buzzer, LED, USB -) |

Skema lengkap: [schematic.png](schematic.png)

Reed switch tidak butuh catu sendiri — ia hanya sakelar magnet yang
menutup ke GND setiap kali magnet di poros melewatinya.

## Cara kerja

Anemometer mangkuk berputar karena angin. Setiap satu putaran penuh,
reed switch menutup sekali dan mengirim satu pulsa ke GPIO4. Pulsa itu
ditangkap ESP32 lewat interupsi (debounce 5 ms supaya pantulan sakelar
tidak terhitung dua kali).

Setiap 3 detik, sketch menghitung frekuensi pulsa (pulsa per detik)
lalu mengubahnya jadi kecepatan angin dengan rumus:

```
kecepatan (km/jam) = frekuensi pulsa (Hz) x 2,4
```

Faktor 2,4 km/jam per 1 Hz adalah nilai umum anemometer mangkuk
(gaya Davis). LCD baris pertama menampilkan kecepatan angin saat ini,
baris kedua menampilkan hembusan terkuat sejak alat dinyalakan atau
sejak tombol reset ditekan. Kategori angin (Tenang, Sepoi, Ringan,
Sedang, Kencang, Sangat kencang, Badai kecil, Badai — mengikuti skala
Beaufort yang disederhanakan) dicetak ke Serial Monitor.

Begitu kecepatan angin mencapai 40 km/jam (angin kencang, Beaufort 6),
buzzer berbunyi sekali sebagai peringatan, LED merah menyala, dan LED
hijau padam. Di bawah ambang itu LED hijau menyala dan LED merah padam.
Tombol reset menolkan hembusan terkuat — tekan sekali, nilainya kembali
ke 0 dan mulai mencatat lagi dari awal.

## Konfigurasi (di awal sketch)

- `KALIBRASI_KMH_PER_HZ` (2.4) — kilometer per jam untuk tiap 1 Hz pulsa.
- `JENDELA_HITUNG_MS` (3000) — kecepatan dihitung ulang tiap 3 detik.
- `DEBOUNCE_PULSA_MS` (5) — jeda anti pantulan reed switch.
- `DEBOUNCE_TOMBOL_MS` (50) — jeda anti pantulan tombol reset.
- `AMBANG_ALARM_KMH` (40.0) — ambang peringatan angin kencang.
- `ALAMAT_LCD` (0x27) — alamat I2C LCD 16x2.

Kalibrasi ulang bila mangkukmu berdiameter berbeda: ukur kecepatan
angin dengan anemometer referensi (atau aplikasi cuaca tepercaya) di
saat yang sama, catat frekuensi pulsa dari Serial Monitor, lalu hitung
`KALIBRASI_KMH_PER_HZ` = kecepatan referensi (km/jam) dibagi frekuensi
(Hz). Pantau lewat Serial Monitor (115200 baud): setiap 3 detik
kecepatan angin, kategori, dan hembusan terkuat dicetak.

## Library yang dibutuhkan

- LiquidCrystal_I2C 1.1.2 (LCD 16x2 via backpack PCF8574).

Library ada di folder `libraries/` (file ZIP, folder library di
top-level zip) — pasang lewat Arduino IDE (Sketch > Include Library >
Add .ZIP Library) atau salin ke folder libraries Arduino.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 AnemometerKecepatanAngin
```

Terverifikasi 24 Nov 2026: sketch 319.449 byte (24% flash),
RAM 25.128 byte (7%), ESP32 core 4.0.0, arduino-cli 1.6.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
