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
- **Status:** MITIGATED (Writer implementiert, Integration pending)
- **Beschreibung:** Schema `uft-loss-report-v1` + Writer-API
  (`include/uft/core/uft_loss_report.h`, `src/core/uft_loss_report.c`) sind
  da. 11 Verlust-Kategorien (WEAK_BITS, FLUX_TIMING, INDEX_PULSES,
  SYNC_PATTERNS, MULTI_REVOLUTION, CUSTOM_METADATA, COPY_PROTECTION,
  LONG_TRACKS, HALF_TRACKS, WRITE_SPLICE, OTHER). Schreibt JSON neben
  Ziel-Datei als `<target>.loss.json`. Noch NICHT an die 44 Konvertierungs-
  pfade angeschlossen — das passiert unter §1.2.
- **Workaround:** CLI kann `uft_loss_report_write()` direkt nutzen.
- **Plan:** §1.2 (Pre-Conversion-Report) wickelt alle `convert_*`-Pfade ein
  und ruft den Writer auf.

### 1.2 Nicht alle Konvertierungen haben Pre-Conversion-Report
- **Status:** MITIGATED (Helper da, Wiring in Konvertierer ausstehend)
- **Beschreibung:** Preflight-Helper implementiert
  (`include/uft/core/uft_preflight.h`, `src/core/uft_preflight.c`). Kombiniert
  §1.1 Sidecar-Writer + §5.1 Round-Trip-Matrix zu einer einheitlichen
  Pre-Check/Commit-API. Vier Entscheidungen nach Prinzip 1+4+5:
  - `OK` (LL still oder LD mit `accept_data_loss=true`)
  - `ABORT_NEED_CONSENT` (LD ohne Zustimmung)
  - `ABORT_IMPOSSIBLE` (Ziel kann Quelle nicht repräsentieren)
  - `ABORT_UNTESTED` (Paar nicht in Matrix)

  Aufrufer-Pattern: `uft_preflight_check()` vor Konvertierung; bei LD-OK
  nach Konvertierung `uft_preflight_emit_sidecar()` mit echten Verlust-
  Counts. Die Integration in die 44 bestehenden `convert_*`-Pfade ist
  der verbleibende Schritt.
- **Workaround:** Konvertierer die direkt `uft_loss_report_write()` nutzen,
  sind weiterhin gültig — der Helper ist Syntax-Zucker.
- **Plan:** Schrittweise Migration aller `convert_*`-Entry-Points auf
  den Preflight-Helper (pro Konvertierer ~10 Zeilen Glue-Code).

---

## Prinzip 5 — Round-Trip als First-Class Funktion

### 5.1 Round-Trip-Matrix unvollständig getestet
- **Status:** MITIGATED (Registry + API + 13 Paare, Rest UNTESTED)
- **Beschreibung:** Registry implementiert (`include/uft/core/uft_roundtrip.h`,
  `src/core/uft_roundtrip.c`). Status pro Paar: `UFT_RT_LOSSLESS` /
  `UFT_RT_LOSSY_DOCUMENTED` / `UFT_RT_IMPOSSIBLE` / `UFT_RT_UNTESTED`.
  Initial-Matrix hat 13 Einträge (SCP↔HFE LL, SCP→IMG/ADF/D64/IMD LD,
  HFE→IMG/ADF LD, IMG/ADF→SCP IM, IMG→HFE IM, IPF→ADF LD, STX→ST LD).
  Alles andere fällt auf UNTESTED und sollte nicht angeboten werden.
  Struktur-Invarianten per Test erzwungen: LD+IM brauchen Notes,
  keine Duplikate, UNTESTED nicht explizit gelistet.
