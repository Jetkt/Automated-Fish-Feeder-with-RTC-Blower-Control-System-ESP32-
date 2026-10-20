/*
  =====================================================================
       SISTEM PAKAN IKAN OTOMATIS - ESP32 DEVKIT V1
       FINAL VERSION
  =====================================================================

  FITUR:
  - RTC DS3231 real-time
  - LCD I2C 16x2
  - Setting tahun
  - Setting bulan
  - Setting tanggal
  - Setting jam
  - Setting menit
  - 3 jadwal pakan
  - Setting durasi blower
  - Preferences untuk menyimpan jadwal & durasi
  - SET short press  = pindah menu
  - SET long press   = simpan / manual feed
  - UP               = menaikkan nilai secara CYCLIC
  - RESET long press = kembali ke setting awal
  - Blower otomatis sesuai jadwal
  - Blower manual melalui SET long press
  - Tidak menggunakan tombol DOWN
  - Tanggal mengikuti jumlah hari pada bulan
  - Tahun kabisat diperhitungkan
  - Relay LOW Trigger
  - Non-blocking menggunakan millis()

  =====================================================================

  PIN:

  GPIO26 -> Relay blower
  GPIO33 -> Tombol UP
  GPIO32 -> Tombol SET
  GPIO27 -> Tombol RESET

  GPIO21 -> SDA RTC + LCD
  GPIO22 -> SCL RTC + LCD

  Tombol:
  GPIO -> tombol -> GND

  Karena INPUT_PULLUP:
  HIGH = tidak ditekan
  LOW  = ditekan

  =====================================================================

  DEFAULT SETTING:

  Jadwal 1 = 07:00
  Jadwal 2 = 12:00
  Jadwal 3 = 17:00
  Durasi   = 5 detik

  RESET TIDAK mengubah RTC.
  RESET hanya mengembalikan jadwal dan durasi.
*/


// =====================================================================
// LIBRARY
// =====================================================================

#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>


// =====================================================================
// PIN
// =====================================================================

#define RELAY_PIN       26

#define BTN_UP_PIN      33
#define BTN_SET_PIN     32
#define BTN_RESET_PIN   27

#define I2C_SDA         21
#define I2C_SCL         22


// =====================================================================
// RELAY
// =====================================================================

#define RELAY_ON  LOW
#define RELAY_OFF HIGH


// =====================================================================
// OBJEK
// =====================================================================

RTC_DS3231 rtc;

LiquidCrystal_I2C lcd(0x27, 16, 2);

Preferences preferences;


// =====================================================================
// KONSTANTA
// =====================================================================

const unsigned long DEBOUNCE_DELAY        = 50;
const unsigned long LONG_PRESS_TIME      = 1500;
const unsigned long RESET_LONG_PRESS     = 2000;

const unsigned long LCD_UPDATE_INTERVAL  = 250;
const unsigned long PAGE_ROTATE_INTERVAL = 3000;


// =====================================================================
// DEFAULT SETTING
// =====================================================================

const uint8_t DEFAULT_J1_HOUR   = 7;
const uint8_t DEFAULT_J1_MINUTE = 0;

const uint8_t DEFAULT_J2_HOUR   = 12;
const uint8_t DEFAULT_J2_MINUTE = 0;

const uint8_t DEFAULT_J3_HOUR   = 17;
const uint8_t DEFAULT_J3_MINUTE = 0;

const uint8_t DEFAULT_DURATION  = 5;


// =====================================================================
// BATAS TAHUN
// =====================================================================

const uint16_t MIN_YEAR = 2026;
const uint16_t MAX_YEAR = 2099;


// =====================================================================
// STRUKTUR JADWAL
// =====================================================================

struct Schedule {

  uint8_t hour;

  uint8_t minute;

  bool triggered;
};


Schedule schedule[3];


// =====================================================================
// DURASI BLOWER
// =====================================================================

uint8_t blowerDuration = DEFAULT_DURATION;


// =====================================================================
// STATUS BLOWER
// =====================================================================

