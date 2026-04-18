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
- **Status:** OPEN
- **Beschreibung:** Einige der 44 Konvertierungspfade führen aktuell ohne
  expliziten Verlust-Report aus. Der `--accept-data-loss`-Flag ist noch nicht
  durchgängig erzwungen.
- **Workaround:** Vor jeder Konvertierung `uft convert --dry-run` nutzen.
- **Plan:** Einheitlicher Wrapper um alle `convert_*`-Pfade in 4.2.x.

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
