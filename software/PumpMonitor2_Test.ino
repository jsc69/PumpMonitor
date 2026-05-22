/**
 * PumpMonitor2 - Hardware-Testprogramm
 * =====================================
 * Ziel: Systematischer Test aller Hardwarekomponenten der Platine
 *
 * Ablauf:
 *   Jeder Testschritt wird über den seriellen Monitor (115200 Baud) geführt.
 *   Mit BTN_UP wird zum nächsten Test gewechselt.
 *   Mit BTN_DOWN wird der aktuelle Test wiederholt.
 *
 * Pin-Belegung (aus Schaltplan abgeleitet):
 *   IO2  -> DIS_MOSI   SPI MOSI Display
 *   IO3  -> DIS_SCK    SPI Clock Display
 *   IO4  -> DIS_CS     Display Chip Select
 *   IO5  -> DIS_DC     Display Data/Command
 *   IO6  -> DIS_RST    Display Reset
 *   IO7  -> DIS_BUSY   Display Busy-Signal
 *   IO8  -> PUMP_CHECK ADC: Stromwandler ZMCT103C (Bürde R9=100Ω, Übersetzung 1000:1)
 *   IO9  -> POWER_CHECK ADC: 5V-Rail (Teiler R10=100k / R11=180k)
 *   IO10 -> AKKU_CHECK  ADC: BATT+-Rail (Teiler R12=100k / R13=100k)
 *   IO11 -> BTN_UP     Taster (Active-Low, internem Pull-Up)
 *   IO12 -> BTN_DOWN   Taster (Active-Low, internem Pull-Up)
 *
 * Bibliotheken (Arduino-IDE Bibliotheksmanager):
 *   - GxEPD2  (für E-Paper-Display, Waveshare-kompatibel)
 *   - Adafruit GFX Library
 *   - WiFi (im ESP32-Arduino-Core enthalten)
 *
 * Board: "ESP32S2 Dev Module" oder "ESP32-S2-WROVER" im Arduino-IDE Boardmanager
 */

#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>

// Optionaler E-Paper-Support: Kommentiere diese Zeile aus, wenn kein Display vorhanden
#define TEST_DISPLAY

#ifdef TEST_DISPLAY
  #include <GxEPD2_BW.h>
  #include <Fonts/FreeMonoBold9pt7b.h>
  // GDEY0154D67: 1.54" SSD1681, 200x200 Pixel, Schwarz/Weiss
  GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(
    GxEPD2_154_GDEY0154D67(/*CS=*/4, /*DC=*/5, /*RST=*/6, /*BUSY=*/7)
  );
#endif

// ── Pin-Definitionen ─────────────────────────────────────────────────────────
#define PIN_DIS_MOSI   2
#define PIN_DIS_SCK    3
#define PIN_DIS_CS     4
#define PIN_DIS_DC     5
#define PIN_DIS_RST    6
#define PIN_DIS_BUSY   7

#define PIN_PUMP_CHECK   8   // ADC1_CH7
#define PIN_POWER_CHECK  9   // ADC1_CH8
#define PIN_AKKU_CHECK  10   // ADC1_CH9

#define PIN_BTN_UP      11
#define PIN_BTN_DOWN    12

// ── Messkonstanten ────────────────────────────────────────────────────────────
// Spannungsteiler AKKU_CHECK: R12=100k (oben), R13=100k (unten)
//   Vadc = Vbatt * 100k/(100k+100k) = Vbatt * 0.5
//   -> Vbatt = Vadc * 2.0
const float AKKU_DIVIDER   = 2.0f;

// Spannungsteiler POWER_CHECK: R10=100k (oben), R11=180k (unten)
//   Vadc = V5V * 180k/(100k+180k) = V5V * 0.6429
//   -> V5V = Vadc * (280/180) = Vadc * 1.5556
const float POWER_DIVIDER  = 280.0f / 180.0f;

// Stromwandler ZMCT103C: Übersetzung 1000:1, Bürde R9 = 100 Ω
//   Vadc = I_pump / 1000 * R9  ->  I_pump [A] = Vadc * 1000 / 100 = Vadc * 10
const float PUMP_I_FACTOR  = 10.0f;  // A/V

// ESP32-S2 ADC: 12 Bit, Referenz 3.3V (nach Kalibrierung)
const float ADC_REF_V      = 3.3f;
const int   ADC_RESOLUTION = 4095;

// Anzahl der ADC-Samples für Strommessung (AC-RMS)
const int   PUMP_SAMPLES   = 1000;

// ── Testvariablen ─────────────────────────────────────────────────────────────
enum TestStep {
  TEST_INTRO = 0,
  TEST_BUTTONS,
  TEST_ADC_SUPPLY,
  TEST_ADC_PUMP,
  TEST_WIFI,
  TEST_DISPLAY,
  TEST_DONE,
  TEST_COUNT
};

