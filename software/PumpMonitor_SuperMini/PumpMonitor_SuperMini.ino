#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <GxEPD2_BW.h>              // Library GxEPD2
#include <U8g2_for_Adafruit_GFX.h>  // Library U8g2_for_Adafruit_GFX
#include <time.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include "esp_sleep.h"

#include <WiFiClientSecure.h>
#define ENABLE_IMAP
#define ENABLE_SMTP
#include <ReadyMail.h>              // Library ReadyMail

// --- KONFIGURATION ---
static const char* TAG = "PUMP_MONITOR";
const int PUMP_TRIGGER = 1500;  // => Pumpe AUS + (Pumpen AN - Pumpe AUS)/2 => 200 + (2800-200)/2 = 1500

const char* ssid = "DEIN_WLAN_NAME";
const char* password = "DEIN_PASSWORT";
const char* ota_user = "admin";
const char* ota_pass = "geheim123";
const bool NO_WHATSAPP = true;

// --- ZEITSTEUERUNG ---
const int SLEEP_MINUTES = 2;   // Zentrales Intervall zwischen zwei Messungen
const int HEARTBEAT_DAYS = 7;  // Alle wie viele Tage soll ein Statusbericht kommen?

// --- E-Mail Zugangsdaten ---
const char* WIFI_SSID = "DEIN_WLAN_NAME";
const char* WIFI_PASSWORD = "DEIN_WLAN_PASSWORT";
const char* SMTP_HOST = "smtp.gmail.com";
const uint16_t SMTP_PORT = 465;
const char* IMAP_HOST = "imap.gmail.com";
const uint16_t IMAP_PORT = 993;
const char* AUTHOR_EMAIL = "deine-email@gmail.com";
const char* AUTHOR_PASSWORD = "dein-app-passwort";
const char* email_recipients[] = {
  "beispiel1@gmail.com",
  "beispiel2@web.de",
  "beispiel3@gmx.net"
};

// Objekte
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);
IMAPClient imap(ssl_client);

// Konfiguration WhatsApp
const char* whatsapp_recipients[] = {
  "491701111111",
  "491712222222",
  "491723333333"
};
String apiKey = "123456";

// ============================================================
// Pins für ESP32-C3 SuperMini (16-Pin-Board)
// ============================================================
// WICHTIG: Der C3 hat nur 13 nutzbare GPIOs (0-10, 20, 21).
// - Analog (ADC) geht NUR auf GPIO 0-4.
// - Deep-Sleep-Wakeup per GPIO geht NUR auf GPIO 0-5 (RTC-fähige Pins).
// - GPIO 2, 8, 9 sind Strapping-Pins (Boot-Modus) - nur mit Vorsicht nutzen.
//
// PIN_PUMP_CHECK und PIN_AKKU brauchen ADC  -> GPIO 0/1
// PIN_POWER_CHECK/BUTTON_DOWN/BUTTON_UP brauchen Wakeup -> GPIO 3/4/5
#define PIN_PUMP_CHECK   0   // ZMCT103C (ADC)
#define PIN_POWER_CHECK  3   // Versorgungsspannung ok = high, Wakeup-fähig
#define PIN_AKKU         1   // Batteriespannungsmessung (ADC)
#define PIN_BUTTON_DOWN  4   // Reset/OTA, Wakeup-fähig
#define PIN_BUTTON_UP    5   // Zusatz-Funktion, Wakeup-fähig

// Display - freie GPIOs, keine ADC/Wakeup-Anforderung
#define CS               6
#define DC               7
#define RST             10
#define BUSY             9   // Achtung: Strapping-Pin (BOOT-Taster), siehe Hinweis unten
#define PIN_EPD_SCK     20
#define PIN_EPD_MOSI    21
// GPIO 2 und GPIO 8 bleiben bewusst frei (beide Strapping-Pins)

// Display & Server Instanzen
GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT> display(GxEPD2_154_GDEY0154D67(CS, DC, RST, BUSY));
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
WebServer httpServer(80);
HTTPUpdateServer httpUpdater;

float batVoltage;

// RTC Speicher
RTC_DATA_ATTR bool alarmSperre = false;
RTC_DATA_ATTR bool geraetLiefZuvor = false;
RTC_DATA_ATTR int wakeupCounter = 0;
RTC_DATA_ATTR char lastSystemStart[20] = "Systemstart";

// --- FUNKTIONEN ---

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
}

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return true;

  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    setTime();
    return true;
  }

  updateDisplay("FEHLER", "Kein WLAN\nKein Internet", 247);
  return false;
}

