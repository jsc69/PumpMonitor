#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

// --- PIN-KONFIGURATION (DEIN SETUP) ---
#define PIN_STROM       35  // ZMCT103C (Analog) [cite: 10]
#define PIN_NETZ_CHECK  32  // Versorgungsspannung (Analog/Digital) [cite: 10]
#define PIN_AKKU        36  // Battery Shield A0 [cite: 10]
#define PIN_BUTTON_DOWN 27  // Button 1 [cite: 10]
#define PIN_BUTTON_UP   25  // Button 2 [cite: 10]

// Die integrierte LED liegt beim ESP32 D1 Mini meist auf Pin 2
const int PIN_LED = 2;

// Display Pins [cite: 10, 11]
#define CS   5
#define DC  17
#define RST 16
#define BUSY 21

GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(GxEPD2_154_GDEY0154D67(CS, DC, RST, BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

static const char* TAG = "PUMP_TEST";

void setup() {
  pinMode(PIN_NETZ_CHECK, INPUT);
  pinMode(PIN_STROM, INPUT);
  pinMode(PIN_AKKU, INPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  //Serial.begin(115200);
  //esp_log_level_set(TAG, ESP_LOG_VERBOSE);

  delay(2000);

  display.init(115200, true, 50, false); // Wichtig für E-Ink nach Sleep/Reset 
  u8g2Fonts.begin(display);

  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
}

void loop() {
  // 1. Sensoren auslesen
  // Akku (Pin 36)
  int rawAkku = analogRead(PIN_AKKU);
  float volAkku = (rawAkku / 4095.0) * 3.3 * 2.0 * 1.1; 

  // Versorgung (Pin 32)
  int rawNetz = analogRead(PIN_NETZ_CHECK);
  float volNetz = (rawNetz / 4095.0) * 3.3 * 2.0; // Beispielhafter Teiler

  // Stromsensor Pumpe (Pin 35) - Peak-to-Peak Messung
  int mx = 0, mn = 4095;
  uint32_t start = millis();
  while (millis() - start < 100) {
    int v = analogRead(PIN_STROM);
    if (v > mx) mx = v;
    if (v < mn) mn = v;
  }
  int stromDiff = mx - mn;

  // Buttons (Low-Aktiv) [cite: 74]
  bool b1 = !digitalRead(PIN_BUTTON_DOWN);
  bool b2 = !digitalRead(PIN_BUTTON_UP);

  // 2. Logging [cite: 61, 67, 93]
//  ESP_LOGI(TAG, "V_Akku: %.2fV | V_Netz: %.2fV | Strom_P2P: %d", volAkku, volNetz, stromDiff);
//  ESP_LOGD(TAG, "Buttons: B1=%s, B2=%s", b1 ? "AN" : "AUS", b2 ? "AN" : "AUS");

  // 3. Display Ausgabe [cite: 14, 15, 20]
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    
    u8g2Fonts.setFont(u8g2_font_helvB18_tf); 
    u8g2Fonts.setCursor(5, 30);
    u8g2Fonts.print("HARDWARE TEST (10s)");
    
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    u8g2Fonts.setCursor(5, 60);
    u8g2Fonts.print("Akku (36): " + String(volAkku) + "V");
    u8g2Fonts.setCursor(5, 85);
    u8g2Fonts.print("Netz (32): " + String(volNetz) + "V");
    u8g2Fonts.setCursor(5, 110);
    u8g2Fonts.print("Pumpe (35): Δ " + String(stromDiff));
    u8g2Fonts.setCursor(5, 135);
    u8g2Fonts.print("Button 27: " + String(b1 ? "GEDRUECKT" : "OFFEN"));
    u8g2Fonts.setCursor(5, 160);
    u8g2Fonts.print("Button 25: " + String(b2 ? "GEDRUECKT" : "OFFEN"));

  } while (display.nextPage());

  for (int i=0; i<5; i++) {
    digitalWrite(PIN_LED, LOW);
    delay(500);
    digitalWrite(PIN_LED, HIGH);
    delay(500);
  }
}