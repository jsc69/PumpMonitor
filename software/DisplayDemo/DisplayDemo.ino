#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#define PIN_CS    5
#define PIN_DC    17
#define PIN_RST   16
#define PIN_BUSY  4

GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(GxEPD2_154_GDEY0154D67(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// Demo-Werte
bool alarmSperre = false;
bool geraetLiefZuvor = false;
int wakeupCounter = 25;
float batVoltage = 3.72;
char lastSystemStart[20] = "22.03. 18:00"; 

void updateDisplay(String header, String detail, char iconCode) {
  Serial.println("Update: " + header + " - " + detail);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // 1. INVERTIERTER HEADER-BALKEN
    display.fillRect(0, 0, 200, 50, GxEPD_BLACK); // Etwas höher für Icons
    
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    
    // Icon zeichnen (links im Header)
    u8g2Fonts.setFont(u8g2_font_open_iconic_all_4x_t); 
    u8g2Fonts.setCursor(5, 42);
    u8g2Fonts.drawGlyph(5, 42, iconCode); // Zeichnet das Icon anhand des Codes

    // Header-Text (leicht nach rechts verschoben wegen Icon)
    u8g2Fonts.setFont(u8g2_font_helvB18_tf); 
    u8g2Fonts.setCursor(40, 35); 
    u8g2Fonts.print(header);

    // 2. DETAILS (Normal)
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB18_tf); 
    u8g2Fonts.setCursor(0, 82);
    u8g2Fonts.print(detail);

    // 3. STATUS-BEREICH (Wie gehabt)
    display.drawLine(0, 142, 200, 142, GxEPD_BLACK); 
    u8g2Fonts.setFont(u8g2_font_helvB10_tf); 
    u8g2Fonts.setCursor(0, 158);
    u8g2Fonts.print("SPERRE: " + String(alarmSperre ? "AN" : "AUS") + " | PUMPE: " + String(geraetLiefZuvor ? "LIEF" : "OK"));
    u8g2Fonts.setCursor(0, 176);
    u8g2Fonts.print("AKKU: " + String(batVoltage) + "V | LAUF: " + String(wakeupCounter));
    u8g2Fonts.setCursor(0, 194);
    u8g2Fonts.print("CHECK: " + String(lastSystemStart));

  } while (display.nextPage());
  display.hibernate();
  Serial.println("Updat DONE");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Diese Zeilen verhindern die "is not set as GPIO" Fehler:
  Serial.println("PIN_CS");
  pinMode(PIN_CS, OUTPUT);   // Pin 5 
  Serial.println("PIN_D");
  pinMode(PIN_DC, OUTPUT);   // Pin 17 
  Serial.println("PIN_RST");
  pinMode(PIN_RST, OUTPUT);  // Pin 16 
  Serial.println("PIN_BUSY");
  pinMode(PIN_BUSY, INPUT);  // Pin 4
  Serial.println("PINS ARE OK");

  display.init(115200, true, 2, false); 
  u8g2Fonts.begin(display); 
  display.setRotation(0);
}

void loop() {
  static int screenState = 0;

switch (screenState) {
  case 0: updateDisplay("FEHLER",    "Kein WLAN\nKein Internet", 247); break;
  case 1: updateDisplay("AKKU LEER", "System ist aus!",  90); break;
  case 2: updateDisplay("OTA",       "Verbinde...",     84); break;
  case 3: updateDisplay("OTA AKTIV", "IP: 192.168.1.50\nUser: admin\nPwd: 1234567890", 129); break;
  case 4: updateDisplay("RESET",     "System\nzurückgesetzt",  243); break;
  case 5: updateDisplay("STROM AUS", "Stromausfall!\nseit 22.03. 10:00h",170); break;
  case 6: updateDisplay("PUMPE EIN", "Pumpe läuft\nseit 22.03. 10:00h", 121); break;
  case 7: updateDisplay("SYSTEM OK", "Statusbericht\ngesendet\nam 22.03. 10:00h", 120); break;
  case 8: updateDisplay("SYSTEM OK", "Letzte Prüfung\nam 22.03. 10:00h", 120); break;
}

  screenState = (screenState + 1) % 9;
  delay(10000); 
}