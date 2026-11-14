# PumpMonitor

Batteriebetriebener Pumpenwächter: ESP32-C3 SuperMini, E-Paper, ZMCT103C-Stromsensor,
Deep-Sleep, Alarme per WhatsApp (CallMeBot) und E-Mail. Details: `readme.md`.

## Struktur und Zuständigkeiten
| Bereich | Pfad | Hinweise |
|---|---|---|
| Firmware | `software/` | siehe `software/AGENTS.md` |
| Elektronik | `hardware/PumpMonitor2_SuperMini/` | siehe dortige `AGENTS.md` |
| Konstruktion | `hardware/3D/` | siehe dortige `AGENTS.md` |
| Doku | `doku/`, `readme.md`, `docs/` | |

Arbeite nur in dem Bereich, der zur Aufgabe passt. Änderungen außerhalb deines
Bereichs nicht selbst durchführen, sondern als Folgeaufgabe benennen.

## Gemeinsamer Kontext
`docs/project-context.md` ist die einzige maßgebliche Quelle für Pinbelegung,
Platinen- und Bauteilmaße, Spannungen und andere Randbedingungen, die mehrere
Bereiche betreffen. Lies sie, bevor du etwas änderst, das daran hängt.
- Ändert sich ein Wert, zuerst dort ändern, danach Firmware, Schaltplan bzw. Konstruktion anpassen.
- Elektrische Änderung mit mechanischer Auswirkung (Taster, Stecker, Platinenmaße)
  oder Pin-Änderung: im Ergebnis ausdrücklich als "Folgearbeit" auflisten.

## Offene Aufgaben
Die TODO-Liste steht in `readme.md`. Dort nachsehen, was ansteht, und erledigte
Punkte abhaken. Keine eigenen Listen in den AGENTS.md führen.

## Regeln für alle
- Secrets: `credentials.h` nie committen, nie ausgeben. Nur die Vorlage `credentials-template.h` nutzen.
- Commits: klein, ein Thema pro Commit, kurze Nachricht im Imperativ.
- Branch pro Aufgabe (`fix/…`, `feat/…`, `hw/…`, `mech/…`), nie direkt auf `main`.
- PR: Was, Warum, Wie getestet. Mit `Closes #n` auf das Issue verweisen, falls vorhanden.
- Merge nur nach Bestätigung durch den User.
- Kommentare im Code nur bei komplexer Logik. Neue Dateien bekommen einen kurzen Header.
- Bei Unklarheiten nachfragen, statt zu raten (z. B. Akkutyp, Spannungen, Maße).
- Im Ergebnis immer nennen: was geändert wurde, wie es geprüft wurde, was der User noch tun muss.