TestStep currentTest = TEST_INTRO;
bool testPassed[TEST_COUNT] = {};

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

float adcToVolt(int raw) {
  return (raw / (float)ADC_RESOLUTION) * ADC_REF_V;
}

// Liest mehrere Samples und gibt den Mittelwert zurück
float readVoltageAvg(int pin, int samples = 64) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100);
  }
  return adcToVolt(sum / samples);
}

// RMS-Messung für Wechselstromsignal (PUMP_CHECK)
// Misst über ~20 ms (bei 50 Hz), gibt RMS-Strom in Ampere zurück
float readPumpCurrentRMS() {
  long sumSq = 0;
  int baseline = 0;

  // Offset-Mittelwert messen (DC-Anteil, sollte bei ~Vcc/2 liegen wenn Bias-Netzwerk vorhanden)
  long baseSum = 0;
  for (int i = 0; i < 128; i++) {
    baseSum += analogRead(PIN_PUMP_CHECK);
    delayMicroseconds(50);
  }
  baseline = baseSum / 128;

  // RMS über PUMP_SAMPLES Messungen
  for (int i = 0; i < PUMP_SAMPLES; i++) {
    int raw = analogRead(PIN_PUMP_CHECK) - baseline;
    sumSq += (long)raw * raw;
    delayMicroseconds(20);  // ~50 kHz Abtastrate
  }

  float rmsRaw = sqrt((float)sumSq / PUMP_SAMPLES);
  float rmsVolt = (rmsRaw / ADC_RESOLUTION) * ADC_REF_V;
  return rmsVolt * PUMP_I_FACTOR;
}

void printSeparator() {
  Serial.println(F("────────────────────────────────────────"));
}

void printResult(const char* name, bool ok) {
  Serial.print(F("  "));
  Serial.print(name);
  Serial.print(F(": "));
  Serial.println(ok ? F("✓ OK") : F("✗ FEHLER"));
}

bool waitForButton(unsigned long timeoutMs = 10000) {
  // Gibt true zurück wenn BTN_UP gedrückt, false bei Timeout oder BTN_DOWN
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (digitalRead(PIN_BTN_UP) == LOW) {
      delay(50);  // Entprellen
      while (digitalRead(PIN_BTN_UP) == LOW);
      return true;
    }
    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      delay(50);
      while (digitalRead(PIN_BTN_DOWN) == LOW);
      return false;
    }
    delay(10);
  }
  return true;  // Timeout: automatisch weiter
}

// ── Einzelne Testfunktionen ───────────────────────────────────────────────────

void testIntro() {
  printSeparator();
  Serial.println(F("PumpMonitor2 - Hardware-Selbsttest"));
  Serial.println(F("Firmware: " __DATE__ " " __TIME__));
  printSeparator();
  Serial.println(F("Steuerung:"));
  Serial.println(F("  BTN_UP   -> Weiter / Bestätigen"));
  Serial.println(F("  BTN_DOWN -> Wiederholen"));
  Serial.println(F(""));
  Serial.println(F("Drücke BTN_UP zum Starten..."));
  waitForButton(0xFFFFFFFF);  // Warten ohne Timeout
  testPassed[TEST_INTRO] = true;
}

void testButtons() {
  printSeparator();
  Serial.println(F("TEST 1: Taster"));
  printSeparator();

  bool upOk = false, downOk = false;
  unsigned long start = millis();

  Serial.println(F("Drücke BTN_UP (IO11)..."));
  while (!upOk && millis() - start < 8000) {
    if (digitalRead(PIN_BTN_UP) == LOW) {
      upOk = true;
      Serial.println(F("  -> BTN_UP erkannt ✓"));
      delay(300);
    }
  }

  Serial.println(F("Drücke BTN_DOWN (IO12)..."));
  start = millis();
  while (!downOk && millis() - start < 8000) {
    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      downOk = true;
      Serial.println(F("  -> BTN_DOWN erkannt ✓"));
      delay(300);
    }
  }

  if (!upOk)   Serial.println(F("  -> BTN_UP nicht reagiert ✗"));
  if (!downOk) Serial.println(F("  -> BTN_DOWN nicht reagiert ✗"));

  testPassed[TEST_BUTTONS] = upOk && downOk;
  printResult("Taster-Test", testPassed[TEST_BUTTONS]);
  delay(1000);
}