bool blowerActive = false;

unsigned long blowerStartTime = 0;


// =====================================================================
// RTC
// =====================================================================

DateTime currentTime;


// Menyimpan tanggal terakhir sistem memproses jadwal
int lastDay = -1;


// =====================================================================
// MENU
// =====================================================================

enum MenuState {

  MENU_NORMAL = 0,

  MENU_SET_YEAR,

  MENU_SET_MONTH,

  MENU_SET_DAY,

  MENU_SET_HOUR,

  MENU_SET_MINUTE,

  MENU_SET_SCHEDULE1_HOUR,

  MENU_SET_SCHEDULE1_MINUTE,

  MENU_SET_SCHEDULE2_HOUR,

  MENU_SET_SCHEDULE2_MINUTE,

  MENU_SET_SCHEDULE3_HOUR,

  MENU_SET_SCHEDULE3_MINUTE,

  MENU_SET_DURATION
};


MenuState menuState = MENU_NORMAL;


// =====================================================================
// VARIABEL TEMPORARY
// =====================================================================

uint16_t tempYear;

uint8_t tempMonth;

uint8_t tempDay;

uint8_t tempHour;

uint8_t tempMinute;

Schedule tempSchedule[3];

uint8_t tempDuration;


// =====================================================================
// STRUKTUR BUTTON
// =====================================================================

struct Button {

  uint8_t pin;

  bool lastReading;

  bool stableState;

  unsigned long lastDebounceTime;

  unsigned long pressStartTime;

  bool longPressFired;

  bool shortPressEvent;

  bool longPressEvent;
};


// =====================================================================
// BUTTON
// =====================================================================

Button btnUp = {

  BTN_UP_PIN,

  HIGH,

  HIGH,

  0,

  0,

  false,

  false,

  false
};


Button btnSet = {

  BTN_SET_PIN,

  HIGH,

  HIGH,

  0,

  0,

  false,

  false,

  false
};


Button btnReset = {

  BTN_RESET_PIN,

  HIGH,

  HIGH,

  0,

  0,

  false,

  false,

  false
};


// =====================================================================
// LCD TIMER
// =====================================================================

unsigned long lastLCDUpdate = 0;

unsigned long lastPageRotate = 0;

uint8_t normalPage = 0;


// =====================================================================
// JUMLAH HARI DALAM BULAN
// =====================================================================

uint8_t daysInMonth(uint16_t year, uint8_t month) {

  switch (month) {

    case 1:
      return 31;

    case 2:

      if ((year % 4 == 0 && year % 100 != 0) ||
          (year % 400 == 0)) {

        return 29;
      }

      return 28;

    case 3:
      return 31;

    case 4:
      return 30;

    case 5:
      return 31;

    case 6:
      return 30;

    case 7:
      return 31;

    case 8:
      return 31;

    case 9:
      return 30;

    case 10:
      return 31;

    case 11:
      return 30;

    case 12:
      return 31;
  }

  return 31;
}


// =====================================================================
// SETUP
// =====================================================================

void setup() {

  Serial.begin(115200);


  // -------------------------------------------------
  // RELAY
  // -------------------------------------------------

  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, RELAY_OFF);


  // -------------------------------------------------
  // BUTTON
  // -------------------------------------------------

  pinMode(BTN_UP_PIN, INPUT_PULLUP);

  pinMode(BTN_SET_PIN, INPUT_PULLUP);

  pinMode(BTN_RESET_PIN, INPUT_PULLUP);


  // -------------------------------------------------
  // I2C
  // -------------------------------------------------

  Wire.begin(I2C_SDA, I2C_SCL);


  // -------------------------------------------------
  // LCD
  // -------------------------------------------------

  lcd.init();

  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("Fish Feeder");

  lcd.setCursor(0, 1);

  lcd.print("Initializing");


  // -------------------------------------------------
  // RTC
  // -------------------------------------------------

  if (!rtc.begin()) {

    digitalWrite(RELAY_PIN, RELAY_OFF);

    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("RTC ERROR!");

    lcd.setCursor(0, 1);

    lcd.print("Check DS3231");

    Serial.println("RTC DS3231 tidak terdeteksi!");

    while (1) {

      digitalWrite(RELAY_PIN, RELAY_OFF);

      delay(1000);
    }
  }


  // -------------------------------------------------
  // RTC KEHILANGAN DAYA
  // -------------------------------------------------

  if (rtc.lostPower()) {

    Serial.println("RTC kehilangan daya.");

    Serial.println("Menggunakan waktu compile.");

    rtc.adjust(
      DateTime(F(__DATE__), F(__TIME__))
    );
  }


  // -------------------------------------------------
  // LOAD PREFERENCES
  // -------------------------------------------------

  loadPreferences();


  delay(800);

  lcd.clear();
}