void setTime() {
  if (connectWiFi()) {
    configTime(3600, 3600, "pool.ntp.org");

    // Warten, bis die interne RTC einen plausiblen Wert hat ( > Jahr 2020)
    time_t now = 0;
    int retry = 0;
    while (time(&now) < 1700000000 && retry < 10) {
      delay(500);
      retry++;
    }
    syncTime();
  }
}

void syncTime() {
  if (time(nullptr) < 1700000000) {
    setTime();
  }
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    strftime(lastSystemStart, sizeof(lastSystemStart), "%d.%m. %H:%M", &timeinfo);
    ESP_LOGD(TAG, "Zeit via NTP synchronisiert.");
  }
}

void sendWhatsApp(String msg) {
  if (NO_WHATSAPP) return;

  if (connectWiFi()) {
    HTTPClient http;
    msg.replace(" ", "+");

    int num_recipients = sizeof(whatsapp_recipients) / sizeof(whatsapp_recipients[0]);
    for (int i = 0; i < num_recipients; i++) {
      String url = "https://api.callmebot.com/whatsapp.php?phone=" + String(whatsapp_recipients[i]) + "&text=" + msg + "&apikey=" + apiKey;
      http.begin(url);
      int httpCode = http.GET();

      if (httpCode > 0) {
        ESP_LOGD("WHATSAPP", "WhatsApp an %s gesendet.\n", whatsapp_recipients[i]);
      } else {
        ESP_LOGE("WHATSAPP", "Fehler bei %s: %s\n", whatsapp_recipients[i], http.errorToString(httpCode).c_str());
      }
      http.end();

      // Kleiner Delay, damit die API nicht blockiert
      delay(500);
    }
  }
}

// --- SMTP: Senden ---
void sendStatusMail(String subject, String messageText, String logFilename = "", String logContent = "") {
  if (!connectWiFi()) return;

  ssl_client.setInsecure();

  auto statusCallback = [](SMTPStatus status) {
    ESP_LOGD("SEND_MAIL","Status: %s", status.text.c_str());
  };

  smtp.connect(SMTP_HOST, SMTP_PORT, statusCallback);

  if (smtp.isConnected()) {
    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);

    SMTPMessage msg;
    int num_email_recipients = sizeof(email_recipients) / sizeof(email_recipients[0]);

    for (int i = 0; i < num_email_recipients; i++) {
      msg.headers.add(rfc822_from, String("PumpMonitor <") + AUTHOR_EMAIL + ">");
      msg.headers.add(rfc822_to, email_recipients[i]);
      msg.headers.add(rfc822_subject, subject);
      msg.text.body(messageText);

      // add some cc and bcc recipient emails.
      // msg.headers.add(rfc822_cc, CC_EMAIL);
      // msg.headers.add(rfc822_bcc, BCC_EMAIL);

      msg.timestamp = time(nullptr);
      if (smtp.send(msg)) {
        ESP_LOGD("SEND_MAIL", "E-Mail erfolgreich an %s gesendet.\n", email_recipients[i]);
      }
      delay(500);
    }
  }
}

// --- IMAP: Lesen & Flag setzen ---
void dataCb(IMAPCallbackData& data) {
  // Showing envelope data.
  if (data.event() == imap_data_event_search || data.event() == imap_data_event_fetch_envelope) {
    // Show additional search info
    if (data.event() == imap_data_event_search)
      ReadyMail.printf("Showing Search result %d (%d) of %d from %d\n\n", data.messageIndex() + 1,
                       data.messageNum(), data.messageAvailable(), data.messageFound());

    // Headers data
    for (size_t i = 0; i < data.headerCount(); i++)
      ReadyMail.printf("%s: %s\n%s", data.getHeader(i).first.c_str(),
                       data.getHeader(i).second.c_str(), i == data.headerCount() - 1 ? "\n" : "");

    // Files data
    for (size_t i = 0; i < data.fileCount(); i++) {
      // You can fetch or download file while searching
      // (or disable it while fetching) with the following options.

      // To enable/disable this file for fetching.
      // data.fetchOption(i) = true;

      // To download if the fetch option is set.
      // data.setFileCallback(i, filecb, "/downloads");

      // To decode the text body based on the character set.
      // if (data.fileInfo(i).charset.length())
      //    data.setTextEncodingCallback(i, textDecodingCb);
      ReadyMail.printf("name: %s, mime: %s, charset: %s, trans-enc: %s, size: %d, fetch: %s%s\n",
                       data.fileInfo(i).filename.c_str(), data.fileInfo(i).mime.c_str(), data.fileInfo(i).charset.c_str(),
                       data.fileInfo(i).transferEncoding.c_str(), data.fileInfo(i).fileSize,
                       data.fetchOption(i) ? "yes" : "no", i == data.fileCount() - 1 ? "\n" : "");
    }
  } else if (data.event() == imap_data_event_fetch_body) {
    // Showing the text file content
    if (data.fileInfo().mime == "text/html" || data.fileInfo().mime == "text/plain") {
      if (data.fileChunk().index == 0)  // Fist chunk
        ESP_LOGV("READ_MAIL", "------------------");
      ESP_LOGV("READ_MAIL", "%s",(char*)data.fileChunk().data);
      if (data.fileChunk().isComplete)  // Last chunk
        ESP_LOGV("READ_MAIL", "------------------");
    } else {
      // Showing the progress of current file fetching
      if (data.fileChunk().index == 0)
        ESP_LOGV("READ_MAIL", "%s", "Downloading");
      if (data.fileProgress().available)
        ESP_LOGV("READ_MAIL", "%s", ".");
      if (data.fileChunk().isComplete)
        ESP_LOGV("READ_MAIL", " complete");
    }
  }
}