void testAdcSupply() {
  printSeparator();
  Serial.println(F("TEST 2: Versorgungsspannungen (ADC)"));
  printSeparator();

  float vBatt  = readVoltageAvg(PIN_AKKU_CHECK)  * AKKU_DIVIDER;
  float v5V    = readVoltageAvg(PIN_POWER_CHECK) * POWER_DIVIDER;

  Serial.print(F("  BATT+   : "));
  Serial.print(vBatt, 2);
  Serial.println(F(" V  (Soll: 3.2 – 4.2 V)"));

  Serial.print(F("  +5V-Rail: "));
  Serial.print(v5V, 2);
  Serial.println(F(" V  (Soll: 4.7 – 5.3 V)"));

  bool battOk  = (vBatt  >= 3.0f && vBatt  <= 4.4f);
  bool v5Ok    = (v5V    >= 4.5f && v5V    <= 5.5f);

  if (!battOk) Serial.println(F("  WARNUNG: BATT+ ausserhalb Toleranz!"));
  if (!v5Ok)   Serial.println(F("  WARNUNG: +5V ausserhalb Toleranz (nur bei USB-Betrieb relevant)"));

  // 5V ist optional (Load-Switch gesteuert), daher nur BATT für Pass/Fail
  testPassed[TEST_ADC_SUPPLY] = battOk;
  printResult("Spannungstest", testPassed[TEST_ADC_SUPPLY]);
  delay(1000);
}

void testAdcPump() {
  printSeparator();
  Serial.println(F("TEST 3: Strommessung (ZMCT103C)"));
  printSeparator();
  Serial.println(F("Hinweis: Pumpe muss für AC-Messung laufen."));
  Serial.println(F("Ohne Pumpe wird ~0.00 A gemessen (Ruhezustand OK)."));
  Serial.println(F(""));

  // Ruhemessung
  float iRuhe = readPumpCurrentRMS();
  Serial.print(F("  Ruhestrom (ADC-Offset): "));
  Serial.print(iRuhe * 1000.0f, 1);
  Serial.println(F(" mA  (Soll: < 50 mA)"));

  bool ruheOk = (iRuhe < 0.05f);

  // Einzelwert-Debug: Rohe ADC-Werte ausgeben
  Serial.println(F("  ADC-Rohwerte (16 Samples):"));
  Serial.print(F("  "));
  for (int i = 0; i < 16; i++) {
    Serial.print(analogRead(PIN_PUMP_CHECK));
    Serial.print(i < 15 ? F(", ") : F("\n"));
    delay(1);
  }

  testPassed[TEST_ADC_PUMP] = ruheOk;
  printResult("Strommessung Ruhe", testPassed[TEST_ADC_PUMP]);
  delay(500);
}

void testWifi() {
  printSeparator();
  Serial.println(F("TEST 4: WLAN (Scan)"));
  printSeparator();
  Serial.println(F("Scanne Netzwerke..."));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int found = WiFi.scanNetworks();

  if (found == 0) {
    Serial.println(F("  Keine Netzwerke gefunden (Antenne ok? Umgebung leer?)"));
    testPassed[TEST_WIFI] = false;
  } else if (found < 0) {
    Serial.println(F("  Scan fehlgeschlagen ✗"));
    testPassed[TEST_WIFI] = false;
  } else {
    Serial.print(F("  "));
    Serial.print(found);
    Serial.println(F(" Netzwerk(e) gefunden:"));
    for (int i = 0; i < min(found, 5); i++) {
      Serial.print(F("    ["));
      Serial.print(i + 1);
      Serial.print(F("] "));
      Serial.print(WiFi.SSID(i));
      Serial.print(F("  RSSI: "));
      Serial.print(WiFi.RSSI(i));
      Serial.println(F(" dBm"));
    }
    testPassed[TEST_WIFI] = true;
  }

  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  printResult("WLAN-Test", testPassed[TEST_WIFI]);
  delay(500);
}