// =====================================================================
// LOOP
// =====================================================================

void loop() {

  readButtons();

  readRTC();

  menuSystem();

  checkSchedule();

  controlBlower();

  updateLCD();
}


// =====================================================================
// BACA RTC
// =====================================================================

void readRTC() {

  currentTime = rtc.now();
}


// =====================================================================
// UPDATE BUTTON
// =====================================================================

void updateButton(Button &b) {

  bool reading = digitalRead(b.pin);


  if (reading != b.lastReading) {

    b.lastDebounceTime = millis();
  }


  if ((millis() - b.lastDebounceTime) >
      DEBOUNCE_DELAY) {

    if (reading != b.stableState) {

      b.stableState = reading;


      // ---------------------------------------------
      // TOMBOL DITEKAN
      // ---------------------------------------------

      if (b.stableState == LOW) {

        b.pressStartTime = millis();

        b.longPressFired = false;
      }


      // ---------------------------------------------
      // TOMBOL DILEPAS
      // ---------------------------------------------

      else {

        if (!b.longPressFired) {

          unsigned long heldTime =
            millis() - b.pressStartTime;


          if (heldTime < LONG_PRESS_TIME) {

            b.shortPressEvent = true;
          }
        }
      }
    }
  }


  // -----------------------------------------------
  // LONG PRESS
  // -----------------------------------------------

  if (b.stableState == LOW &&
      !b.longPressFired) {

    if ((millis() - b.pressStartTime) >=
        LONG_PRESS_TIME) {

      b.longPressFired = true;

      b.longPressEvent = true;
    }
  }


  b.lastReading = reading;
}


// =====================================================================
// READ BUTTONS
// =====================================================================

void readButtons() {

  updateButton(btnUp);

  updateButton(btnSet);

  updateButton(btnReset);
}


// =====================================================================
// LOAD DATA KE TEMP
// =====================================================================

void loadTempFromCurrent() {

  tempYear = currentTime.year();

  tempMonth = currentTime.month();

  tempDay = currentTime.day();

  tempHour = currentTime.hour();

  tempMinute = currentTime.minute();


  for (int i = 0; i < 3; i++) {

    tempSchedule[i].hour =
      schedule[i].hour;

    tempSchedule[i].minute =
      schedule[i].minute;

    tempSchedule[i].triggered =
      false;
  }


  tempDuration = blowerDuration;
}


// =====================================================================
// ADJUST VALUE - UP CYCLIC
// =====================================================================

