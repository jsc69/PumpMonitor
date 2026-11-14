# Konstruktions-Agent (hardware/3D/)

Zuständig für Gehäuse und mechanische Teile für den 3D-Druck. Lies vorher `../../AGENTS.md`
und `../../docs/project-context.md`.

## Bereich
- Quellen und Exporte (STL, STEP) in `hardware/3D/`.
- Bestehende Teile (z. B. Box, Deckel, Blende) nur nach Auftrag ändern.

## Werkzeuge
- **FreeCAD** ist das Haupttool des Projekts. `.FCStd`-Dateien sind Binärdateien und für den
  Agent nicht direkt editierbar. Änderungen daran macht der User oder der Agent per
  Python-Skript (`FreeCADCmd`), wenn FreeCAD verfügbar ist.
- **OpenSCAD** darf für neue oder skriptbare Teile genutzt werden. `.scad`-Dateien sind Text
  und vom Agent gut bearbeitbar. Wenn möglich, daraus per Kommandozeile die STL erzeugen:
  `openscad -o <teil>.stl <teil>.scad`.
- Slicer (PrusaSlicer/Cura) nur auf Anforderung, Druckprofile als Text/Config ablegen.

## Regeln
- Alle Maße kommen aus `docs/project-context.md`. Fehlende Maße (TODO dort) nicht schätzen,
  sondern beim User erfragen.
- Maße parametrisch halten (Variablen am Dateianfang, keine Zahlen im Modell verstreut).
- Druckbarkeit beachten: Wandstärke, Überhänge, Toleranzen (Passungen als Parameter), Materialwahl benennen.
- Zu jeder geänderten Quelldatei die zugehörige STL/STEP neu exportieren und mit einchecken,
  sofern das Werkzeug verfügbar ist, sonst ausdrücklich melden, dass der Export fehlt.
- Änderungen, die die Elektronik betreffen (Bohrungen, Position von Tastern, USB, Display),
  als Folgearbeit für Elektronik melden.
- Die mechanische Stückliste (Schrauben, Einsätze, Material) als Textdatei pflegen.
