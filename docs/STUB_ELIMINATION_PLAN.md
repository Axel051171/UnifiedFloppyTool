# UFT Stub Elimination Plan

**Stand:** 2026-06-10, post Audit-Closing (siehe `KNOWN_ISSUES.md`).
**Owner:** `stub-eliminator` Agent (Pro-Stub-Entscheidung: IMPLEMENT /
DELEGATE / DOCUMENT / DELETE).

## Bestandsaufnahme (verifizierte Counts — korrigiert MF-289)

> **Zahlen-Korrektur 2026-07-02:** Die Erst-Fassung dieses Plans übernahm
> die stale „287 Format-Stubs" aus KNOWN_ISSUES §7.3 (Zensus von VOR dem
> MF-011-Cleanup). Realer Bestand nachgemessen:

| # | Kategorie | Anzahl | Quelle | Aktueller Status |
|---|---|---|---|---|
| **C1** | Pattern-A Alt-Parser (`*_parser_v3.c`, kein Plugin, kein `is_stub`) | **11** | `find src/formats -name "*_parser_v3.c"` | Rest der ehemals 287; Triage nötig |
| **C2** | Skeleton-Header (≥10 decls, ≥80 % missing) | **133** | `scripts/audit_skeleton_headers.py` | banner-markiert (M1-Wellen) |
| **C3** | Unimplementierte `uft_*`-Deklarationen in C2 | **2613** | dito | überlappt mit C5 |
| **C4** | HAL honest-stubs (USB/Serial wiring pending) | 12 | `src/hal/uft_xum1541.c` (6) + `uft_applesauce.c` (6) | KNOWN_ISSUES M.3 — honest-stub |
| **C5** | `uft_core_stubs.c` Residual-Implementierungen | ~17 Funktionen | `src/core/uft_core_stubs.c` (post-A07) | unflagged, A07-Klasse |
| **C6** | ADF Write-Side | 3 | `src/formats/uft_adf.c:897/907/1013` | KNOWN_ISSUES §7.4 — honest-stub |
| **C7** | Recovery / Salvage scaffolds | 3 doc-comments | `src/recovery/uft_salvage_fs.c` | XCOPY_INTEGRATION_TODO.md — by-design |
| **C8** | Sonstige Stub-Marker in `src/` (Triage nötig) | 39 Dateien | diverse | unbewertet |

**Total ≈ 2800 zu erfassende Items** (dominiert von C3). Nicht alle sind
„Lazy Stubs"; ein nennenswerter Teil ist *honest-stub* mit dokumentiertem
Plan und gehört NICHT eliminiert — er wird in der Inventar-Spalte
„DOCUMENT" verankert. Die 84 registrierten Plugins sind NICHT pauschal
Stubs — 41 % haben reale Tests; die per-Plugin-Triage (`is_stub`-Flag)
bleibt Phase-1-Arbeit.

## Konvention: honest vs. lazy

Reproduktion aus `.claude/CLAUDE.md` zur Klarheit:

| Eigenschaft | honest-stub (BEHALTEN) | lazy-stub (ELIMINIEREN) |
|---|---|---|
| Rückgabe | `UFT_ERR_NOT_IMPLEMENTED` / `-1` / `ProviderError("backend not wired")` | `return UFT_OK;` / `return NULL;` / `if (!feature) return DEFAULT;` |
| Tracking | Eintrag in KNOWN_ISSUES + Milestone-ID + Source-Verweis | nichts oder „// TODO: implement" |
| Caller-Wahrnehmung | sieht „nicht implementiert" und kann reagieren | sieht „erfolgreich" obwohl nichts passierte |
| Forensik-Verstoß | nein | ja (DESIGN_PRINCIPLE 4 + 7) |

Ein lazy-stub im neu geschriebenen Code ist ein **Bug**, kein TODO.

## Phasen-Plan

### Phase 1 — Triage & Inventarisierung — ✓ ABGESCHLOSSEN (MF-291, 2026-07-02)

**Ziel:** Jedes Item in IMPLEMENT / DELEGATE / DOCUMENT / DELETE
einsortieren — verbindlich. Ohne diese Sortierung läuft jede Folge-Phase ins
Blaue.

**Gate-Artefakte (geliefert):**
- `docs/stub_inventory.yaml` — die verbindliche Triage-Tabelle (generiert,
  nicht hand-editieren; via `python scripts/scan_skeleton_callers.py --yaml`)
- `docs/stub_callers.csv` — per-Deklaration Caller-Counts (3109 Zeilen)
- `scripts/scan_skeleton_callers.py` — der Scanner (reproduzierbar)

**Ergebnis:**

| Metrik | Wert |
|---|---|
| Unimplementierte `uft_*`-Deklarationen (ALLE Header, nicht nur ≥10/80 %) | **3109** |
| davon NULL Referenzen in src/+tests/ → DELETE-Kandidat | **2897 (93 %)** |
| davon mit Referenzen → Kaskaden-Analyse Phase 2 | 212 |
| Header komplett referenzlos → DELETE-Kandidat ganz | **170** |
| Header SPLIT (referenzlose Decls löschen, Rest behalten) | 33 |
| Header voll referenziert → DOCUMENT/IMPLEMENT | 8 |
| C1 v3-Parser: DELETE-ready / REVIEW | 7 / 4 |
| C5 core-stub-Fns: DELETE-Kandidat / DOCUMENT / A07-Duplikate | 5 / 17 / **0** |
| C8 Stub-Marker-Dateien: honest / unmarkiert (Phase-2-Triage) | 7 / 21 |

