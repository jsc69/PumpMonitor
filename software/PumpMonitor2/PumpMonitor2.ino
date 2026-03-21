#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoOTA.h>
#include <GxEPD2_BW.h>  // Library Library GxEPD2
#include <Fonts/FreeSansBold9pt7b.h>
#include <time.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>

#include <WiFiClientSecure.h>
#define ENABLE_IMAP
#define ENABLE_SMTP
#include <ReadyMail.h>

// --- KONFIGURATION ---
const int PUMP_TRIGGER = 1500; // => Pumpe AUS + (Pumpen AN - Pumpe AUS)/2 => 200 + (2800-200)/2 = 1500

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
const char* RECIPIENT_EMAIL = "deine-email@gmail.com";

// Objekte
WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);
IMAPClient imap(ssl_client);

String phone = "4912345678";
String apiKey = "123456";

// Pins für Wemos D1 Mini ESP32 & Battery Shield
#define PIN_NETZ_CHECK 14
#define PIN_STROM 34
#define PIN_AKKU 35
#define PIN_RESET 15

// Display & Server Instanzen
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(5, 17, 16, 4));
WebServer httpServer(80);
HTTPUpdateServer httpUpdater;

float batVoltage;

// RTC Speicher
RTC_DATA_ATTR bool alarmSperre = false;
RTC_DATA_ATTR bool geraetLiefZuvor = false;
RTC_DATA_ATTR int wakeupCounter = 0;
RTC_DATA_ATTR char lastSystemStart[20] = "Systemstart";

// --- FUNKTIONEN ---

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

  updateDisplay("FEHLER", "Keine Verbindung zum Internet!");
  return false;
}

void updateDisplay(String header, String detail) {
  display.init(115200);
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(0, 25);
    display.println(header);
    display.drawLine(0, 35, 200, 35, GxEPD_BLACK);
    display.setFont();
    display.setCursor(0, 60);
    display.println(detail);
    display.setCursor(0, 140);
    display.print("Akku: ");
    display.print(batVoltage);
    display.print("V");
  } while (display.nextPage());
  display.hibernate();
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
      Serial.println("Zeit via NTP synchronisiert.");
    }
}

void sendWhatsApp(String msg) {
  if (NO_WHATSAPP) return;

  if (connectWiFi()) {
    HTTPClient http;
    msg.replace(" ", "+");
    http.begin("https://api.callmebot.com/whatsapp.php?phone=" + phone + "&text=" + msg + "&apikey=" + apiKey);
    http.GET();
    http.end();
  }
}

// --- SMTP: Senden ---
void sendStatusMail(String subject, String messageText, String logFilename = "", String logContent = "") {
  if (!connectWiFi()) return;

  ssl_client.setInsecure();

  auto statusCallback = [](SMTPStatus status) {
    Serial.println(status.text);
  };

  smtp.connect(SMTP_HOST, SMTP_PORT, statusCallback);

  if (smtp.isConnected()) {
    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password);

    SMTPMessage msg;
    msg.headers.add(rfc822_from, String("PumpMonitor <") + AUTHOR_EMAIL + ">");
    msg.headers.add(rfc822_to, String("Admin <") + RECIPIENT_EMAIL + ">");
    msg.headers.add(rfc822_subject, subject);
    msg.text.body(messageText);

    // add some cc and bcc recipient emails.
    // msg.headers.add(rfc822_cc, CC_EMAIL);
    // msg.headers.add(rfc822_bcc, BCC_EMAIL);

    msg.timestamp = time(nullptr);
    smtp.send(msg);
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
        Serial.println("------------------");
      Serial.print((char*)data.fileChunk().data);
      if (data.fileChunk().isComplete)  // Last chunk
        Serial.println("------------------");
    } else {
      // Showing the progress of current file fetching
      if (data.fileChunk().index == 0)
        Serial.print("Downloading");
      if (data.fileProgress().available)
        Serial.print(".");
      if (data.fileChunk().isComplete)
        Serial.println(" complete");
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
  for (int i = 0; i < 15; i++) {
    raw += analogRead(PIN_AKKU);
    delay(2);
  }
  batVoltage = (raw / 15.0 / 4095.0) * 3.3 * 2.0 * 1.1;
}

