# Firmware-Agent (software/)

Zuständig für die ESP32-C3-Firmware. Lies vorher `../AGENTS.md` und `../docs/project-context.md`.

## Bereich
- Hauptprojekt: `software/PumpMonitor_SuperMini/` (Sketch `PumpMonitor_SuperMini.ino`, Vorlage `credentials-template.h`).
- Die übrigen Ordner (`AkkuCheck`, `Blink`, `CurrentSensor`, `DisplayDemo`, `DisplayTest`,
  `HardwareTest`, `PumpMonitor`, `PumpMonitor2`, `PumpMonitor2_Test`) sind Tests und ältere Varianten.
  Nicht ändern, außer die Aufgabe nennt sie ausdrücklich.

## Technik
- ESP32-C3, Arduino-Framework. Libraries: GxEPD2, U8g2_for_Adafruit_GFX, ReadyMail,
  HTTPClient, WiFi, WebServer, HTTPUpdateServer, esp_sleep.
- Das Gerät läuft aus dem Deep-Sleep: `setup()` erledigt alles, `loop()` ist leer.
- Zustand über Deep-Sleep nur per `RTC_DATA_ATTR`.
- Stromverbrauch hat Priorität: keine blockierenden Wartezeiten, WLAN so kurz wie möglich.

## Build
Aktuell Arduino IDE. Die Migration auf PlatformIO steht als erstes TODO in `readme.md`.
Nach der Migration gilt: `pio run -e esp32c3` muss fehlerfrei durchlaufen, bevor ein PR entsteht.

## Flashen
Der Agent darf flashen, wenn das Gerät per USB angeschlossen ist oder per OTA erreichbar ist.
**Vor jedem Flash-Vorgang muss der User ausdrücklich bestätigen.** Zeige vorher:
Welche Firmware (Branch/Commit), welcher Weg (USB oder OTA), welcher Port bzw. welche Adresse.
- USB: Port vorher prüfen, nicht raten.
- OTA: Das Gerät muss im OTA-Modus sein (langer Druck auf DOWN). Der Upload läuft über die
  Weboberfläche des `HTTPUpdateServer` mit `ota_user`/`ota_pass`. Diese Zugangsdaten nie ausgeben.
- Nach dem Flash: Serial-Ausgabe prüfen, falls USB verbunden, sonst den User um Rückmeldung bitten.

## Regeln
- `credentials.h` nie lesen, ändern oder ausgeben. Nur `credentials-template.h` bearbeiten.
- Pinbelegung nur gemäß `docs/project-context.md`. Pin-Änderungen dort zuerst eintragen
  und im Ergebnis als Folgearbeit für Elektronik melden.
- Verhalten nicht ändern, wenn die Aufgabe ein reiner Umbau ist (z. B. Migration).
- Logging über `ESP_LOGx` mit Tag `PUMP_MONITOR`.
- Bekannte Probleme stehen in der `readme.md` unter "Bekannte Fehler / Quirks". Beim Beheben dort aktualisieren.
