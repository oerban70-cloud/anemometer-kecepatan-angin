// AnemometerKecepatanAngin.ino — Pengukur kecepatan angin berbasis ESP32.
//
// Cara kerja:
//   1. Anemometer mangkuk (3 cup) berputar tertiup angin. Setiap satu
//      putaran penuh, magnet kecil di poros melewati reed switch dan
//      mengirim SATU pulsa ke ESP32.
//   2. Pulsa dihitung lewat interupsi di GPIO4 (debounce 5 ms supaya
//      pantulan sakelar tidak terhitung dua kali).
//   3. Setiap JENDELA_HITUNG_MS (3 detik), ESP32 menghitung frekuensi
//      pulsa lalu mengubahnya jadi kecepatan angin:
//         km/jam = frekuensi_pulsa_Hz x KALIBRASI_KMH_PER_HZ
//      Faktor kalibrasi 2,4 km/jam per 1 Hz adalah nilai umum anemometer
//      mangkuk (gaya Davis) — kalibrasi ulang terhadap anemometer
//      referensi bila mangkukmu berdiameter berbeda, caranya ada di README.
//   4. LCD menampilkan kecepatan angin saat ini dan hembusan terkuat
//      (gust) sejak alat dinyalakan atau sejak tombol reset ditekan.
//   5. Bila kecepatan angin mencapai AMBANG_ALARM_KMH (40 km/jam,
//      angin kencang skala Beaufort 6), buzzer berbunyi, LED merah
//      menyala, dan LED hijau padam. Di bawah ambang itu LED hijau menyala.
//
// Pinout (HARUS sama dengan skema di schematic.png):
//   GPIO4  <- pulsa reed switch anemometer (interupsi, aktif LOW)
//   GPIO32 <- tombol reset hembusan maks (INPUT_PULLUP, aktif LOW)
//   GPIO21 -> LCD I2C SDA, GPIO22 -> LCD I2C SCL (alamat 0x27)
//   GPIO26 -> buzzer aktif HIGH
//   GPIO14 -> LED merah (angin kencang) lewat resistor 220 ohm ke GND
//   GPIO12 -> LED hijau (angin normal) lewat resistor 220 ohm ke GND
//
// Library yang dibutuhkan (lihat folder libraries/ di repo):
//   - LiquidCrystal_I2C 1.1.2
//
// Catu daya: ESP32 + LCD + buzzer dari USB 5V. Anemometer reed switch
// tidak butuh catu sendiri — ia hanya sakelar magnet yang menutup ke GND.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------------------- pin --------------------
const uint8_t PIN_PULSA_ANEMOMETER = 4;   // interupsi, aktif LOW
const uint8_t PIN_TOMBOL_RESET     = 32;  // INPUT_PULLUP, aktif LOW
const uint8_t PIN_BUZZER           = 26;
const uint8_t PIN_LED_MERAH        = 14;
const uint8_t PIN_LED_HIJAU        = 12;
const uint8_t PIN_SDA_LCD          = 21;
const uint8_t PIN_SCL_LCD          = 22;

// -------------------- konfigurasi --------------------
const float         KALIBRASI_KMH_PER_HZ = 2.4f;   // 1 Hz pulsa = 2,4 km/jam
const unsigned long JENDELA_HITUNG_MS    = 3000;   // hitung pulsa tiap 3 detik
const unsigned long DEBOUNCE_PULSA_MS    = 5;      // anti pantulan reed switch
const unsigned long DEBOUNCE_TOMBOL_MS   = 50;     // anti pantulan tombol
const float         AMBANG_ALARM_KMH     = 40.0f;  // angin kencang (Beaufort 6)
const uint8_t       ALAMAT_LCD           = 0x27;
const uint8_t       KOLOM_LCD            = 16;
const uint8_t       BARIS_LCD            = 2;

// -------------------- objek --------------------
LiquidCrystal_I2C lcd(ALAMAT_LCD, KOLOM_LCD, BARIS_LCD);

// -------------------- state --------------------
// Dihitung di ISR — wajib volatile.
volatile unsigned long jumlahPulsa = 0;
volatile unsigned long waktuPulsaTerakhirMs = 0;

float kecepatanKmh = 0.0f;       // kecepatan angin jendela terakhir
float hembusanMaksKmh = 0.0f;    // hembusan terkuat sejak reset
unsigned long waktuJendelaMulaiMs = 0;
unsigned long pulsaJendelaMulai = 0;
bool alarmAktif = false;

// Debounce tombol reset (non-blocking).
bool keadaanTombolTerakhir = HIGH;
unsigned long waktuTombolBerubahMs = 0;

// -------------------- interupsi pulsa anemometer --------------------
void IRAM_ATTR saatPulsaAnemometer() {
  unsigned long sekarang = millis();
  if (sekarang - waktuPulsaTerakhirMs >= DEBOUNCE_PULSA_MS) {
    jumlahPulsa++;
    waktuPulsaTerakhirMs = sekarang;
  }
}

