# ESP32-C3 PumpMonitor

Ein batteriebetriebener Pumpenwächter auf Basis eines **ESP32-C3 SuperMini** (16-Pin-Board) mit E-Paper-Display, Stromsensor (ZMCT103C), Wakeup-Tasten, WiFi-OTA und Benachrichtigungen via **WhatsApp (CallMeBot)** und **E-Mail (SMTP/IMAP)**.

Das Gerät wacht zyklisch aus dem Deep-Sleep auf, prüft Akkuspannung, Stromaufnahme an der Pumpe sowie Netzausfall, meldet Alarme und geht sofort wieder schlafen, um den Akku zu schonen.

---

## Inhaltsverzeichnis

1. [Architektur](#architektur)
2. [Pinning](#pinning)
3. [Installation](#installation)
4. [Konfiguration – credentials.h](#konfiguration--credentialsh)
5. [Betriebsmodi](#betriebsmodi)
6. [OTA-Update via Weboberfläche](#ota-update-via-weboberfläche)
7. [Kalibrierung](#kalibrierung)
8. [Akkumanagement](#akkumanagement)
9. [Debugging](#debugging)
10. [Bekannte Fehler / Quirks](#bekannte-fehler--quirks)
11. [Ressourcen](#ressourcen)
12. [Todo](#todo)

---

## Architektur

### Überblick

```
┌──────────────────────────────────────────────────────────┐
│                  ESP32-C3 SuperMini                      │
│                                                          │
│   ┌──────────┐    ┌────────────┐    ┌───────────────┐    │
│   │ ZMCT103C │───▶│ ADC1_CH0   │    │  E-Paper      │    │
│   │ (Strom)  │    │  GPIO 0    │    │  GxEPD2 BW    │    │
│   └──────────┘    └────────────┘    │  SPI (HSPI)   │    │
│                                     └───────────────┘    │
│   ┌──────────┐    ┌────────────┐                         │
│   │ Akku /   │───▶│ ADC1_CH1   │    ┌───────────────┐    │
│   │ Spannungs│    │  GPIO 1    │    │  Status-LED   │    │
│   │ teiler   │    └────────────┘    │  GPIO 8       │    │
│   └──────────┘                      └───────────────┘    │
│                                                          │
│   ┌──────────┐    ┌────────────┐                         │
│   │ Taster   │───▶│ GPIO 4     │   (Wakeup, lang=OTA)    │
│   │ DOWN     │    │ ext0       │                         │
│   └──────────┘    └────────────┘                         │
│                                                          │
│   ┌──────────┐    ┌────────────┐                         │
│   │ Taster   │───▶│ GPIO 5     │   (Wakeup, MESSUNG)     │
│   │ UP       │    │ ext1       │                         │
│   └──────────┘    └────────────┘                         │
└──────────────────────────────────────────────────────────┘
        │                                │
        ▼                                ▼
   WiFi (2,4 GHz)                Akku (Li-Ion/LiPo)
   - HTTPUpdateServer            Spannungsteiler 1:1
   - WhatsApp CallMeBot
   - SMTP/IMAP (Gmail)
```

### Software-Komponenten

| Komponente        | Library              | Zweck                                     |
|-------------------|----------------------|-------------------------------------------|
| Display           | `GxEPD2_BW`          | E-Paper-Rendering (200×200, 1 Bit)        |
| Display-Fonts     | `U8g2_for_Adafruit_GFX` | TrueType-Fonts über GxEPD2              |
| Webserver         | `WebServer`          | HTTP-Request-Handler                      |
| OTA               | `HTTPUpdateServer`   | Firmware-Upload via Browser               |
| Sleep             | `esp_sleep.h`        | Deep-Sleep + GPIO-Wakeup (`ext0/ext1`)    |
| E-Mail            | `ReadyMail`          | SMTP-Versand und IMAP-Empfang (TLS)       |
| WhatsApp          | `HTTPClient`         | GET-Aufruf gegen CallMeBot-API            |

### Programmfluss (`setup()`)

1. **Serial** starten (115200 Baud, nur Debug).
2. **Wakeup-Ursache** ermitteln (`ESP_SLEEP_WAKEUP_GPIO` oder Timer).
3. **Pin-Initialisierung** (Display, Buttons, LED).
4. **Display** initialisieren (`init(115200, true, 10, false)`).
5. **Zeit** synchronisieren (NTP via `pool.ntp.org`, Zeitzone `CET-1CEST`).
6. **Tasten** auswerten (`checkButton`) → ggf. OTA, MESSUNG oder Alarm-Reset.
7. **Akkuspannung** messen und prüfen (`checkBattery`).
8. **Pumpenaktivität** prüfen (`checkPumpActivity`).
9. **Statusbericht** senden, wenn Wakeup-Zähler `cyclesRequired` erreicht.
10. **Alarme** auslösen (`handleAlarms`) – Stromausfall / Pumpe lief.
11. **Display-Update** „OK“ bzw. Fehlerbild.
12. **Deep-Sleep** starten (`startDeepSleep`).

`loop()` ist leer – die gesamte Logik läuft einmalig in `setup()` und das System schläft anschließend wieder.

### RTC-Memory (`RTC_DATA_ATTR`)

Folgende Variablen überleben den Deep-Sleep ohne Flash-Schreibzugriff:

- `alarmSperre` – verhindert mehrfaches Auslösen eines Stromausfall-/Pumpen-Alarms.
- `geraetLiefZuvor` – Marker „Pumpe war im letzten Zyklus aktiv“.
- `wakeupCounter` – Zähler für zyklische Status-E-Mails.
- `lastSystemStart[20]` – letzter RTC-Zeitstempel als Boot-Grund.

---

## Pinning

### ESP32-C3 SuperMini – 16-Pin-Board

> **Wichtig:** Die ESP32-C3-Serie hat ADC nur auf **GPIO 0–4**. Deshalb sind `PIN_PUMP_CHECK` und `PIN_AKKU` fix auf GPIO 0/1 gelegt. Wakeup-fähige GPIOs für Deep-Sleep-`ext0/ext1`: GPIO 0–5.

### Pinbelegung

| GPIO | Funktion             | Richtung | Bemerkung                                 |
|------|----------------------|----------|-------------------------------------------|
| 0    | `PIN_PUMP_CHECK`     | Analog In | ZMCT103C-Stromsensor (ADC1_CH0)           |
| 1    | `PIN_AKKU`           | Analog In | Akku-Spannungsteiler (ADC1_CH1)           |
| 2    | `PIN_POWER_CHECK`    | Digital In | Netzausfall-Erkennung (5 V Versorgung)  |
| 3    | reserviert           | –        | –                                         |
| 4    | `PIN_BUTTON_DOWN`    | Digital In, `INPUT_PULLUP`, ext0-Wakeup | Kurzer Druck → Alarm-Reset, **langer Druck (≥ 3 s) → OTA** |
| 5    | `PIN_BUTTON_UP`      | Digital In, `INPUT_PULLUP`, ext1-Wakeup | MESSUNG-Modus aktivieren |
| 6    | `PIN_DIS_CS`         | Output   | E-Paper SPI Chip-Select                   |
| 7    | `PIN_DIS_DC`         | Output   | E-Paper Data/Command                      |
| 8    | `PIN_LED`            | Output   | Status-LED                                |
| 9    | `PIN_DIS_BUSY`       | Input    | E-Paper Busy-Signal                       |
| 10   | `PIN_DIS_RST`        | Output   | E-Paper Reset                             |
| 20   | `PIN_DIS_SCK`        | Output   | E-Paper SPI Clock (HSPI)                  |
| 21   | `PIN_DIS_MOSI`       | Output   | E-Paper SPI MOSI (HSPI)                   |

### Spannungsteiler Akku

```
  V_BAT ── R1 (100 kΩ) ──┬── GPIO 1
                         │
                         R2 (100 kΩ)
                         │
                        GND
```

- Teilerfaktor im Code: `* 2.0` (R1 = R2).
- ADC-Referenz: 3,3 V (intern).
- Korrekturfaktor: `* 1.1` (Eichfaktor, siehe [Kalibrierung](#kalibrierung)).

---

## Installation

### Voraussetzungen

- **Arduino IDE** ≥ 2.x oder **PlatformIO**
- **ESP32-C3**-Board-Paket (`esp32` von Espressif, Version ≥ 2.0.13)
- USB-C-Kabel (ESP32-C3 SuperMini besitzt native USB-Serial)

### Benötigte Libraries (über den Library-Manager)

| Library                | Zweck                                 |
|------------------------|---------------------------------------|
| `GxEPD2`               | E-Paper-Display-Treiber               |
| `U8g2_for_Adafruit_GFX`| Schriftarten                          |
| `ReadyMail`            | SMTP/IMAP (kein separates Auth-Lib)   |
| `ArduinoOTA`           | OTA-Framework (in ESP32-Core)         |
| `HTTPUpdateServer`     | OTA über Webserver (in ESP32-Core)    |

### Arduino IDE – Schritte

1. **Datei → Voreinstellungen → Zusätzliche Boardverwalter-URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. **Werkzeuge → Board → ESP32 Arduino → ESP32C3 Dev Module** auswählen.
3. **Boardeinstellungen**:
   - USB CDC On Boot: **Enabled**
   - Flash Size: **4 MB (32 Mb)**
   - Partition Scheme: **Default 4 MB with spiffs**
   - Upload Speed: **921600**
4. Sketch `PumpMonitor_SuperMini.ino` und `credentials.h` in **denselben Ordner** legen.
5. **Hochladen**.

### PlatformIO – alternative `platformio.ini`

```ini
[env:esp32c3]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
lib_deps =
    zinggjm/GxEPD2
    moononournation/GFX Library for Arduino
    mo-thunder/ReadyMail
```

---

## Konfiguration – credentials.h

Die Datei `credentials.h` liegt im selben Verzeichnis wie das `.ino`-Sketch und wird per `#include "credentials.h"` eingebunden. Sie **darf nicht ins Versionskontrollsystem** eingecheckt werden.

### Vorlage

```c
#pragma once
// --- WIFI ---
const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* ota_user = "OTA_USER";
const char* ota_pass = "OTA_PASSWORD";

// --- E-Mail Zugangsdaten ---
const char* SMTP_HOST = "smtp.gmail.com";
const uint16_t SMTP_PORT = 465;
const char* IMAP_HOST = "imap.gmail.com";
const uint16_t IMAP_PORT = 993;
const char* AUTHOR_EMAIL = "nobody@gmail.com";
const char* AUTHOR_PASSWORD = "dein-app-passwort";
const char* email_recipients[] = {
  "somebody@gmail.com"
};

// Konfiguration WhatsApp
const int num_recipients = 2;
const char* whatsapp_recipients[num_recipients][2] = {
  {"+49176xxxxxxxx","xxxxxxx"},
  {"+49176xxxxxxxx","xxxxxxx"}
};
```

### Felder erklärt

| Variable                              | Beschreibung                                          |
|---------------------------------------|-------------------------------------------------------|
| `ssid`, `password`                    | WLAN-Zugangsdaten (2,4 GHz)                           |
| `ota_user`, `ota_pass`                | Basic-Auth-Zugang für die OTA-Weboberfläche           |
| `SMTP_HOST`, `SMTP_PORT`              | SMTP-Server für ausgehende Alarm-E-Mails              |
| `IMAP_HOST`, `IMAP_PORT`              | IMAP-Server zum Lesen neuer E-Mails                   |
| `AUTHOR_EMAIL`, `AUTHOR_PASSWORD`     | App-Passwort (z. B. Gmail App-Password, **kein** Klartext-Login) |
| `email_recipients[]`                  | Array der Empfänger-Adressen für Alarm-E-Mails        |
| `num_recipients`                      | Anzahl der WhatsApp-Empfänger                         |
| `whatsapp_recipients[][]`             | Paare aus `{Telefonnummer mit Ländervorwahl, CallMeBot-API-Key}` |

### CallMeBot-API-Key besorgen

1. Die Nummer **+34 644 71 81 99** in den eigenen Kontakten speichern.
2. Über WhatsApp die Nachricht `I allow callmebot to send me messages` senden.
3. Die automatische Antwort enthält den persönlichen `apikey`.

### Gmail App-Passwort

1. 2-Faktor-Authentifizierung im Google-Konto aktivieren.
2. Sicherheit → App-Passwörter → „Mail / Other“ generieren.
3. Das 16-stellige Passwort in `AUTHOR_PASSWORD` eintragen.

### Globale Schalter im Sketch

Oben in `PumpMonitor_SuperMini.ino` lassen sich Benachrichtigungskanäle abschalten:

```cpp
bool NO_WHATSAPP = false;   // true = keine WhatsApp-Alarme
bool NO_EMAIL    = false;   // true = keine SMTP-Alarme
```

Sowie ein Debug-Schalter am Dateianfang:

```cpp
//#define USB // Aktiviert: Stromausfalltest wird DEAKTIVIERT (Netz gilt immer als vorhanden)
```

---

## Betriebsmodi

Das Gerät arbeitet ausschließlich aus dem Deep-Sleep heraus. Beim Aufwachen entscheidet die Tasten-Erkennung über den Modus.

### 1. Normaler Wachzyklus

* **Auslöser:** Timer-Wakeup (Standard).
* **Ablauf:** Akku messen → Strom messen → ggf. Status-E-Mail → Deep-Sleep.
* **Anzeige:** „OK“ + Akkuspannung + letzte Systemstartzeit.

### 2. OTA-Modus

* **Auslöser:** `PIN_BUTTON_DOWN` **≥ 3 Sekunden** gedrückt halten (long-press).
* **Anzeige:** „OTA / Verbinde…“ → nach WiFi-Verbindung IP-Adresse und Hinweis.
* **Verhalten:** Startet `HTTPUpdateServer`, IP wird angezeigt.
* **Abbruch:** Erneuter Druck auf `DOWN` oder Timeout (`OTA_TIMEOUT = 300000 ms` = **5 Minuten**).
* **Nach Abbruch:** „ABORT / OTA beendet“ → 2 s Anzeige → Deep-Sleep.

### 3. Messungs-Modus

* **Auslöser:** `PIN_BUTTON_UP` (Wakeup-Pin).
* **Ablauf:** Endlosschleife mit Display-Refresh jede Sekunde.
* **Beenden:** Druck auf `DOWN` → sofortiger Deep-Sleep.
* **Zweck:** Anzeige vor Ort justieren / Pumpe manuell triggern.

### 4. Alarm-Modus

* **Auslöser:** Erkennung im Normalzyklus.
* **Quellen:**
  - `PIN_POWER_CHECK = LOW` → Stromausfall → WhatsApp + E-Mail
  - `geraetLiefZuvor == true` → Pumpe war im letzten Zyklus aktiv → WhatsApp + E-Mail
* **Schutz:** `alarmSperre` verhindert Re-Trigger.
* **Reset:** Kurzer Druck auf `DOWN` setzt `alarmSperre` und `geraetLiefZuvor` zurück.

### 5. Akku-leer-Modus

* **Auslöser:** `batVoltage < 3.3 V` (Schwelle in `checkBattery()`).
* **Verhalten:** Display zeigt „AKKU LEER! / System ist aus!“, dann sofortiger Deep-Sleep. Keine WiFi-Verbindung → keine Alarm-E-Mails.

---

## OTA-Update via Weboberfläche

> **Wichtig:** Updates erfolgen ausschließlich über die **HTML-Weboberfläche** im Browser (z. B. Firefox/Chrome) – **nicht** via `curl` oder Python-Skript. Das ist die vom `HTTPUpdateServer` vorgesehene und getestete Vorgehensweise.

### Voraussetzungen

1. Gerät befindet sich im **OTA-Modus** (langer Druck auf `DOWN`).
2. Display zeigt die zugewiesene **IP-Adresse**.
3. PC/Smartphone befindet sich im **gleichen WLAN** wie das Gerät.

### Schritt-für-Schritt im Browser

1. Browser öffnen und folgende URL aufrufen:
   ```
   http://<IP-aus-Display>/
   ```
   z. B. `http://192.168.1.42/`
2. Die Startseite des `HTTPUpdateServer` erscheint.
3. Benutzername und Passwort eingeben:
   - `ota_user` und `ota_pass` aus `credentials.h`
4. Auf den Link **„Firmware“** (oder „Update“) klicken.
5. Die kompilierte Firmware-Datei (`PumpMonitor_SuperMini.ino.esp32c3.bin` oder gleichnamige `.bin`) auswählen.
6. Schaltfläche **„Update“** (bzw. „Upload“) drücken.
7. Der Browser zeigt Fortschritt und Erfolgsmeldung.
8. Das Gerät startet automatisch mit der neuen Firmware neu.

### Während des Updates

- **Display blinkt** ggf. mit Status-Texten.
- **Stromversorgung nicht unterbrechen.**
- Innerhalb der **5 Minuten Timeout** abschließen, sonst Deep-Sleep.
- **DOWN-Taste** drücken → Abbruch und sofortiger Deep-Sleep.

### Mehrere Geräte

Pro Gerät eine **eigene `credentials.h`** (verschiedene `ssid`/`password` und `ota_user`/`ota_pass`) einsetzen. Andernfalls kollidieren mehrere gleichzeitig lauschende OTA-Server im gleichen WLAN.

---

## Kalibrierung

### Akku-Spannungsmessung

Funktion `fetchBatteryVoltage()`:

```cpp
int raw = 0;
for (int i = 0; i < 20; i++) {
  raw += analogRead(PIN_AKKU);
  delay(2);
}
batVoltage = (raw / 20.0 / 4095.0) * 3.3 * 2.0 * 1.1;
```

**Schritte zur Kalibrierung:**

1. Ein **Labornetzteil** oder ein **Multimeter mit bekannter Spannung** an den Akku-Eingang anklemmen (gleicher Spannungsteiler wie im Aufbau).
2. Den angezeigten Wert im Display bzw. via `Serial.println(batVoltage)` ablesen.
3. Den Korrekturfaktor `1.1` so anpassen, dass die berechnete Spannung mit der Referenzspannung übereinstimmt (z. B. `1.08`, `1.12` …).
4. Schwelle in `checkBattery()` (`< 3.3`) je nach Akku-Chemie (Li-Ion ≈ 3.0–3.3 V, LiFePO4 ≈ 2.8–3.2 V) anpassen.

### Pumpen-Strommessung (ZMCT103C)

Funktion `messurePumpCurrent()` misst den **Spitze-zu-Spitze-Wert** über 250 ms.

**Vorgehensweise:**

1. Pumpe **normal** betreiben.
2. Auf dem Display den Modus „MESSUNG“ starten (`UP`-Taste).
3. Über `Serial` den `peakToPeak`-Wert loggen (z. B. mit `ESP_LOGD` oder zusätzlichem `Serial.print` im Code).
4. Pumpe **ausschalten** und Hintergrund-Rauschpegel notieren.
5. Schwelle `PUMP_TRIGGER` so wählen, dass sie **etwa 20 % über** dem Ruhewert liegt.

> Hinweis: Der genaue Name der Konstante `PUMP_TRIGGER` und `cyclesRequired` ist im Sketch zu prüfen, da sie nicht im vorliegenden Auszug enthalten sind.

---

## Akkumanagement

### Verbrauch

- **Deep-Sleep:** ca. 40–60 µA (ESP32-C3 ohne externe Peripherie).
- **Wake + WiFi:** Spitze ca. 160 mA für wenige Sekunden.
- **Display-Refresh:** ca. 25 mA für ca. 2 s pro Wakeup.

Durch den kurzen Wakeup und die sofortige Rückkehr in den Deep-Sleep ist eine Akkulaufzeit von **mehreren Monaten** realistisch.

### Ladezustand-Schwellen

| Spannung  | Bedeutung                                       |
|-----------|--------------------------------------------------|
| > 4.0 V   | Voll (nur bei USB-/Solar-Ladung sinnvoll)        |
| 3.5–3.9 V | Normalbetrieb                                    |
| 3.3–3.5 V | Akku wird knapp                                  |
| < 3.3 V   | **System-Stopp** (Tiefentladungsschutz)          |

### Aufwachen optimieren

- `WiFi.disconnect()` wird im OTA-Abbruch aufgerufen – im **Normalzyklus** muss darauf geachtet werden, dass die Verbindung sauber beendet wird (passiert über `esp_sleep_start`, nach dem die WiFi-Schnittstelle ohnehin schläft).
- `WiFi.begin(ssid, password)` mit max. 20 Retries à 500 ms = max. 10 s Blockade.
- `setTime()` benötigt zusätzliche bis zu 10 × 500 ms für NTP-Sync.

---

## Debugging

### Serieller Monitor

- Baud-Rate: **115200**.
- Log-Level über `esp_log_level_set("PUMP_MONITOR", ESP_LOG_DEBUG)` zur Laufzeit anpassen.
- Wichtige Tags: `"PUMP_MONITOR"`, `"READ_MAIL"`.

### Logging-Strategie

| Level  | Verwendung im Sketch                               |
|--------|----------------------------------------------------|
| INFO   | Wakeup-Quelle, OTA-Start/Abbruch                   |
| WARN   | Stromausfall erkannt, Pumpe war aktiv              |
| DEBUG  | Statusbericht, NTP-Sync                            |
| VERBOSE| IMAP-Detail-Output (Header, Anhänge)               |

### Häufige Diagnosefälle

| Symptom                                  | Ursache / Lösung                                    |
|------------------------------------------|-----------------------------------------------------|
| Display bleibt weiß                       | SPI-Verkabelung prüfen, `BUSY`-Pegel im Reset beachten |
| Keine Verbindung zu WiFi                  | Falsche Zugangsdaten in `credentials.h`             |
| OTA-Seite nicht erreichbar               | PC nicht im gleichen WLAN, IP-Adresse vom Display ablesen |
| Keine WhatsApp-Nachricht                 | CallMeBot-API-Key ungültig oder Nummer nicht registriert |
| E-Mail-Versand scheitert                 | App-Passwort statt Login-Passwort verwenden        |
| Akku-Spannung unrealistisch              | Spannungsteiler-Werte / Korrekturfaktor `1.1`      |
| `wakeupCounter` zählt nicht hoch          | RTC-Memory wird durch Brown-out gelöscht → Akku prüfen |

### Compile-Time-Analyse

- Während der Entwicklung `//#define USB` aktivieren, um den Stromausfall-Pfad im Labor zu simulieren.
- Mit `NO_WHATSAPP = true` und `NO_EMAIL = true` lässt sich der reine Messbetrieb ohne externe Abhängigkeiten testen.

---

## Bekannte Fehler / Quirks

1. **`millis() % 1000 == 0` in `handleMeasurement()`**
   * Dieser Vergleich ist sehr unzuverlässig, da `millis()` nur an Vielfachen von 1000 *exakt* Null ergibt. In der Praxis aktualisiert sich das Display dadurch **meistens nie oder nur sporadisch**. Empfehlung: gegen einen separaten Timer-Zähler (`uint32_t last = 0; if (millis() - last > 1000) {...}`) ersetzen.

2. **„Unendlich“-Schleife in `handleMeasurement()`**
   * Die Schleife wird nur durch `digitalRead(PIN_BUTTON_DOWN)` verlassen. Sollte der Taster prellen oder ein anderer Wakeup-Pin betätigt werden, bleibt das Gerät **ewig wach** und entlädt den Akku in Minuten.

3. **`while(!digitalRead(PIN_BUTTON_UP)) {}` in `handleMeasurement()`**
   * Wartet blockierend auf das Loslassen des Tasters. Während dieser Zeit sind keine anderen Tasks möglich – das Gerät ist „eingefroren“.

4. **`PIN_POWER_CHECK` ohne `INPUT_PULLUP`**
   * Der Pin wird mit `pinMode(PIN_POWER_CHECK, INPUT)` initialisiert. Wenn die Beschaltung den Pegel im Ruhezustand nicht aktiv hält, kann es zu Floating und Fehlalarmen kommen. Empfehlung: externer Pull-up/down oder `INPUT_PULLDOWN` mit Pullup am Versorgungs-Signal.

5. **`alarmSperre`-Reset nur über Taster**
   * Nach einem Stromausfall wird der Alarm **nicht automatisch zurückgesetzt**, sobald Netz wieder vorhanden ist. Der Nutzer muss die `DOWN`-Taste drücken.

6. **Fehlende Definitionen im Auszug**
   * `PUMP_TRIGGER`, `cyclesRequired`, `PIN_BUTTON_UP`, `PIN_POWER_CHECK`, `num_email_recipients`, `sendStatusReport`, `time_smtp`, `setTime` vollständig etc. sind im vorliegenden Code-Auszug nicht vollständig sichtbar – sie müssen im realen Sketch ergänzt werden.

7. **`connectWiFi()` ohne SSID-Validierung**
   * Wenn `ssid == ""` oder `password == ""` ist, ruft `WiFi.begin` die Standard-SSID auf, was zu ungewollten Verbindungen führt. Vor jedem Deployment prüfen!

8. **NTP-Retry blockiert bis zu 5 s ohne Feedback**
   * Bei schlechter WLAN-Verbindung hängt der Setup-Vorgang bis zu 10 × 500 ms ohne Status-Update auf dem Display.

9. **`HTTPClient` für WhatsApp ohne SSL-Pinning**
   * Die CallMeBot-URL wird per HTTP verwendet; in neueren Versionen ist HTTPS verfügbar und sollte genutzt werden (`ssl_client.setInsecure()` analog zu SMTP).

10. **`ArduinoOTA` ist eingebunden, aber `ArduinoOTA.begin()` wird nie aufgerufen**
    * Das `HTTPUpdateServer`-Konstrukt wird genutzt, nicht der ArduinoOTA-Handler. Die Bibliothek wird daher aktuell **nicht** verwendet – Platz für Aufräumarbeiten.

---

## Ressourcen

### Hardware

- [ESP32-C3 SuperMini – Datenblatt (Espressif)](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [ZMCT103C – Stromsensor-Modul](https://en.clever-innovations.com/products/ZMCT103C)
- [GxEPD2 – kompatible 1,54″/2,13″ E-Paper-Displays (Waveshare)](https://www.waveshare.com/wiki/2.13inch_e-Paper_HAT)

### Software / Libraries

- [GxEPD2 (Jean-Marc Zingg)](https://github.com/ZinggJM/GxEPD2)
- [U8g2_for_Adafruit_GFX](https://github.com/olikraus/U8g2_for_Adafruit_GFX)
- [ReadyMail (mo-thunder)](https://github.com/mo-thunder/ReadyMail)
- [CallMeBot API](https://www.callmebot.com/blog/free-api-whatsapp-messages/)
- [Arduino-ESP32 (Espressif)](https://github.com/espressif/arduino-esp32)

### Referenzen

- [ESP-IDF: Deep-Sleep und GPIO-Wakeup](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/sleep_modes.html)
- [Gmail App-Passwörter](https://support.google.com/accounts/answer/185833)

---

## Todo

- [ ] **Bugfix:** `handleMeasurement()`-Display-Refresh reparieren (`millis() % 1000` durch Timer ersetzen).
- [ ] **Bugfix:** Watchdog in `handleMeasurement()` einbauen, damit das Gerät nicht dauerhaft wach bleibt.
- [ ] **Bugfix:** `while(!digitalRead(PIN_BUTTON_UP)) {}` durch Interrupt oder Timer ersetzen.
- [ ] **Refactor:** `ArduinoOTA` entfernen oder konsistent einsetzen (aktuell unbenutzt).
- [ ] **Robustheit:** `alarmSperre` automatisch zurücksetzen, sobald das Netz länger als X Sekunden stabil ist.
- [ ] **Robustheit:** `PIN_POWER_CHECK` mit definiertem Pull-up/down beschalten oder `INPUT_PULLDOWN` setzen.
- [ ] **Feature:** Konfiguration der Schwellen (`PUMP_TRIGGER`, Akku-Schwelle, `cyclesRequired`) über Weboberfläche.
- [ ] **Feature:** `credentials.h` durch LittleFS-Datei ersetzen (Konfiguration via OTA-Weboberfläche).
- [ ] **Feature:** mDNS-Service-Name (`pumpmonitor.local`) statt IP-Anzeige auf dem Display.
- [ ] **Feature:** HTTPS statt HTTP für WhatsApp (`ssl_client.setInsecure()` analog SMTP).
- [ ] **Doku:** Schaltplan und PCB-Layout als PDF/Image hinzufügen.
- [ ] **Test:** Unit-Tests für `fetchBatteryVoltage()` und `messurePumpCurrent()`.
- [ ] **CI:** GitHub-Actions-Workflow für PlattformIO-Build.
- [ ] **Gehäuse:** 3D-Druck-Modell (STEP/STL) für Wandmontage.
- [ ] **Power:** Solar-/USB-Lade-IC (z. B. TP4056) sauber in den Deep-Sleep-Pfad integrieren.
- [ ] **Logging:** Optional Persistierung der letzten X Alarme ins SPIFFS für spätere Diagnose.

---

*Letzte Aktualisierung: README-Generierung basierend auf `PumpMonitor_SuperMini.ino` (588 Zeilen) und `credentials-template.h`.*