bool checkBattery() {
  if (batVoltage < 3.3) {
    updateDisplay("AKKU LEER!", "System schlaeft.");
    sendWhatsApp("AKKU LEER! Dainagepumpen Monitor wird deaktiviert.");
    esp_sleep_enable_ext1_wakeup((1ULL << PIN_RESET), ESP_EXT1_WAKEUP_ALL_LOW);
    esp_deep_sleep_start();
  }
  return true;
}

void checkPumpActivity() {
  int mx = 0, mn = 4095;
  uint32_t start = millis();
  while (millis() - start < 100) {
    int v = analogRead(PIN_STROM);
    if (v > mx) mx = v;
    if (v < mn) mn = v;
  }
  
  int differenz = mx - mn;
  Serial.print("Gemessene Differenz: ");
  Serial.println(differenz);

  if (differenz > PUMP_TRIGGER) {
    geraetLiefZuvor = true;
  }
}

void handleOTAMode() {
  updateDisplay("OTA MODUS", "Verbinde WLAN...");
  if (connectWiFi()) {
    String ip = WiFi.localIP().toString();
    updateDisplay("OTA BEREIT", "URL: http://" + ip + "/update\nUser: " + String(ota_user));
    httpUpdater.setup(&httpServer, "/update", ota_user, ota_pass);
    httpServer.begin();
    while (true) {
      httpServer.handleClient();
      delay(1);
    }
  }
}

void checkButton() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_EXT1) {
    uint32_t t = millis();
    bool longPress = true;
    while (millis() - t < 3000) {
      if (digitalRead(PIN_RESET)) {
        longPress = false;
        break;
      }
    }
    if (longPress) {
      handleOTAMode();
    } else {
      alarmSperre = false;
      geraetLiefZuvor = false;
      updateDisplay("SYSTEM RESET", "Zaehler auf 0 gesetzt.");
    }
  }
}

void handleAlarms() {
  if (alarmSperre) return;

  bool netz = digitalRead(PIN_NETZ_CHECK);
  if (!netz) {
    sendWhatsApp("ALARM: Stromausfall!");
    updateDisplay("STROM AUSFALL!", "Seit: " + String(lastSystemStart));
    alarmSperre = true;
  } else if (geraetLiefZuvor) {
    sendWhatsApp("ALARM: Geraet war an!");
    updateDisplay("GERAET LIEF!", "Seit: " + String(lastSystemStart));
    alarmSperre = true;
  }
}

void handleStatusReport() {
  int cyclesRequired = (HEARTBEAT_DAYS * 24 * 60) / SLEEP_MINUTES;

  if (wakeupCounter >= cyclesRequired) {
    if (connectWiFi()) {
      String message = "Statusbericht vom " + String(lastSystemStart);

      sendStatusMail("Statusbericht Drainagepumpe", message);
      updateDisplay("SYSTEM OK", message);
      wakeupCounter = 0;
    }
  }
}

void startDeepSleep() {
  wakeupCounter++;

  // Konvertierung von Minuten in Mikrosekunden für den ESP32 Timer
  uint64_t sleepTimeMicros = (uint64_t)SLEEP_MINUTES * 60 * 1000000ULL;

  esp_sleep_enable_timer_wakeup(sleepTimeMicros);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_NETZ_CHECK, 0);
  esp_sleep_enable_ext1_wakeup((1ULL << PIN_RESET), ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
}

// --- HAUPTPROGRAMM ---

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RESET, INPUT_PULLUP);
  pinMode(PIN_NETZ_CHECK, INPUT);
  syncTime();
  
  fetchBatteryVoltage();
  checkBattery();
  checkPumpActivity();

  checkButton();

  handleAlarms();
  handleStatusReport();
  startDeepSleep();
}

void loop() {}