// -------------------- kategori angin (skala Beaufort, disederhanakan) --------------------
const char *kategoriAngin(float kmh) {
  if (kmh < 1.0f)   return "Tenang";
  if (kmh < 6.0f)   return "Sepoi";
  if (kmh < 12.0f)  return "Ringan";
  if (kmh < 20.0f)  return "Sedang";
  if (kmh < 29.0f)  return "Kencang";
  if (kmh < 39.0f)  return "Sgt kencang";
  if (kmh < 50.0f)  return "Badai kecil";
  return "Badai";
}

// -------------------- buzzer --------------------
void bunyiBuzzer(unsigned int durasiMs) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(durasiMs);
  digitalWrite(PIN_BUZZER, LOW);
}

// -------------------- LCD --------------------
void tampilkanLcd() {
  char baris1[17];
  char baris2[17];
  snprintf(baris1, sizeof(baris1), "Angin: %.1f km/j", kecepatanKmh);
  snprintf(baris2, sizeof(baris2), "Maks: %.1f km/j", hembusanMaksKmh);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(baris1);
  lcd.setCursor(0, 1);
  lcd.print(baris2);
}

// -------------------- alarm angin kencang --------------------
void perbaruiAlarm() {
  bool kencang = kecepatanKmh >= AMBANG_ALARM_KMH;
  digitalWrite(PIN_LED_MERAH, kencang ? HIGH : LOW);
  digitalWrite(PIN_LED_HIJAU, kencang ? LOW : HIGH);
  if (kencang && !alarmAktif) {
    // Baru melewati ambang: bunyi sekali sebagai peringatan.
    bunyiBuzzer(300);
    Serial.println("PERINGATAN: angin kencang!");
  }
  alarmAktif = kencang;
}

// -------------------- tombol reset hembusan maks --------------------
// Mengembalikan true tepat sekali saat tombol ditekan (transisi HIGH->LOW).
bool tombolResetDitekan() {
  bool keadaan = digitalRead(PIN_TOMBOL_RESET);
  unsigned long sekarang = millis();
  if (keadaan != keadaanTombolTerakhir) {
    waktuTombolBerubahMs = sekarang;
    keadaanTombolTerakhir = keadaan;
  }
  if (sekarang - waktuTombolBerubahMs >= DEBOUNCE_TOMBOL_MS
      && keadaan == LOW) {
    // Tunggu tombol dilepas supaya satu tekanan = satu reset.
    while (digitalRead(PIN_TOMBOL_RESET) == LOW) {
      delay(10);
    }
    return true;
  }
  return false;
}

// -------------------- setup / loop --------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_PULSA_ANEMOMETER, INPUT_PULLUP);
  pinMode(PIN_TOMBOL_RESET, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_MERAH, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_MERAH, LOW);
  digitalWrite(PIN_LED_HIJAU, LOW);

  attachInterrupt(digitalPinToInterrupt(PIN_PULSA_ANEMOMETER),
                  saatPulsaAnemometer, FALLING);

  Wire.begin(PIN_SDA_LCD, PIN_SCL_LCD);
  lcd.init();
  lcd.backlight();
  tampilkanLcd();

  waktuJendelaMulaiMs = millis();
  pulsaJendelaMulai = jumlahPulsa;

  Serial.println("Anemometer siap. Menunggu putaran mangkuk...");
}

void loop() {
  unsigned long sekarang = millis();

  // 1. Tombol reset: nol-kan hembusan terkuat.
  if (tombolResetDitekan()) {
    hembusanMaksKmh = 0.0f;
    tampilkanLcd();
    Serial.println("Hembusan maks direset ke 0.");
  }

  // 2. Jendela hitung selesai -> hitung kecepatan angin.
  if (sekarang - waktuJendelaMulaiMs >= JENDELA_HITUNG_MS) {
    unsigned long pulsaBaru =
        jumlahPulsa - pulsaJendelaMulai;
    float frekuensiHz =
        (float)pulsaBaru / (JENDELA_HITUNG_MS / 1000.0f);
    kecepatanKmh = frekuensiHz * KALIBRASI_KMH_PER_HZ;

    if (kecepatanKmh > hembusanMaksKmh) {
      hembusanMaksKmh = kecepatanKmh;
    }

    float kecepatanMs = kecepatanKmh / 3.6f;
    Serial.print("Angin: ");
    Serial.print(kecepatanKmh, 1);
    Serial.print(" km/j (");
    Serial.print(kecepatanMs, 1);
    Serial.print(" m/s) - ");
    Serial.print(kategoriAngin(kecepatanKmh));
    Serial.print(", maks ");
    Serial.print(hembusanMaksKmh, 1);
    Serial.println(" km/j");

    tampilkanLcd();
    perbaruiAlarm();

    waktuJendelaMulaiMs = sekarang;
    pulsaJendelaMulai = jumlahPulsa;
  }
}
