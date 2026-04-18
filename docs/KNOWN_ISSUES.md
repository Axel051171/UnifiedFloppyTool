# Known Issues — Principle Compliance

Diese Liste dokumentiert Fälle wo UFT aktuell die [Design-Prinzipien](DESIGN_PRINCIPLES.md)
nicht vollständig einhält. Die Liste ist öffentlich (Meta-Prinzip C) und wird
aktiv abgearbeitet.

**Format pro Eintrag:**
- **Prinzip:** Welches Prinzip betroffen ist
- **Status:** `OPEN` / `MITIGATED` / `WORKING-AS-DESIGNED-INTERIM`
- **Beschreibung:** Was aktuell nicht stimmt
- **Workaround:** Was Nutzer heute tun können
- **Plan:** Wie und wann es adressiert wird

---

## Prinzip 1 — Niemals stille Datenverluste

### 1.1 `.loss.json` Sidecar-Format noch nicht implementiert
- **Status:** OPEN
- **Beschreibung:** Prinzip 1 verlangt Sidecar-Dateien für nicht-repräsentier-
  bare Informationen bei LOSSY-DOCUMENTED-Konvertierungen. Das Schema und der
  Writer existieren noch nicht. Aktuell werden Verluste im Audit-Log protokol-
  liert, aber nicht als separate Machine-readable-Datei.
- **Workaround:** Audit-Log inspizieren (`uft log --session=<id>`).
- **Plan:** Siehe Roadmap Milestone "Lossless-Framework".

### 1.2 Nicht alle Konvertierungen haben Pre-Conversion-Report
- **Status:** OPEN
- **Beschreibung:** Einige der 44 Konvertierungspfade führen aktuell ohne
  expliziten Verlust-Report aus. Der `--accept-data-loss`-Flag ist noch nicht
  durchgängig erzwungen.
- **Workaround:** Vor jeder Konvertierung `uft convert --dry-run` nutzen.
- **Plan:** Einheitlicher Wrapper um alle `convert_*`-Pfade in 4.2.x.

---

## Prinzip 5 — Round-Trip als First-Class Funktion

### 5.1 Round-Trip-Matrix unvollständig getestet
- **Status:** OPEN
- **Beschreibung:** Für viele Format-Paare existiert noch kein automatisierter
  Round-Trip-Test. Diese Paare haben formal Status `?` und sollten nicht als
  Konvertierung angeboten werden — werden aber angeboten.
- **Workaround:** Für forensisch kritische Archivierung nur SCP↔HFE nutzen
  (einziger aktuell durchgängig getesteter LL-Pfad).
- **Plan:** Matrix-Tests werden pro Release-Zyklus erweitert. Ziel 4.3: alle
  angebotenen Pfade haben mindestens LD-Dokumentation.

### 5.2 Keine Sichtbarkeit des Round-Trip-Status in der GUI
- **Status:** OPEN
- **Beschreibung:** Der Format-Converter-Dialog zeigt aktuell keinen
  LL/LD/IM-Status. Nutzer sieht nur die Liste möglicher Targets.
- **Workaround:** CLI `uft convert --list-status` zeigt verfügbare Status.
- **Plan:** GUI-Integration in 4.2.x zusammen mit Pre-Conversion-Report.

---

## Prinzip 6 — Emulator-Kompatibilität

### 6.1 Keine CI-Pipeline mit Emulator-Verifikation
- **Status:** OPEN
- **Beschreibung:** Prinzip 6 verlangt CI die Exports durch Emulatoren
  schickt. Aktuell ist das für kein Format automatisiert. Manuelle Tests
  existieren ad-hoc.
- **Workaround:** Keine — Nutzer müssen Emulator-Tests selbst durchführen.
- **Plan:** Initial ADF/WinUAE, D64/VICE in 4.3.

### 6.2 Kompatibilitäts-Matrizen pro Format fehlen größtenteils
- **Status:** OPEN
- **Beschreibung:** Außer bei ADF gibt es keine gepflegten Kompatibilitäts-
  Matrizen (welche Emulator-Version / welches Tool konsumiert den Export
  korrekt). Das blockiert ehrliche "funktioniert mit X" Claims.
- **Workaround:** In Issues nach Format-Namen suchen — dort oft Erfahrungs-
  berichte.
- **Plan:** `compatibility-import-export`-Agent soll die Matrizen aus
  Community-Tests aufbauen.

---

## Prinzip 7 — Ehrlichkeit bei proprietären Formaten

### 7.1 Spec-Status-Marker pro Plugin fehlt
- **Status:** OPEN
- **Beschreibung:** Die 80 registrierten Plugins haben aktuell keinen
  maschinenlesbaren Spec-Status-Marker (`OFFIZIELL-SPEC` / `REVERSE-ENGINEERED`
  etc.). Nutzer kann nicht per `uft formats --list` sehen welche Plugins auf
  offizieller vs. rekonstruierter Spec basieren.
- **Workaround:** Einzelne Plugin-Header (`src/formats/*/uft_*.c`) im
  Dateikommentar konsultieren — inkonsistent gepflegt.
- **Plan:** Feld `spec_status` in `uft_format_plugin_t` in 4.2.x.

### 7.2 Feature-Matrizen pro Plugin fehlen
- **Status:** OPEN
- **Beschreibung:** Keine strukturierte Feature-Matrix pro Format (welche
  Sub-Features unterstützt / teilweise / nicht). "IPF unterstützt" aktuell
  nicht spezifiziert auf welcher Ebene.
- **Workaround:** README-Abschnitt pro Format manuell konsultieren.
- **Plan:** Standard-Tabelle in Plugin-Metadaten in 4.3.

### 7.3 287 Stub-Parser sind als "registriert" sichtbar
- **Status:** WORKING-AS-DESIGNED-INTERIM
- **Beschreibung:** Von 138+287 Format-IDs sind 287 aktuell Stubs. Sie sind
  sichtbar im Plugin-Katalog ohne klare Kennzeichnung dass sie noch keinen
  echten Parser haben. Das verstößt gegen den Geist von Prinzip 7.
- **Workaround:** `uft formats --real-only` listet nur die 138 vollständigen.
- **Plan:** Stub-Parser werden in `memory/project_stub_conversion.md`
  priorisiert abgearbeitet (Tier 1 zuerst). Bis dahin: Kennzeichnung
  `[STUB - no parser]` in der Default-Anzeige.

---

## Meta-Ebene

### M.1 Nicht alle Prinzipien haben automatisierte Tests
- **Status:** OPEN
- **Beschreibung:** Meta-Prinzip A verlangt für jede Zusage mindestens einen
  CI-Test. Aktuell fehlen Tests für: Sidecar-Schema (1.1), Round-Trip-LL für
  viele Paare (5.1), Emulator-Pipeline (6.1), Fehlermeldungs-Struktur (3).
- **Workaround:** Keine — Prinzipien gelten sind nicht vollständig erzwungen.
- **Plan:** Priorisierung in Roadmap-Milestones.

---

## Wie beitragen

- **Neues Issue melden:** GitHub Issue mit Label `principle-violation`.
- **Eintrag abarbeiten:** PR die den Fix plus den entsprechenden CI-Test
  liefert. Eintrag hier wird dann entfernt.
- **Status-Update:** Wenn ein Eintrag obsolet wird oder sich der Status
  ändert, PR gegen diese Datei.

---

**Version:** 1.0
**Stand:** 2026-04-18