Der 50 %-DOCUMENT-Deckel aus der Risiko-Tabelle ist deutlich eingehalten —
DELETE dominiert mit 93 %. Wichtige Interpretation: ein Caller-Count > 0
bei einer UNIMPLEMENTIERTEN Funktion heißt, der referenzierende Code kann
selbst nicht linken (tot/unbuilt) oder ein Test erwartet eine geplante
API — beides Phase-2-Kaskaden-Arbeit.

**Bewusste Abweichung vom ursprünglichen Task-Zuschnitt:** Das geplante
`populate_is_stub.py` (287 Plugins mechanisch flaggen) entfiel mit der
Zensus-Korrektur — die 84 registrierten Plugins sind mehrheitlich real;
per-Plugin `is_stub`-Triage wandert als manuelle Review-Aufgabe in
Phase 2 (Signalquelle: Plugin-Test-Existenz + read_track-Body-Analyse,
kein blinder Sweep).

### Phase 2 — Mechanische Aufräumung (v4.1.6, ~1-2 Wochen)

**Ziel:** alle eindeutig identifizierten DELETE-Items und mechanischen
IMPLEMENTs landen. Keine Architektur-Entscheidungen, kein Format-Wissen
nötig.

| Aufgabe | Erwartete Wirkung | Aktion |
|---|---|---|
| Header-Decls ohne Caller → DELETE | C3 schrumpft drastisch (geschätzt 30-60 % von 1473) | `quick-fix` agent löscht pro Header die unrebenutzten Decls |
| `uft_core_stubs.c` Residuals à la A07 | C5 → 0; Datei shrumpft oder verschwindet | `provider-migrator`-Pattern an Klasse anpassen |
| Plugin `is_stub=true` Sweep | C1 wird audit-transparent | `populate_is_stub.py` + CI-Audit reflektiert echte Lücke |
| Lazy-Stub Detector im pre-commit | künftige lazy-stubs strukturell verhindert | Erweiterung von `scripts/check_consistency.py` um Pattern-Klasse („return UFT_OK; in function body" + „// TODO: implement") |

**Gate für Phase 3:** `check_consistency.py` enthält neue Kategorie
`lazy-stub patterns: 0`, KNOWN_ISSUES §7.3 von MITIGATED → CLOSED.

### Phase 3 — HAL-Wiring (v4.2, ~2 Wochen pro Controller, parallelisierbar)

**Ziel:** C4 von 12 honest-stubs auf 0. Pro Controller ein Worktree, am
Ende ein zusammenhängender Commit pro Controller.

| Controller | Aufwand | Tool/Agent | HW-Bench |
|---|---|---|---|
| M3.2 XUM1541 libusb wiring | ~1 Woche Code + 3 Tage Test | `provider-migrator` + `hardware-emulation-author` (Tier-2.5 zuerst) | UFT-008-Pattern |
| M3.3 Applesauce serial wiring | ~1 Woche | dito | UFT-009 (neu) |
| M3.4 USBFloppy (UFI) | ~2 Wochen (komplexer SCSI-CDB-Layer) | dito | UFT-010 (neu) |

**Gate für Phase 4:** Alle drei HAL-Tests grün auf dem Emulator + signed-off
HW-Bench-Report pro Controller.

### Phase 4 — Format-Plugin Tier-Implementierung (v4.2 bis v4.3+, multi-month)

**Ziel:** C1 echte Parser pro Tier statt nur is_stub-Flag. Größter Brocken,
braucht das meiste Domain-Wissen.

Tier-Klassifizierung (Triage auf User-Demand):

| Tier | Anzahl Formate | Kriterium | Zeitfenster | Beispiele |
|---|---|---|---|---|
| **Tier 1** | ~20 | Top-Demand, im Demo gezeigt | v4.2 (2-3 Monate) | D71, D81, ATR, MSA, TD0, IMD-erweitert |
| **Tier 2** | ~50-80 | Retro-Communities (Amiga/Atari ST/CPC/MSX) | v4.3 (4-6 Monate) | DSK-erweitert, EDSK, STX, NIB |
| **Tier 3** | ~150 | Exotisch / Forschung | opportunistisch | Roland, HP LIF, Thomson, TI-99 spezial |

Pro Plugin-Implementierung:
1. `format-implementation`-Agent oder spezialisierter Provider-Migrator
2. Conformance-Test gegen Reference-Image
3. Roundtrip-Matrix-Eintrag wenn nicht UNTESTED
4. `is_stub=false` setzen (statt löschen — Plugin bleibt registriert)

**Gate für Phase 5:** Pro Tier vorher 0 echte Parser → Tier-Count nach
Phase 4 ist konkret messbar (jeden Monat in `audit_plugin_compliance.py`).