void checkIncomingMails() {
  if (!connectWiFi()) return;

  imap.connect(IMAP_HOST, IMAP_PORT);
  if (!imap.isConnected())
    return;

  imap.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);
  if (!imap.isAuthenticated())
    return;

  // List all mailboxes.
  imap.list();

  // Select INBOX mailbox.
  imap.select("INBOX");

  // Fetch the latest message in INBOX.
  imap.fetch(imap.getMailbox().msgCount, dataCb, NULL /* FileCallback */, true /*AWAIT_MODE*/, 1024 * 1024 /*MAX_CONTENT_SIZE*/);

  // To fetch message with UID, use imap.fetchUID instead.
}

float fetchBatteryVoltage() {
  int raw = 0;
  for (int i = 0; i < 20; i++) {
    raw += analogRead(PIN_AKKU);
    delay(2);
  }
  // ACHTUNG: Diese Formel war auf den Spannungsteiler/ADC des Wemos Battery
  // Shields abgestimmt. Beim Umbau auf den C3 SuperMini entfällt das Shield -
  // eigenen Spannungsteiler bauen und den Faktor hier neu kalibrieren
  // (z.B. mit Multimeter gegen die tatsächliche Akkuspannung abgleichen).
  batVoltage = (raw / 20.0 / 4095.0) * 3.3 * 2.0 * 1.1;
  ESP_LOGD(TAG, "Batteriespannung berechnet: %.2fV", batVoltage);
  return batVoltage;
}

bool checkBattery() {
  if (batVoltage < 3.3) {
    updateDisplay("AKKU LEER!", "System ist aus!", 90);
    sendWhatsApp("AKKU LEER! Dainagepumpen Monitor wird deaktiviert.");
    esp_deep_sleep_start();
  }
  return true;
}

void checkPumpActivity() {
  int mx = 0, mn = 4095;
  uint32_t start = millis();
  while (millis() - start < 200) {
    int v = analogRead(PIN_PUMP_CHECK);
    if (v > mx) mx = v;
    if (v < mn) mn = v;
  }

  int differenz = mx - mn;
  ESP_LOGD(TAG, "Gemessene Differenz: %d", differenz);

  if (differenz > PUMP_TRIGGER) {
    ESP_LOGI(TAG, "Aktivität erkannt: %d > %d", differenz, PUMP_TRIGGER);
    geraetLiefZuvor = true;
  }
}

void handleOTAMode() {
  updateDisplay("OTA", "Verbinde...", 84);
  if (connectWiFi()) {
    String ip = WiFi.localIP().toString();
    updateDisplay("OTA AN", "IP: " + ip + "\nUser: " + String(ota_user) + "\nPwd: " + String(ota_pass), 129);
    httpUpdater.setup(&httpServer, "/update", ota_user, ota_pass);
    httpServer.begin();
    while (true) {
      httpServer.handleClient();
      delay(1);
    }
  }
}

void resetAlarm() {
  alarmSperre = false;
  geraetLiefZuvor = false;
  updateDisplay("RESET", "System\nzurückgesetzt", 243);
}

void checkButton() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  // ESP32-C3 kennt kein EXT1 - der gemeinsame GPIO-Wakeup (Netz + beide
  // Taster) liefert stattdessen ESP_SLEEP_WAKEUP_GPIO.
  if (reason == ESP_SLEEP_WAKEUP_GPIO) {
    // Welcher Button wurde gedrückt? (Low-Aktiv)
    bool down = !digitalRead(PIN_BUTTON_DOWN);
    bool up = !digitalRead(PIN_BUTTON_UP);

    if (down) {
      ESP_LOGI(TAG, "Button 1 erkannt -> Starte Reset/OTA-Logik");
      uint32_t t = millis();
      bool longPress = true;
      while (millis() - t < 3000) {
        if (digitalRead(PIN_BUTTON_DOWN)) {
          longPress = false;
          break;
        }
      }
      if (longPress) {
        handleOTAMode();
      } else {
        resetAlarm();
      }
    }

    if (up) {
      ESP_LOGI(TAG, "Button 2 erkannt -> Aktion ausführen");
      updateDisplay("MANUELL", "Messung\nerzwungen", 120);
      // Hier könnte z.B. eine sofortige Messung erfolgen
    }
  }
}

