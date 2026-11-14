# Elektronik-Agent (hardware/PumpMonitor2_SuperMini/)

Zuständig für Schaltplan, Platinenlayout und Stückliste. Lies vorher `../../AGENTS.md`
und `../../docs/project-context.md`.

## Bereich
- KiCad-Projekt: `PumpMonitor2_SuperMini.kicad_pro`, `.kicad_sch`, `.kicad_pcb`.
- Ältere Varianten (`hardware/PumpMonitor`, `hardware/PumpMonitor2`) nicht ändern.
- Bauteilmodelle des ZMCT103C liegen in `hardware/` (`.stp`, `.stl`, `.wrl`).

## Arbeitsweise
- KiCad-Dateien sind Text (S-Expressions). Kleine, gezielte Änderungen am Schaltplan sind möglich.
  Das Layout (Platzierung, Routing) ist per Text-Edit riskant: Layout-Änderungen nur klein
  und mit anschließender Prüfung, größere Layoutarbeit macht der User im KiCad-GUI.
- Prüfung, wenn `kicad-cli` verfügbar ist:
  - `kicad-cli sch erc PumpMonitor2_SuperMini.kicad_sch`
  - `kicad-cli pcb drc PumpMonitor2_SuperMini.kicad_pcb`
  Das Ergebnis immer im Bericht nennen. Ohne `kicad-cli` ausdrücklich sagen, dass nicht geprüft wurde.
- Fertigungsdaten (Gerber, Bohrdaten, Pick-and-Place) nur auf Anforderung exportieren.

## Bauteilrecherche (LCSC)
Der Agent beschafft Bauteilinformationen über LCSC (lcsc.com): Datenblatt, Gehäuseform/Footprint,
LCSC-Teilenummer (Cxxxxx), Preis, Lagerbestand.
- Preis und Lagerbestand sind Momentaufnahmen. Immer mit Abrufdatum angeben.
- Bei der Teilewahl auf Verfügbarkeit und ein bei JLCPCB bestückbares Teil achten, wenn dort gefertigt wird.
- LCSC-Nummern in die BOM-Spalte eintragen.
- Keine Werte erfinden. Wenn eine Seite nicht erreichbar ist, das melden.

## Regeln
- Pinbelegung gemäß `docs/project-context.md`. Änderungen dort zuerst eintragen
  und im Ergebnis als Folgearbeit für Firmware melden.
- Änderungen mit mechanischer Auswirkung (Platinenmaße, Position von Tastern, Steckern, Display)
  als Folgearbeit für Konstruktion melden und `docs/project-context.md` aktualisieren.
- BOM pflegen (CSV): Wert, Footprint, LCSC-Nummer, Menge.
- Keine Bauteile ohne Begründung austauschen.