void adjustValue() {

  switch (menuState) {


    // =================================================
    // TAHUN
    // =================================================

    case MENU_SET_YEAR:

      tempYear++;

      if (tempYear > MAX_YEAR) {

        tempYear = MIN_YEAR;
      }


      if (tempDay >
          daysInMonth(tempYear, tempMonth)) {

        tempDay =
          daysInMonth(tempYear, tempMonth);
      }

      break;


    // =================================================
    // BULAN
    // =================================================

    case MENU_SET_MONTH:

      tempMonth++;

      if (tempMonth > 12) {

        tempMonth = 1;
      }


      if (tempDay >
          daysInMonth(tempYear, tempMonth)) {

        tempDay =
          daysInMonth(tempYear, tempMonth);
      }

      break;


    // =================================================
    // TANGGAL
    // =================================================

    case MENU_SET_DAY:

      tempDay++;


      if (tempDay >
          daysInMonth(tempYear, tempMonth)) {

        tempDay = 1;
      }

      break;


    // =================================================
    // JAM RTC
    // =================================================

    case MENU_SET_HOUR:

      tempHour++;


      if (tempHour >= 24) {

        tempHour = 0;
      }

      break;


    // =================================================
    // MENIT RTC
    // =================================================

    case MENU_SET_MINUTE:

      tempMinute++;


      if (tempMinute >= 60) {

        tempMinute = 0;
      }

      break;


    // =================================================
    // JADWAL 1 JAM
    // =================================================

    case MENU_SET_SCHEDULE1_HOUR:

      tempSchedule[0].hour++;


      if (tempSchedule[0].hour >= 24) {

        tempSchedule[0].hour = 0;
      }

      break;


    // =================================================
    // JADWAL 1 MENIT
    // =================================================

    case MENU_SET_SCHEDULE1_MINUTE:

      tempSchedule[0].minute++;


      if (tempSchedule[0].minute >= 60) {

        tempSchedule[0].minute = 0;
      }

      break;


    // =================================================
    // JADWAL 2 JAM
    // =================================================

    case MENU_SET_SCHEDULE2_HOUR:

      tempSchedule[1].hour++;


      if (tempSchedule[1].hour >= 24) {

        tempSchedule[1].hour = 0;
      }

      break;


    // =================================================
    // JADWAL 2 MENIT
    // =================================================

    case MENU_SET_SCHEDULE2_MINUTE:

      tempSchedule[1].minute++;


      if (tempSchedule[1].minute >= 60) {

        tempSchedule[1].minute = 0;
      }

      break;


    // =================================================
    // JADWAL 3 JAM
    // =================================================

    case MENU_SET_SCHEDULE3_HOUR:

      tempSchedule[2].hour++;


      if (tempSchedule[2].hour >= 24) {

        tempSchedule[2].hour = 0;
      }

      break;


    // =================================================
    // JADWAL 3 MENIT
    // =================================================

    case MENU_SET_SCHEDULE3_MINUTE:

      tempSchedule[2].minute++;


      if (tempSchedule[2].minute >= 60) {

        tempSchedule[2].minute = 0;
      }

      break;


    // =================================================
    // DURASI
    // =================================================

    case MENU_SET_DURATION:

      tempDuration++;


      if (tempDuration > 120) {

        tempDuration = 1;
      }

      break;


    default:

      break;
  }
}


// =====================================================================
// RESET KE SETELAN AWAL
// =====================================================================

void resetToDefault() {

  schedule[0].hour = DEFAULT_J1_HOUR;
  schedule[0].minute = DEFAULT_J1_MINUTE;

  schedule[1].hour = DEFAULT_J2_HOUR;
  schedule[1].minute = DEFAULT_J2_MINUTE;

  schedule[2].hour = DEFAULT_J3_HOUR;
  schedule[2].minute = DEFAULT_J3_MINUTE;

  blowerDuration = DEFAULT_DURATION;


  for (int i = 0; i < 3; i++) {

    schedule[i].triggered = false;
  }


  savePreferences();


  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("RESET BERHASIL");

  lcd.setCursor(0, 1);

  lcd.print("07 12 17 / 5s");


  Serial.println("==============================");

  Serial.println("RESET SETELAN AWAL");

  Serial.println("J1 = 07:00");

  Serial.println("J2 = 12:00");

  Serial.println("J3 = 17:00");

  Serial.println("Durasi = 5 detik");

  Serial.println("==============================");
}


// =====================================================================
// SIMPAN MENU
// =====================================================================

