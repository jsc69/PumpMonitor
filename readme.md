# PumpMonitor SuperMini

Ein **ESP32-C3 SuperMini**-basiertes Überwachungssystem für Drainagepumpen mit E-Paper-Display, WiFi-Benachrichtigungen (WhatsApp/Email) und OTA-Update-Funktion.

## 🎯 Features

- **Stromverbrauchsmessung** via ZMCT103C Current Sensor
- **Batteriespannungsüberwachung** (Tiefentladungsschutz)
- **Echtzeit-Pumpenaktivitätserkennung** mit konfigurierbarem Trigger-Schwellwert
- **Intelligente Alarme** mit Alarm-Sperre (verhindert Spam)
- **WiFi-Benachrichtigungen**:
    - WhatsApp-Nachrichten via CallMeBot API
    - Email-Status-Reports
- **OTA-Firmware-Updates** über Web-Interface
- **Deep Sleep** (2-Min-Intervalle) für extrem niedrigen Stromverbrauch
- **1,54" E-Paper-Display** mit Icons für Status-Anzeige
- **Zwei programmierbare Taster** für manuelle Funktionen
- **Echtzeituhr** mit NTP-Synchronisierung (CET/CEST)

---

## 📋 Hardware

### Verwendete Komponenten

| Komponente | Modell | Anmerkung |
| --- | --- | --- |
| Microcontroller | ESP32-C3 SuperMini | 16-Pin Board, 160 MHz |
| Display | GxEPD2 1,54" E-Paper | 200x200px, GDEY0154D67 |
| Current Sensor | ZMCT103C | für Pumpen-Stromerfassung |
| Power Supply | USB 5V / Li-Ion Batterie | mit Spannungsteiler (2:1) |

### Pinbelegung (ESP32-C3)

```
GPIO 0  → PIN_PUMP_CHECK    (ADC - Stromsensor ZMCT103C)
GPIO 1  → PIN_AKKU          (ADC - Batteriespannung)
GPIO 3  → PIN_POWER_CHECK   (Netzversorgung OK, aktiv HIGH)
GPIO 4  → PIN_BUTTON_DOWN   (Taster 1: Reset/OTA, aktiv LOW)
GPIO 5  → PIN_BUTTON_UP     (Taster 2: Messmodus, aktiv LOW)
GPIO 6  → PIN_DIS_CS        (Display SPI: Chip Select)
GPIO 7  → PIN_DIS_DC        (Display SPI: Data/Command)
GPIO 8  → PIN_LED           (Statusanzeige LED)
GPIO 9  → PIN_DIS_BUSY      (Display: Busy Signal)
GPIO 10 → PIN_DIS_RST       (Display: Reset)
GPIO 20 → PIN_DIS_SCK       (Display SPI: Clock)
GPIO 21 → PIN_DIS_MOSI      (Display SPI: MOSI)
```

**⚠️ Wichtig:** ESP32-C3 hat nur **13 nutzbare GPIO-Pins**. ADC nur auf GPIO 0-4, RTC-Wakeup nur auf GPIO 0-5!

---

## 🔧 Installation

### 1. Arduino IDE Setup

1. **Board hinzufügen:**
    - Preferences → Additional Boards Manager URLs:
    
    ```
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    ```
    
    - Boards Manager → "esp32" von Espressif Systems installieren
    - Board: `ESP32C3 Dev Module`
2. **Benötigte Bibliotheken** (Sketch → Bibliotheken einbinden):
    - `GxEPD2` von ZinggJM (E-Paper Display)
    - `ReadyMail` von mobizt (Email-Versand)
    - `ArduinoJson` von Benoit Blanchon (JSON-Parsing)
    - `WiFi` (ESP32 Standard)
    - `HTTPUpdate` (ESP32 Standard - OTA)
    - `time` / `sys/time` (NTP-Sync)

### 2. Konfiguration

**Datei:** `credentials.h`

```cpp
const char* ssid = "DEIN_WLAN";              // WiFi SSID
const char* password = "DEIN_PASSWORD";      // WiFi Passwort

const char* callmebot_user = "+49XXXXXXXXX"; // Deine Handynummer (WhatsApp)
const char* ota_user = "admin";              // OTA-Benutzername
const char* ota_pass = "sicher1234";        // OTA-Passwort
```

**Datei:** `PumpMonitor_SuperMini.ino`

```cpp
// Stromschwellwert für Pumpen-Aktivitätserkennung
const int PUMP_TRIGGER = 1500;  // (max - min) > 1500 = Pumpe aktiv

// Deep Sleep Intervall
const int SLEEP_MINUTES = 2;    // Messung alle 2 Minuten

// Status-Report Häufigkeit
const int HEARTBEAT_DAYS = 7;   // Alle 7 Tage ein Status-Email

// OTA Timeout
const uint32_t OTA_TIMEOUT = 120000;  // 120 Sekunden

// Benachrichtigungen deaktivieren (z.B. für Tests)
bool NO_WHATSAPP = true;  // true = Keine WhatsApp-Nachrichten
bool NO_EMAIL = true;     // true = Keine Emails
```

---

## 📖 Bedienungsanleitung

### Display-Anzeigen

```
┌──────────────────────┐
│  [ICON] STATUS       │  ← Icon zeigt Zustand
│                      │
│  Batterie: 3.7V      │  ← Spannung
│  Pumpe: INAKTIV      │  ← Pumpen-Status
│  WLAN: OK            │  ← Verbindungsstatus
└──────────────────────┘
```

### Normal-Modus (Standard)

**Automatischer Betrieb:**

1. ESP wacht alle **2 Minuten** auf
2. Misst Stromverbrauch und Batteriespannung
3. Prüft ob Pumpe aktiv ist (Stromspitze > PUMP_TRIGGER)
4. Sendet ggf. WhatsApp/Email-Alarm
5. Geht zurück in **Deep Sleep**