void handleAlarms() {
  if (alarmSperre) return;

  bool netz = digitalRead(PIN_POWER_CHECK);
  if (!netz) {
    ESP_LOGW(TAG, "Stromausfall detektiert!");
    sendWhatsApp("ALARM: Stromausfall!");
    updateDisplay("STROM AUS", "Stromausfall!\nseit " + String(lastSystemStart) + "h", 170);
    alarmSperre = true;
  } else if (geraetLiefZuvor) {
    ESP_LOGW(TAG, "Geraetaktivitaet erkannt!");
    sendWhatsApp("ALARM: Geraet war an!");
    updateDisplay("PUMPE EIN", "Pumpe läuft\nseit " + String(lastSystemStart) + "h", 121);
    alarmSperre = true;
  }
}

void handleStatusReport() {
  int cyclesRequired = (HEARTBEAT_DAYS * 24 * 60) / SLEEP_MINUTES;

  if (wakeupCounter >= cyclesRequired) {
    if (connectWiFi()) {
      ESP_LOGD(TAG, "Sende Statusbericht");
      String message = "Statusbericht vom " + String(lastSystemStart);

      sendStatusMail("Statusbericht Drainagepumpe", message);
      updateDisplay("SYSTEM OK", "Statusbericht\ngesendet\nam" + String(lastSystemStart) + "h", 120);
      wakeupCounter = 0;
    }
  }
}

void startDeepSleep() {
  wakeupCounter++;
  uint64_t sleepTimeMicros = (uint64_t)SLEEP_MINUTES * 60 * 1000000ULL;

  esp_sleep_enable_timer_wakeup(sleepTimeMicros); // Zeit-Intervall

  // --- GPIO-Wakeup (ersetzt EXT0 + EXT1, die der C3 nicht hat) ---
  // Der ESP32-C3 kennt weder ext0- noch ext1-Wakeup wie der klassische
  // ESP32. Stattdessen gibt es nur "GPIO Wakeup" für den Deep Sleep, und
  // der C3 unterstützt maximal zwei Wakeup-Quellen gleichzeitig
  // (hier: Timer + eine gemeinsame GPIO-Maske).
  //
  // Da PIN_POWER_CHECK, PIN_BUTTON_DOWN und PIN_BUTTON_UP alle aktiv-LOW
  // sind, können sie in EINER Maske zusammengefasst werden: das System
  // wacht auf, sobald IRGENDEINER dieser Pins auf LOW geht.
  uint64_t wakeupMask = (1ULL << PIN_POWER_CHECK) | (1ULL << PIN_BUTTON_DOWN) | (1ULL << PIN_BUTTON_UP);
  esp_deep_sleep_enable_gpio_wakeup(wakeupMask, ESP_GPIO_WAKEUP_GPIO_LOW);

  ESP_LOGD(TAG, "Deep Sleep gestartet für %d Min.", SLEEP_MINUTES);
  esp_deep_sleep_start();
}

// --- HAUPTPROGRAMM ---

void setup() {
  Serial.begin(115200);

  // Hardware-SPI auf die gewählten Display-Pins legen (der C3 hat keine
  // fest verdrahteten Standard-SPI-Pins wie 18/23 beim klassischen ESP32).
  SPI.begin(PIN_EPD_SCK, -1 /* MISO wird vom Display nicht benötigt */, PIN_EPD_MOSI, CS);

  display.init(115200, true, 10, false); 
  u8g2Fonts.begin(display); 
  display.setRotation(0);

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_GPIO) {
    ESP_LOGI(TAG, "Wakeup: GPIO (Netz/Button)");
  } else if (reason == ESP_SLEEP_WAKEUP_TIMER) {
    ESP_LOGI(TAG, "Wakeup: TIMER (Intervall %d)", wakeupCounter);
  }

  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_POWER_CHECK, INPUT); // Interner Pullup wäre auf GPIO3 zwar möglich, aber wie im Original bewusst extern gelöst
  syncTime();

  fetchBatteryVoltage();
  checkBattery();
  checkPumpActivity();

  checkButton();

  handleAlarms();
  handleStatusReport();
  // updateDisplay("SYSTEM OK", "Letzte Prüfung\nam 22.03. 10:00h", 120);
  startDeepSleep();
}

void loop() {}