- **Workaround:** `uft_roundtrip_status(from, to)` vor Konvertierung abfragen.
- **Plan:** Integration in CLI-Konvertierungspfad (nächster Schritt zu §5.2
  GUI-Sichtbarkeit + §1.2 Pre-Conversion-Report).

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
- **Status:** MITIGATED (Infrastruktur da, Populierung 1/80)
- **Beschreibung:** `uft_plugin_compat_entry_t` Array + `compat_entries` /
  `compat_count` Felder sind in `uft_format_plugin_t`. Status pro
  Konsumer: `UFT_EMU_COMPATIBLE` / `UFT_EMU_INCOMPATIBLE` / `UFT_EMU_PARTIAL`
  / `UFT_EMU_UNTESTED`. Felder pro Eintrag: consumer-name,
  status, note (Pflicht bei PARTIAL/INCOMPATIBLE), test_date, ci_tested.
  Populiert: ADF (6 Konsumer-Einträge als Exemplar).
- **Workaround:** Bis mehr Plugins populiert sind — in Issue-Tracker nach
  Format-Namen suchen.
- **Plan:** `compatibility-import-export`-Agent oder Community-PRs erweitern
  die Matrizen iterativ. Langfrist: CI-Pipeline (§6.1) schreibt `ci_tested`
  automatisch.

---

## Prinzip 7 — Ehrlichkeit bei proprietären Formaten

### 7.1 Spec-Status-Marker pro Plugin fehlt
- **Status:** MITIGATED (Infrastruktur da, Populierung 15/80)
- **Beschreibung:** Feld `spec_status` (`uft_spec_status_t`) ist in
  `uft_format_plugin_t` implementiert. 15 Plugins sind populiert (2IMG, ADF,
  ATR, CQM, D64, DSK-CPC, EDSK, G64, HFE, IMD, IMG, IPF, STX, TD0, WOZ). Die
  restlichen ~65 Plugins stehen defaultmäßig auf `UFT_SPEC_UNKNOWN` — das ist
  ein Prinzip-Verstoß und muss populiert werden.
- **Workaround:** `tests/test_spec_status.c` zeigt die API-Form; populierte
  Plugins sind in den jeweiligen `uft_format_plugin_<name> = { .spec_status = …}`
  Initializern dokumentiert.
- **Plan:** Restliche Plugins in 4.2.x iterativ populieren. CI-Audit der
  Plugins mit `spec_status == UFT_SPEC_UNKNOWN` ausschlägt (separater Eintrag
  unter M.1).

### 7.2 Feature-Matrizen pro Plugin fehlen
- **Status:** MITIGATED (Infrastruktur da, Populierung 5/80)
- **Beschreibung:** `uft_plugin_feature_t` Array + `features` / `feature_count`
  Felder sind in `uft_format_plugin_t` implementiert. Status je Feature:
  `UFT_FEATURE_SUPPORTED` / `UFT_FEATURE_PARTIAL` / `UFT_FEATURE_UNSUPPORTED`;
  PARTIAL verlangt zwingend einen `note` der die Einschränkung erklärt.
  Populiert: ADF, HFE, IPF, STX, WOZ. Rest hat `features = NULL`.
- **Workaround:** `tests/test_spec_status.c` zeigt API-Form. Beispielmatrizen
  in den 5 populierten Plugins.
- **Plan:** Restliche Plugins in 4.2.x iterativ. CI-Audit der Plugins mit
  `features == NULL` geplant (unter M.1).

### 7.3 287 Stub-Parser sind als "registriert" sichtbar
- **Status:** MITIGATED (Marker da, Populierung 0/287)
- **Beschreibung:** Feld `is_stub` ist in `uft_format_plugin_t` implementiert.
  Default `false` — das heisst: ein Stub MUSS aktiv `is_stub = true` setzen
  um ehrlich zu sein. CLI-Filter `uft formats --real-only` nutzt diesen Flag.
  Die eigentliche Populierung der 287 Stub-Plugins ist noch ausstehend.
- **Workaround:** Bis jeder Stub das Feld setzt, zählt ein Plugin ohne echten
  Parser als Prinzip-Verstoß unter CI-Audit (siehe §M.1).
- **Plan:** Stubs in `memory/project_stub_conversion.md` werden pro Tier
  abgearbeitet. Jedes Stub-Plugin bekommt entweder echte Implementierung
  ODER `is_stub = true` (stub-eliminator-Agent).

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
