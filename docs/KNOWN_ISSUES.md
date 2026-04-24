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
- **Status:** MITIGATED (Converter-Wizard angeschlossen, weitere GUI-Flächen ausstehend)
- **Beschreibung:** `UftTargetPage::updateConversionWarning()` konsultiert
  jetzt `uft_roundtrip_status()` / `uft_roundtrip_note()` sobald Quell- UND
  Ziel-Format beide in der Roundtrip-Matrix hinterlegt sind
  (`FormatEntry.rt_id`). Anzeige farbkodiert:
  - **LOSSLESS** grün mit „byte-identical" Badge
  - **LOSSY-DOCUMENTED** orange mit expliziter Verlustliste + Hinweis auf
    `.loss.json` Sidecar
  - **IMPOSSIBLE** rot mit Grund
  - **UNTESTED** grau mit Verweis auf DESIGN_PRINCIPLES §5
  Fallback auf die bisherige Heuristik wenn ein Format noch nicht auf
  `uft_format_id_t` gemappt ist.
- **Workaround:** Entfällt — Wizard zeigt den Status direkt bei Format-Auswahl.
- **Plan:** Rest-GUI-Flächen (Main-Window Convert-Aktion, Batch-Dialog)
  gleiche Info anbringen wenn die dort implementiert werden.

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

### M.-1 ATX-Probe Byte-Order-Bug (entdeckt 2026-04-24)

- **Status:** OPEN, benötigt One-Line-Fix
- **Datei:** `src/formats/atx/uft_atx.c`
- **Problem:** `ATX_SIGNATURE` ist als `0x41543858u` definiert mit Kommentar
  `"AT8X" LE`, aber das ist die **Big-Endian**-Darstellung. Der Probe nutzt
  `uft_read_le32(data)` auf einem Puffer mit Bytes 'A','T','8','X'
  (0x41, 0x54, 0x38, 0x58), was 0x58385441 ergibt — nicht 0x41543858.
  Folge: `atx_plugin_probe` akzeptiert **nie** eine echte ATX-Datei.
- **Fix:** `#define ATX_SIGNATURE   0x58385441u` (LE-korrekt).
- **Beweis:** Der geplante Plugin-Test `test_atx_plugin.c` entdeckte die
  Inkonsistenz beim Vergleich von Probe-Replikation mit echten ATX-Bytes.
  Test wurde daher verschoben bis der Source-Bug behoben ist; siehe
  session-commit-message der M1-Phase.
- **Plan:** Fix im nächsten Bugfix-Commit; dann Test aktivieren.

---

### M.0 Planned APIs (MF-011 DOCUMENT-Welle)

- **Status:** MARKED, nicht implementiert (2026-04-24)
- **Beschreibung:** 98 Skeleton-Header in `include/uft/` deklarieren zusammen
  **1 952 öffentliche `uft_*`-Funktionen ohne Implementation**. Jeder dieser
  Header trägt jetzt einen `/* PLANNED FEATURE — <scope> */`-Banner, so dass
  Consumer vor neuen Call-Sites gewarnt werden.
- **Detailliste:** [`docs/PLANNED_APIS.md`](PLANNED_APIS.md) (auto-generiert
  aus `docs/skeleton_triage.csv`)
- **Workaround:** Bis zur Implementation linken Call-Sites entweder fehl (bei
  tatsächlicher Nutzung) oder die Funktionen sind tot (keine Consumer).
- **Plan:** Implementation erfolgt subsystem-weise in M2/M3 laut `MASTER_PLAN.md`.
  Kein neuer Call darf gegen einen `PLANNED FEATURE`-Header hinzugefügt werden,
  ohne zuerst die Implementation zu liefern oder das Prototyp zu entfernen
  (Master-Plan Regel 1).

---

### M.1 Nicht alle Prinzipien haben automatisierte Tests
- **Status:** MITIGATED (Kern-Audit live, weitere Checks ausstehend)
- **Beschreibung:** Meta-Prinzip A verlangt für jede Zusage einen CI-Test.
  Stand heute:

  | Prinzip / §  | Test(s)                                 | Enforcement |
  |--------------|-----------------------------------------|-------------|
  | 1.1 Sidecar  | `tests/test_loss_report.c` (8)          | ctest       |
  | 1.2 Preflight| `tests/test_preflight.c` (13)           | ctest       |
  | 5.1 Roundtrip| `tests/test_roundtrip_matrix.c` (13)    | ctest       |
  | 7.1–7.3      | `tests/test_spec_status.c` (15)         | ctest       |
  | 7.x (plugin-weit) | `scripts/audit_plugin_compliance.py` | ctest (Python), regression-guard |

  Der neue Plugin-Compliance-Audit scant alle 83 `uft_format_plugin_t`
  Literale unter `src/formats/` und prüft `.spec_status`, `.features`,
  `.compat_entries`, `.is_stub`. Baseline: **5 voll-compliant** (ADF, HFE,
  IPF, STX, WOZ), **15 mit spec_status**. CI failt bei Regression.

  Noch offen:
  - §1 Round-Trip-LL-Tests für konkrete Format-Paare (nur Matrix-API getestet)
  - §3 Fehlermeldungs-Struktur (Fix-Vorschlag + Warum + Was)
  - §6 Emulator-Pipeline im CI
- **Workaround:** `ctest --label-regex principle-compliance` führt alle
  Prinzip-Tests lokal aus.
- **Plan:** Integration der Audit-Baseline in CI-Job (als separater Schritt
  oder im Coverage-Workflow). Monotones Hochsetzen der `--min-pass` /
  `--min-spec-status` Baselines bei jeder Populierungs-Runde.

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