void saveAndExitMenu() {

  // -------------------------------------------------
  // SET RTC
  // -------------------------------------------------

  rtc.adjust(
    DateTime(
      tempYear,
      tempMonth,
      tempDay,
      tempHour,
      tempMinute,
      0
    )
  );


  // -------------------------------------------------
  // SIMPAN JADWAL
  // -------------------------------------------------

  for (int i = 0; i < 3; i++) {

    schedule[i].hour =
      tempSchedule[i].hour;

    schedule[i].minute =
      tempSchedule[i].minute;

    schedule[i].triggered = false;
  }


  // -------------------------------------------------
  // DURASI
  // -------------------------------------------------

  blowerDuration =
    tempDuration;


  // -------------------------------------------------
  // FLASH
  // -------------------------------------------------

  savePreferences();


  // -------------------------------------------------
  // KELUAR
  // -------------------------------------------------

  menuState = MENU_NORMAL;


  lcd.clear();

  lcd.setCursor(0, 0);

  lcd.print("Pengaturan");

  lcd.setCursor(0, 1);

  lcd.print("Tersimpan");
}


// =====================================================================
// MANUAL FEED
// =====================================================================

void manualFeed() {

  if (!blowerActive) {

    startBlower();


    lcd.clear();

    lcd.setCursor(0, 0);

    lcd.print("Manual Feeding");

    lcd.setCursor(0, 1);

    lcd.print("Blower ON");
  }
}


// =====================================================================
// SISTEM MENU
// =====================================================================

void menuSystem() {

  // =================================================
  // UP
  // =================================================

  if (btnUp.shortPressEvent) {

    btnUp.shortPressEvent = false;


    if (menuState != MENU_NORMAL) {

      adjustValue();
    }
  }


  btnUp.longPressEvent = false;


  // =================================================
  // RESET
  // =================================================

  /*
     RESET HARUS DITEKAN LAMA.

     Tujuannya mencegah salah tekan.
  */

  if (btnReset.longPressEvent) {

    btnReset.longPressEvent = false;


    resetToDefault();


    menuState = MENU_NORMAL;
  }


  btnReset.shortPressEvent = false;


  // =================================================
  // SET SHORT PRESS
  // =================================================

  if (btnSet.shortPressEvent) {

    btnSet.shortPressEvent = false;


    if (menuState == MENU_NORMAL) {

      loadTempFromCurrent();

      menuState = MENU_SET_YEAR;
    }


    else {

      int next =
        (int)menuState + 1;


      if (next > MENU_SET_DURATION) {

        next = MENU_SET_YEAR;
      }


      menuState =
        (MenuState)next;
    }


    lcd.clear();
  }


  // =================================================
  // SET LONG PRESS
  // =================================================

  if (btnSet.longPressEvent) {

    btnSet.longPressEvent = false;


    if (menuState == MENU_NORMAL) {

      manualFeed();
    }


    else {

      saveAndExitMenu();
    }
  }
}


// =====================================================================
// CEK JADWAL
// =====================================================================

void checkSchedule() {

  // -------------------------------------------------
  // Jika tanggal berubah
  // reset status jadwal
  // -------------------------------------------------

  if (currentTime.day() != lastDay) {

    lastDay =
      currentTime.day();


    for (int i = 0; i < 3; i++) {

      schedule[i].triggered =
        false;
    }
  }


  // -------------------------------------------------
  // CEK SEMUA JADWAL
  // -------------------------------------------------

  for (int i = 0; i < 3; i++) {

    bool jamCocok =
      currentTime.hour() ==
      schedule[i].hour;


    bool menitCocok =
      currentTime.minute() ==
      schedule[i].minute;


    if (jamCocok &&
        menitCocok &&
        !schedule[i].triggered) {

      /*
         Tandai terlebih dahulu agar satu
         jadwal tidak dieksekusi berulang.
      */

      schedule[i].triggered = true;


      /*
         Jika blower sedang aktif,
         jadwal tersebut tidak memulai
         blower kedua.
      */

      if (!blowerActive) {

        startBlower();
      }
    }
  }
}


// =====================================================================
// START BLOWER
// =====================================================================

