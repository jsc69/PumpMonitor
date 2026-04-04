#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>

// Definiere die Pins explizit, um Konflikte mit internen Makros zu vermeiden
#define EPD_CS    5
#define EPD_DC    17
#define EPD_RST   16
#define EPD_BUSY  4

// Nutze den GDEY0154D67 Treiber für das WeAct 1.54" Modul (SSD1681) [cite: 10, 16]
GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(GxEPD2_154_GDEY0154D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

void setup() {
  Serial.begin(115200);
  delay(1000); // Kurze Pause für den Serial Monitor
  Serial.println("WeAct Display Test startet...");

  display.init(115200, true, 2, false); // Initialisierung mit Reset-Impuls
  
  display.setRotation(1);
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(10, 30);
    display.print("WeAct OK!");
    
    display.drawLine(0, 45, 200, 45, GxEPD_BLACK);
    
    display.setFont(); // Wechsel zum Standard-Systemfont
    display.setCursor(10, 70);
    display.println("Treiber: GDEY0154D67");
    display.setCursor(10, 90);
    display.println("System: ESP32");
    
    display.drawRect(10, 120, 180, 40, GxEPD_BLACK);
    display.setCursor(45, 145);
    display.print("TEST ERFOLGREICH");
    
  } while (display.nextPage());

  Serial.println("Update fertig. Display schlaeft.");
  display.hibernate();
}

void loop() {
  // Bleibt leer für den Test
}