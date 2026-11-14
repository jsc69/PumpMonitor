# Projektkontext (gemeinsame Quelle für alle Agents)

Diese Datei ist die einzige maßgebliche Quelle für Werte, die mehrere Bereiche
(Firmware, Elektronik, Konstruktion) betreffen. Ändert sich ein Wert, zuerst hier
ändern, danach die betroffenen Bereiche anpassen.

## Pinbelegung ESP32-C3 SuperMini

Hinweis: ADC gibt es beim ESP32-C3 nur auf GPIO 0–4. Deep-Sleep-Wakeup per GPIO: GPIO 0–5.

| GPIO | Funktion | Richtung | Bemerkung |
|------|----------|----------|-----------|
| 0 | `PIN_PUMP_CHECK` | Analog In | ZMCT103C-Stromsensor (ADC1_CH0) |
| 1 | `PIN_AKKU` | Analog In | Akku-Spannungsteiler (ADC1_CH1) |
| 2 | `PIN_POWER_CHECK` | Digital In | Netzausfall-Erkennung (5 V Versorgung) |
| 3 | reserviert | – | – |
| 4 | `PIN_BUTTON_DOWN` | Digital In, `INPUT_PULLUP`, ext0-Wakeup | kurz: Alarm-Reset, lang (≥ 3 s): OTA |
| 5 | `PIN_BUTTON_UP` | Digital In, `INPUT_PULLUP`, ext1-Wakeup | MESSUNG-Modus |
| 6 | `PIN_DIS_CS` | Output | E-Paper SPI Chip-Select |
| 7 | `PIN_DIS_DC` | Output | E-Paper Data/Command |
| 8 | `PIN_LED` | Output | Status-LED |
| 9 | `PIN_DIS_BUSY` | Input | E-Paper Busy |
| 10 | `PIN_DIS_RST` | Output | E-Paper Reset |
| 20 | `PIN_DIS_SCK` | Output | E-Paper SPI Clock |
| 21 | `PIN_DIS_MOSI` | Output | E-Paper SPI MOSI |

## Spannungen und Akku

- Akku: Li-Ion/LiPo (genauer Typ und Kapazität: TODO, vom User zu ergänzen)
- Spannungsteiler Akku: R1 = R2 = 100 kΩ (Faktor 2), Eichfaktor in der Firmware 1.1
- ADC-Referenz: 3,3 V
- Abschaltschwelle Firmware: 3,3 V

## Mechanik (vom User zu befüllen)

| Größe | Wert |
|-------|------|
| Platine (L x B x H) | TODO |
| Montagebohrungen | TODO |
| ESP32-C3 SuperMini (ca.) | 40 x 20 mm |
| E-Paper-Display (Typ, Außenmaß, aktive Fläche) | TODO |
| Position USB-C-Buchse | TODO |
| Position Taster UP/DOWN | TODO |
| ZMCT103C (Maße, Position) | TODO, Modelle in `hardware/zmct103c.stp` / `.stl` / `.wrl` |
| Akku-Maße | TODO |