void testDisplay() {
  printSeparator();
  Serial.println(F("TEST 5: E-Paper-Display"));
  printSeparator();

#ifdef TEST_DISPLAY
  Serial.println(F("  Initialisiere Display..."));

  SPI.begin(PIN_DIS_SCK, -1, PIN_DIS_MOSI, PIN_DIS_CS);
  display.init(115200, true, 2, false);

  Serial.println(F("  Zeige Testbild..."));

  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  display.setFullWindow();
  display.firstPage();
  // Messwerte einmalig vor der Darstellung lesen
  float vBattDisp  = readVoltageAvg(PIN_AKKU_CHECK)  * AKKU_DIVIDER;
  float v5VDisp    = readVoltageAvg(PIN_POWER_CHECK) * POWER_DIVIDER;
  float iPumpDisp  = readPumpCurrentRMS() * 1000.0f;  // in mA

  do {
    display.fillScreen(GxEPD_WHITE);

    // Äusserer Rahmen
    display.drawRect(2, 2, display.width()-4, display.height()-4, GxEPD_BLACK);
    // Trennlinie unter Titel
    display.drawLine(4, 30, display.width()-5, 30, GxEPD_BLACK);

    // Titel (200x200 Hochformat)
    display.setCursor(10, 22);
    display.print(F("PumpMonitor2 Test"));

    // Messwerte
    display.setCursor(10, 50);
    display.print(F("BATT+: "));
    display.print(vBattDisp, 2);
    display.print(F(" V"));

    display.setCursor(10, 70);
    display.print(F("5V:    "));
    display.print(v5VDisp, 2);
    display.print(F(" V"));

    display.setCursor(10, 90);
    display.print(F("Pump:  "));
    display.print(iPumpDisp, 1);
    display.print(F(" mA"));

    // Trennlinie
    display.drawLine(4, 108, display.width()-5, 108, GxEPD_BLACK);

    // Testergebnisse kompakt
    display.setCursor(10, 126);
    display.print(F("Tasten: "));
    display.print(testPassed[TEST_BUTTONS]    ? F("OK") : F("--"));

    display.setCursor(10, 146);
    display.print(F("Spanng: "));
    display.print(testPassed[TEST_ADC_SUPPLY] ? F("OK") : F("--"));

    display.setCursor(10, 166);
    display.print(F("WLAN:   "));
    display.print(testPassed[TEST_WIFI]       ? F("OK") : F("--"));

    display.setCursor(10, 186);
    display.print(F("Display-Test OK?"));

  } while (display.nextPage());

  display.hibernate();
  Serial.println(F("  Display-Ausgabe abgeschlossen."));
  Serial.println(F("  Bitte Display visuell prüfen!"));

  testPassed[TEST_DISPLAY] = true;  // Manuell verifizieren
  Serial.println(F("  -> Siehst du Text auf dem Display? BTN_UP=Ja / BTN_DOWN=Nein"));
  bool userOk = waitForButton(15000);
  testPassed[TEST_DISPLAY] = userOk;

#else
  Serial.println(F("  Display-Test deaktiviert (#define TEST_DISPLAY auskommentiert)"));
  testPassed[TEST_DISPLAY] = true;
#endif

  printResult("Display-Test", testPassed[TEST_DISPLAY]);
  delay(500);
}

void testSummary() {
  printSeparator();
  Serial.println(F("TESTERGEBNIS"));
  printSeparator();

  const char* names[] = {
    "Intro        ",
    "Taster       ",
    "Versorgung   ",
    "Strommessung ",
    "WLAN         ",
    "Display      "
  };

  int passed = 0;
  for (int i = 0; i < TEST_DONE; i++) {
    Serial.print(F("  "));
    Serial.print(names[i]);
    Serial.println(testPassed[i] ? F("✓") : F("✗"));
    if (testPassed[i]) passed++;
  }

  printSeparator();
  Serial.print(passed);
  Serial.print(F("/"));
  Serial.print((int)TEST_DONE);
  Serial.println(passed == TEST_DONE ? F(" Tests bestanden  ✓✓✓") : F(" Tests bestanden"));

  if (passed < TEST_DONE) {
    Serial.println(F("Fehlerhafte Tests bitte manuell prüfen."));
  }

  testPassed[TEST_DONE] = true;
}

// ── Arduino Setup / Loop ──────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);

  // ADC-Auflösung auf 12 Bit
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);  // Messbereich 0 – 3.3 V

  // Taster: Active-Low mit internem Pull-Up (kein externer Pull-Up im Schaltplan)
  pinMode(PIN_BTN_UP,   INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
}

void loop() {
  switch (currentTest) {
    case TEST_INTRO:      testIntro();      break;
    case TEST_BUTTONS:    testButtons();    break;
    case TEST_ADC_SUPPLY: testAdcSupply();  break;
    case TEST_ADC_PUMP:   testAdcPump();    break;
    case TEST_WIFI:       testWifi();       break;
    case TEST_DISPLAY:    testDisplay();    break;
    case TEST_DONE:       testSummary();    break;
    default: break;
  }

  if (currentTest < TEST_DONE) {
    Serial.println(F(""));
    Serial.println(F("BTN_UP -> Weiter  |  BTN_DOWN -> Wiederholen"));
    bool advance = waitForButton(30000);
    if (advance) {
      currentTest = (TestStep)((int)currentTest + 1);
    }
    // Bei BTN_DOWN bleibt currentTest gleich -> Test wird wiederholt
  } else {
    // Fertig – Endlosschleife, Reset zum Neustart
    delay(5000);
    Serial.println(F("Drücke Reset zum Neustart."));
    while (true) delay(1000);
  }
}