### Phase 5 — Filesystem Write-Operationen (v4.2)

**Ziel:** C6 (ADF) + analoge Lücken in anderen Filesystems (FAT, ProDOS,
CBM) schließen.

| Filesystem | Stubs heute | Aufwand |
|---|---|---|
| ADF (AmigaDOS) | `add_file` + `delete` + abgeleiteter Stub (§7.4) | ~400 LOC, Bitmap + Hash + Checksum |
| FAT12 (PC) | TBD-Audit | unbekannt — Triage in Phase 1 |
| ProDOS | TBD-Audit | unbekannt |
| CBM DOS | TBD-Audit | unbekannt |

**Gate für Phase 6:** je Filesystem mind. eine echte Write-Roundtrip-Test
(create→delete→verify in Emulator).

### Phase 6 — Honest-Stub Lock-In (kontinuierlich)

**Ziel:** verhindern, dass die Aufräumung der Phasen 1-5 schleichend wieder
aufgefressen wird.

| Mechanismus | Wirkung |
|---|---|
| `stub-eliminator` agent in pre-commit hook (Diff-Range) | jeder neue lazy-stub blockt den Commit |
| `consistency-auditor` Pattern-Set („Trivial-Body"-Detection) | nicht-trivialer Funktionsname mit Trivial-Body wird geblockt |
| KNOWN_ISSUES-Eintrag obligatorisch für jeden honest-stub | unverlinkter honest-stub wird via Grep gefunden und gemeldet |
| CI-Job `audit_plugin_compliance.py` regelmäßig | is_stub-Drift wird Diff-sichtbar |

## Strikt-Aufwandsschätzung

| Phase | Realer Aufwand | Kalenderzeit | Parallelisierbar? |
|---|---|---|---|
| 1 — Triage | 1 Person × 1 Woche | 1 Woche | Nein |
| 2 — Mechanik | 1 Person × 1-2 Wochen | 1-2 Wochen | Tools-gestützt |
| 3 — HAL-Wiring | 3 × 1-2 Wochen | 2-3 Wochen wenn parallel | Ja, pro Controller |
| 4 — Tier-Plugins | T1: 2 Pers × 3 Mon | 3-12 Monate je nach Tier | Pro Format |
| 5 — Filesystems | 1 Pers × ~6 Wochen | 6 Wochen | Pro FS |
| 6 — Lock-In | 1 Person × 1 Woche initial, dann durchlaufend | dauerhaft | — |

**Kritischer Pfad bis MITIGATED-CLOSED (KNOWN_ISSUES §7.3 + M.0):**
Phase 1 + 2 → v4.1.6. **Realistisch in ~3 Wochen erreichbar.**

**Kritischer Pfad bis Tier-1-Vollständigkeit:** + Phase 3 + Phase 4 Tier 1
→ v4.2, ~4-5 Monate ab Phase-1-Start.

**Vollständige Stub-Elimination (alle Tiers + alle FS):** ist ein
mehrjähriges Projekt und wird nicht versprochen. Stattdessen: kontinuierliche
Tier-Auswahl nach User-Demand.

## Risiken & Anti-Pattern

| Risiko | Gegenmaßnahme |
|---|---|
| Triage wird zu „erstmal alles `DOCUMENT`" | Phase-1-Gate-Review: max 50 % DOCUMENT erlaubt; Rest muss IMPLEMENT/DELEGATE/DELETE sein |
| Tier-1-Auswahl politisch statt user-driven | Issue-Tracker-Stimmen / GitHub-Reactions als Auswahl-Kriterium |
| HAL-Wiring landet ohne HW-Bench | UFT-008-Pattern als Gate — keine HAL-Wiring-Closure ohne signed-off HW-Test |
| Lazy-Stubs schleichen über CI ein | Phase-6-Mechanismen MÜSSEN VOR Phase 2 stehen, sonst rollback unmittelbar |
| Plan wird wie der frühere `project_stub_conversion.md` zur Karteileiche | Quartals-Review mit Status-Tabelle im Repo (nicht extern), Owner = `stub-eliminator` agent |

## Was diese Datei nicht ist

- Keine Liste aller 1832 Items mit Einzelentscheidung (das ist
  `stub_inventory.yaml` aus Phase 1)
- Kein Hardware-Bench-Protokoll (siehe `tests/HARDWARE_TRUTH_TESTS.md`)
- Kein Format-Plugin-Spec-Dokument (siehe `docs/DESIGN_PRINCIPLES.md` §7)

## Cross-Refs

- `docs/KNOWN_ISSUES.md` §7.3 (287 Stub-Parser), §7.4 (ADF), M.0 (Skeleton-Banner), M.3 (HAL honest-stubs)
- `.claude/agents/stub-eliminator.md` — pro-Stub-Entscheidungs-Agent
- `tests/HARDWARE_TRUTH_TESTS.md` — UFT-008/009/010 Gates für Phase 3
- `scripts/check_consistency.py` — wird in Phase 2 um lazy-stub-Pattern erweitert
- `audit_plugin_compliance.py` — Tier-Fortschritt-Messung in Phase 4