**Button-Funktionen:**

- **DOWN-Button** (lange drücken, 3 Sekunden): OTA-Update-Modus aktivieren
- **UP-Button** (kurz drücken): Startet **Messlauf-Modus**

---

### 🔍 Messlauf-Modus

**Aktivierung:** UP-Button kurz drücken

**Was passiert:**

- Display zeigt: "MESSLAUF"
- ESP bleibt **wach** (kein Sleep)
- Misst kontinuierlich alle 500ms
- Zeigt Stromwerte live auf Display:
    
    ```
    MESSLAUF
    Min: 1250  Max: 2850
    Ø: 1650
    ```
    

**Beendigung:** 

- DOWN-Button drücken → zurück zu Normalmode
- Oder 5 Minuten ohne Aktion → Rückkehr zu Normalmode

**Wofür hilfreich:**

- Stromsensor kalibrieren
- Pumpen-Triggerwert ermitteln
- Fehlalarme diagnostizieren

---

### 🔧 OTA-Update-Modus

**Aktivierung:** DOWN-Button **lange drücken** (mindestens 3 Sekunden)

**Was passiert:**

1. Display zeigt: "OTA MODUS"
2. ESP versucht mit WiFi zu verbinden
3. Nach erfolgreicher Verbindung anzeige:
    
    ```
    OTA AN
    IP: 192.168.1.100
    User: admin
    Pwd: sicher1234
    ```
    

**Firmware-Update von PC aus:**

```bash
# Option 1: Arduino IDE
1. Sketch → Export compiled Binary
2. Browser öffnen: http://192.168.1.100/update
3. Username: admin
4. Passwort: sicher1234
5. .bin-Datei hochladen

# Option 2: curl (Linux/Mac/Windows-WSL)
curl -F "image=@firmware.bin" \
     --user admin:sicher1234 \
     http://192.168.1.100/update
```

**Beendigung:**

- Nach erfolgreichem Update: **automatischer Neustart**
- Bei Fehler: DOWN-Button drücken → Reset & Rückkehr zu Normalmode

⚠️ **Wichtig:** OTA-Modus verbraucht viel Strom! Nur mit USB-Stromversorgung verwenden!

---

## 🔋 Batterie & Stromverbrauch

### Messungen (typisch)

| Phase | Stromaufnahme | Dauer |
| --- | --- | --- |
| Deep Sleep | ~50 µA | 118 Sekunden |
| Messung (Sensor + WiFi) | ~150 mA | ~2 Sekunden |
| **Durchschnitt (2-Min-Zyklus)** | **~2 mA** | — |

### Batterielaufzeit

- **1000 mAh Li-Ion:** ~500 Stunden (21 Tage)
- **2000 mAh Li-Ion:** ~1000 Stunden (42 Tage)

### Tiefentladungsschutz

Wenn Akkuspannung < 3,3V:

- System zeigt "AKKU LEER!"
- Sendet WhatsApp-Warnung
- Geht in permanenten Sleep (nur durch Reset erreichbar)

---

## ⚙️ Konfiguration & Anpassung

### Pumpen-Trigger einstellen

1. **Messlauf-Modus** starten (UP-Button)
2. Pumpe manuell aktivieren
3. Min/Max-Werte notieren (z.B. Min: 1200, Max: 2900)
4. Differenz berechnen: 2900 - 1200 = **1700**
5. `PUMP_TRIGGER` auf Differenz + Puffer setzen:
    
    ```cpp
    const int PUMP_TRIGGER = 1500;  // Etwas niedriger als Messwert
    ```
    

### Deep Sleep Intervall ändern

```cpp
const int SLEEP_MINUTES = 2;  // Auf z.B. 5 Min ändern
```

⚠️ **Achtung:** Häufigere Messungen = höherer Stromverbrauch!

### Email-Adresse für Status-Reports

In `PumpMonitor_SuperMini.ino`:

```cpp
const char* ADMIN_EMAIL = "deine.email@example.com";
```

---

## 🐛 Fehlersuche

### "Kein WLAN" auf Display

- WiFi-SSID/Passwort in `credentials.h` prüfen
- Signal-Stärke testen
- AP 2.4 GHz verwenden (C3 unterstützt kein 5 GHz)

### Keine WhatsApp/Email-Benachrichtigungen

- `NO_WHATSAPP = false` / `NO_EMAIL = false` in Code setzen
- Credentials prüfen
- Internet-Verbindung testen (Serial Monitor)

### Pumpen-Trigger springt ständig an

- `PUMP_TRIGGER` Wert erhöhen (z.B. 2000)
- Mit Messlauf-Modus (`BUTTON_UP`) testen
- Stromsensor-Kalibrierung überprüfen

### Deep Sleep funktioniert nicht

- RTC-Memory überprüfen: `RTC_DATA_ATTR` korrekt?
- GPIO-Wakeup aktiviert? (`esp_deep_sleep_enable_gpio_wakeup`)
- Timer-Wakeup: `esp_sleep_enable_timer_wakeup` aufgerufen?

### Display bleibt dunkel

- SPI-Pins (6, 7, 10, 20, 21) prüfen
- Display-Stromversorgung testen (3,3V)
- GxEPD2 Bibliothek aktualisieren

---

## 🔗 Nützliche Links

- ESP32-C3 Datenblatt
- GxEPD2 Library
- U8g2 Fonts
- CallMeBot WhatsApp API
- ReadyMail Library
- ESP32 Deep Sleep Guide

---

## 📄 Lizenz

MIT License

## 👨‍💻 Autor

Drainagepumpen-Überwachung | 2026