void startBlower() {

  blowerActive = true;

  blowerStartTime = millis();

  digitalWrite(
    RELAY_PIN,
    RELAY_ON
  );
}


// =====================================================================
// KONTROL BLOWER
// =====================================================================

void controlBlower() {

  if (blowerActive) {

    unsigned long elapsed =
      millis() - blowerStartTime;


    if (elapsed >=
        ((unsigned long)blowerDuration *
         1000UL)) {

      blowerActive = false;


      digitalWrite(
        RELAY_PIN,
        RELAY_OFF
      );
    }
  }


  else {

    digitalWrite(
      RELAY_PIN,
      RELAY_OFF
    );
  }
}


// =====================================================================
// TWO DIGITS
// =====================================================================

String twoDigits(int value) {

  String s =
    String(value);


  if (s.length() < 2) {

    s = "0" + s;
  }


  return s;
}


// =====================================================================
// UPDATE LCD
// =====================================================================

void updateLCD() {

  if (millis() - lastLCDUpdate <
      LCD_UPDATE_INTERVAL) {

    return;
  }


  lastLCDUpdate =
    millis();


  if (menuState == MENU_NORMAL) {

    displayNormalScreen();
  }

  else {

    displayMenuScreen();
  }
}


// =====================================================================
// LCD NORMAL
// =====================================================================

void displayNormalScreen() {

  // -------------------------------------------------
  // BARIS 1
  // -------------------------------------------------

  lcd.setCursor(0, 0);


  String line0 =
    twoDigits(currentTime.day()) +
    "/" +
    twoDigits(currentTime.month()) +
    "/" +
    String(currentTime.year() % 100) +
    " " +
    twoDigits(currentTime.hour()) +
    ":" +
    twoDigits(currentTime.minute()) +
    ":" +
    twoDigits(currentTime.second());


  lcd.print(
    line0.substring(0, 16)
  );


  // -------------------------------------------------
  // BARIS 2
  // -------------------------------------------------

  lcd.setCursor(0, 1);


  if (blowerActive) {

    unsigned long elapsed =
      (millis() - blowerStartTime) / 1000;


    long remaining =
      (long)blowerDuration -
      (long)elapsed;


    if (remaining < 0) {

      remaining = 0;
    }


    String line1 =
      "Blower ON " +
      String(remaining) +
      "s";


    while (line1.length() < 16) {

      line1 += " ";
    }


    lcd.print(
      line1.substring(0, 16)
    );
  }


  else {

    if (millis() -
        lastPageRotate >=
        PAGE_ROTATE_INTERVAL) {

      lastPageRotate =
        millis();


      normalPage =
        (normalPage + 1) % 3;
    }


    String line1;


    if (normalPage == 0) {

      line1 =
        "J1:" +
        twoDigits(schedule[0].hour) +
        ":" +
        twoDigits(schedule[0].minute) +
        " J2:" +
        twoDigits(schedule[1].hour) +
        ":" +
        twoDigits(schedule[1].minute);
    }


    else if (normalPage == 1) {

      line1 =
        "J3:" +
        twoDigits(schedule[2].hour) +
        ":" +
        twoDigits(schedule[2].minute) +
        " D:" +
        String(blowerDuration) +
        "s";
    }


    else {

      line1 =
        "SET lama=Feed";
    }


    while (line1.length() < 16) {

      line1 += " ";
    }


    lcd.print(
      line1.substring(0, 16)
    );
  }
}


// =====================================================================
// LCD MENU
// =====================================================================

