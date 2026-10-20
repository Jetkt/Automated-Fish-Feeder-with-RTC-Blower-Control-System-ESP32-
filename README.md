# 🐟 Automated Fish Feeder with RTC DS3231 & Blower Control System (ESP32)

Sistem pakan ikan otomatis berbasis mikrokontroler **ESP32 DevKit V1** yang terintegrasi dengan modul **RTC DS3231** dan **LCD I2C 16x2**. Proyek ini dikembangkan sebagai bagian dari program **Pengabdian Masyarakat** untuk membantu peternak/pembudidaya ikan mengotomatisasi jadwal pakan dan lontaran pakan secara presisi, terukur, dan andal.

---

## 📌 Deskripsi Proyek

Pemberian pakan ikan secara teratur dan konsisten sangat krusial dalam budidaya perikanan. Proyek ini menghadirkan solusi otomasi pakan ikan yang tahan terhadap pemadaman listrik (*power outage*) dengan menyimpan konfigurasi jadwal dan durasi ke dalam memori non-volatile (**Preferences**). 

Sistem ini memicu mekanisme pakan menggunakan **Exhaust Blower** (dikendalikan via Relay Low Trigger) sesuai dengan 3 jadwal pakan harian yang dapat dikonfigurasi langsung melalui tombol navigasi tanpa perlu mengunggah ulang kode program.

---

## ⚙️ Fitur Utama

- **Real-Time Clock (RTC DS3231):** Menjaga akurasi waktu secara presisi meskipun perangkat kehilangan daya listrik.
- **Tampilan Interaktif LCD I2C 16x2:** Memudahkan pemantauan waktu riil, status pakan, serta navigasi menu pengaturan.
- **Pengaturan Tanggal & Waktu Lengkap:** Fitur kalibrasi Tahun, Bulan, Tanggal, Jam, dan Menit langsung melalui tombol fisik.
- **Kalkulasi Kalender Otomatis:** Memperhitungkan batas jumlah hari dalam setiap bulan secara dinamis serta memperhitungkan **Tahun Kabisat**.
- **3 Jadwal Pakan & Durasi Lontaran Disesuaikan:** Penjadwalan 3 kali pakan harian lengkap dengan durasi aktif blower yang dapat diatur.
- **Penyimpanan Permanen (`Preferences.h`):** Jadwal dan durasi pakan tersimpan di NVS ESP32 dan tidak akan hilang saat mati listrik.
- **Kontrol Tombol Cerdas (Multifungsi):**
  - **SET (Short Press):** Pindah antar menu *setting*.
  - **SET (Long Press):** Menyimpan konfigurasi / Pemicu *Manual Feeding* (Blower manual).
  - **UP:** Menaikkan nilai opsi secara berputar (*CYCLIC*).
  - **RESET (Long Press):** Mengembalikan jadwal & durasi pakan ke *default* tanpa mengubah konfigurasi jam RTC.
- **Sistem Non-Blocking (`millis()`):** Eksekusi *software* yang responsif tanpa jeda tertahan (`delay()`), memastikan pembacaan tombol dan pemutaran jam tetap fleksibel.

---

## 🛠️ Pinout & Skematik Hardware

### **Detail Koneksi Pin ESP32:**

| Komponen / Modul | Pin Modul | Pin ESP32 | Catatan Logika / Keterangan |
| :--- | :--- | :--- | :--- |
| **Relay Blower** | IN | `GPIO 26` | Trigger LOW (Aktif saat `LOW`) |
| **Tombol UP** | Signal | `GPIO 33` | Internal Pull-Up (`INPUT_PULLUP`) |
| **Tombol SET** | Signal | `GPIO 32` | Internal Pull-Up (`INPUT_PULLUP`) |
| **Tombol RESET** | Signal | `GPIO 27` | Internal Pull-Up (`INPUT_PULLUP`) |
| **RTC DS3231 & LCD** | SDA | `GPIO 21` | Jalur I2C Data |
| **RTC DS3231 & LCD** | SCL | `GPIO 22` | Jalur I2C Clock |

> ⚠️ **Catatan Skematik Tombol:**  
> Rangkaian tombol terhubung dengan logika `GPIO -> Tombol -> GND`.  
> Menggunakan fitur `INPUT_PULLUP`:
> - `HIGH` = Tombol tidak ditekan  
> - `LOW` = Tombol ditekan  

---

## ⚙️ Konfigurasi Bawaan (Default Setting)

Saat pertama kali diaktifkan atau setelah fungsi **RESET (Long Press)** dijalankan, sistem akan menggunakan parameter default berikut:

- **Jadwal Pakan 1:** `07:00`
- **Jadwal Pakan 2:** `12:00`
- **Jadwal Pakan 3:** `17:00`
- **Durasi Blower:** `5 Detik`

*(Catatan: Prosedur RESET hanya mengembalikan jadwal & durasi pakan ke nilai di atas dan **TIDAK** mengubah settingan RTC DS3231).*

---

## 📁 Struktur Repositori

```text
├── Fish_Feeder_ESP32.ino    # Source code utama Arduino/ESP32 (RTC, LCD, Menu, & Preferences)
└── README.md                # Dokumentasi resmi proyek