void displayMenuScreen() {

  String title;

  String value;


  switch (menuState) {


    case MENU_SET_YEAR:

      title = "Set Tahun";

      value =
        String(tempYear);

      break;


    case MENU_SET_MONTH:

      title = "Set Bulan";

      value =
        twoDigits(tempMonth);

      break;


    case MENU_SET_DAY:

      title = "Set Tanggal";

      value =
        twoDigits(tempDay);

      break;


    case MENU_SET_HOUR:

      title = "Set Jam RTC";

      value =
        twoDigits(tempHour);

      break;


    case MENU_SET_MINUTE:

      title = "Set Menit RTC";

      value =
        twoDigits(tempMinute);

      break;


    case MENU_SET_SCHEDULE1_HOUR:

      title = "Jadwal 1 - Jam";

      value =
        twoDigits(
          tempSchedule[0].hour
        );

      break;


    case MENU_SET_SCHEDULE1_MINUTE:

      title = "Jadwal 1 - Menit";

      value =
        twoDigits(
          tempSchedule[0].minute
        );

      break;


    case MENU_SET_SCHEDULE2_HOUR:

      title = "Jadwal 2 - Jam";

      value =
        twoDigits(
          tempSchedule[1].hour
        );

      break;


    case MENU_SET_SCHEDULE2_MINUTE:

      title = "Jadwal 2 - Menit";

      value =
        twoDigits(
          tempSchedule[1].minute
        );

      break;


    case MENU_SET_SCHEDULE3_HOUR:

      title = "Jadwal 3 - Jam";

      value =
        twoDigits(
          tempSchedule[2].hour
        );

      break;


    case MENU_SET_SCHEDULE3_MINUTE:

      title = "Jadwal 3 - Menit";

      value =
        twoDigits(
          tempSchedule[2].minute
        );

      break;


    case MENU_SET_DURATION:

      title = "Durasi Blower";

      value =
        String(tempDuration) +
        " detik";

      break;


    default:

      title = "";

      value = "";

      break;
  }


  // -------------------------------------------------
  // BARIS 1
  // -------------------------------------------------

  lcd.setCursor(0, 0);


  while (title.length() < 16) {

    title += " ";
  }


  lcd.print(
    title.substring(0, 16)
  );


  // -------------------------------------------------
  // BARIS 2
  // -------------------------------------------------

  lcd.setCursor(0, 1);


  String line1 =
    "> " + value;


  while (line1.length() < 16) {

    line1 += " ";
  }


  lcd.print(
    line1.substring(0, 16)
  );
}


// =====================================================================
// SAVE PREFERENCES
// =====================================================================

void savePreferences() {

  preferences.begin(
    "fishfeed",
    false
  );


  preferences.putUChar(
    "s1h",
    schedule[0].hour
  );

  preferences.putUChar(
    "s1m",
    schedule[0].minute
  );


  preferences.putUChar(
    "s2h",
    schedule[1].hour
  );

  preferences.putUChar(
    "s2m",
    schedule[1].minute
  );


  preferences.putUChar(
    "s3h",
    schedule[2].hour
  );

  preferences.putUChar(
    "s3m",
    schedule[2].minute
  );


  preferences.putUChar(
    "dur",
    blowerDuration
  );


  preferences.end();


  Serial.println(
    "Pengaturan tersimpan."
  );
}


// =====================================================================
// LOAD PREFERENCES
// =====================================================================

void loadPreferences() {

  preferences.begin(
    "fishfeed",
    true
  );


  schedule[0].hour =
    preferences.getUChar(
      "s1h",
      DEFAULT_J1_HOUR
    );


  schedule[0].minute =
    preferences.getUChar(
      "s1m",
      DEFAULT_J1_MINUTE
    );


  schedule[1].hour =
    preferences.getUChar(
      "s2h",
      DEFAULT_J2_HOUR
    );


  schedule[1].minute =
    preferences.getUChar(
      "s2m",
      DEFAULT_J2_MINUTE
    );


  schedule[2].hour =
    preferences.getUChar(
      "s3h",
      DEFAULT_J3_HOUR
    );


  schedule[2].minute =
    preferences.getUChar(
      "s3m",
      DEFAULT_J3_MINUTE
    );


  blowerDuration =
    preferences.getUChar(
      "dur",
      DEFAULT_DURATION
    );


  for (int i = 0; i < 3; i++) {

    schedule[i].triggered =
      false;
  }


  preferences.end();


  Serial.println(
    "Preferences dimuat."
  );
}