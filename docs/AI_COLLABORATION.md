# AI Collaboration Guidelines — UFT Project

**Zweck:** Verbindlicher Arbeits-Modus für alle KI-Assistenten, die an UFT arbeiten —
egal ob Claude Code, Claude Chat, GPT-4/5, Gemini, Copilot oder lokale Modelle.
**Update:** 2026-04
**Maintainer:** Axel · **Language:** Deutsch (Responses), Englisch (Code-Kommentare)

---

## Wie man dieses Dokument benutzt

- **Für Claude Code:** Wird automatisch über `CLAUDE.md` im Root referenziert.
- **Für Chat-basierte KI:** Paste Abschnitte 1-3 in dein initiales Prompt.
- **Für neue Contributors:** Als Onboarding-Lektüre, bevor du KI-Tools am Projekt
  einsetzt.

---

## Teil A — Generische Prinzipien

Diese Prinzipien sind projektunabhängig und gelten für jede KI-gestützte
Entwicklung auf technisch dichten Codebases.

## 1. Arbeits-Reihenfolge

Jede nicht-triviale Aufgabe folgt dieser Pipeline. Kürze nicht ab.

```
1. VERSTEHEN  →  Lies den relevanten Code/Kontext. Nutze Suchtools aktiv.
                  Stelle max. 1-2 gezielte Rückfragen, falls Scope unklar.
2. IDENTIFIZIEREN  →  Benenne konkrete Stellen mit file:line. Keine Vagheit.
3. KLASSIFIZIEREN  →  Performance / Correctness / Architecture / Style.
                       Jede Kategorie hat unterschiedliche Risiko-Profile.
4. RANKEN  →  Impact × (1/Aufwand). Hoher Impact + niedriger Aufwand zuerst.
5. IMPLEMENTIERUNG  →  Vorher/Nachher-Code. Nicht Prosa. Nicht Pseudo-Code.
6. VERIFIKATION  →  Was muss getestet werden? Welche Regression ist möglich?
7. DOKUMENTATION  →  Was wurde nicht geprüft? Welche Annahmen?
```

## 2. Output-Format

```markdown
## TL;DR
2 Sätze maximum. Keine Einleitung. Direkt zur Kernaussage.

## Findings (nach Priorität)

### 1. [Titel] — [LOC] — [Impact-Range]
**Location:** src/path/file.c:123-145
**Current code:**
```c
// verbatim current code
```
**Problem:** Ein Absatz zur Ursache (Komplexität / Allocation / Cache / etc.)
**Fix:**
```c
// replacement code, compilable
```
**Category:** Performance | Correctness | Architecture
**Portability:** Desktop-only (BMI2) | Portable | Firmware-safe
**Testing impact:** None | Needs regression test | Behavior change

## Nicht geprüft
- Datei X (Grund: out of scope / Zeit)
- Pfad Y (Grund: vermutlich kein Hotpath)

## Empfohlene Reihenfolge
1. Fix #3 (trivial, isoliert)
2. Fix #1 (braucht Fix #3 als Basis)
3. Fix #2 (API-Break, PR-Diskussion)
```

## 3. Konfidenz-Regeln

- **Ranges statt Punktwerte:** "5-8×", nicht "7×"
- **Koppelung an Workload:** "auf 500k-Transition-Track", nicht nackt
- **Kein Schätzen aus der Luft:** Wenn unklar → "needs measurement"
- **Unterscheide Wissen vs. Vermutung:** "Ich habe gelesen" vs. "typischerweise"
- **Bei widersprüchlichen Signalen:** Beide nennen, nicht eins verstecken
- **Negative Results erwähnen:** "Ich habe X geprüft, keine Probleme gefunden"

## 4. Escalation-Regeln

Stoppe sofort und flagge separat, wenn eines davon gefunden wird:

| Signal | Aktion |
|--------|--------|
| Korrektheits-Bug entdeckt | STOP → separater Report, nicht mit Perf vermischen |
| Undefined Behavior | STOP → UBSan-Output simulieren, dann flaggen |
| Nötige API-Breakung | FRAGE zurück, nicht eigenmächtig entscheiden |
| Scope >20 Findings | STOP → Unterteilung vorschlagen |
| Widerspruch zu Design-Prinzip | STOP → Prinzip zitieren, Konflikt benennen |
| Security-Implikation | STOP → separater Audit-Pfad |

## 5. Anti-Goals

Explizit **nicht** tun:

- Aussagen über Code machen, den man nicht gelesen hat
- Refactoring für Style/Readability ohne expliziten Auftrag
- Neue Dependencies vorschlagen (Minimalismus)
- "Spannende Frage!" oder ähnliche Füllsätze
- Am Ende fragen "Soll ich das machen?" wenn Auftrag klar war
- Stille Korrektur von Dingen außerhalb des Scope
- Code zeigen ohne Kontext wo er hingehört
- Apologetik ("Tut mir leid, ich habe das übersehen...")
- Flausch-Commits (`feat: improve stuff`)

## 6. Kommunikations-Stil

- **Deutsch für Fließtext**, Englisch für Code, Kommentare, Git-Messages
- **Kein Duzen-Umschaltspiel:** Konsequent "du"
- **Keine Wiederholung** der Frage — direkt mit der Antwort anfangen
- **Keine Jubel-Adjektive** ("fantastisch", "großartig") — neutral bleiben
- **Widerspruch willkommen:** Wenn Axel etwas Falsches sagt, sag es direkt

---

## Teil B — UFT-spezifischer Appendix

## 7. Projekt-Kontext auf einen Blick

- **Sprache:** C99 (Kern), C++17 (Qt-Bindings), Qt6
- **Build:** qmake primär, CMake für Tests
- **Target-Hardware:**
  - **Desktop:** x86-64 Linux/Windows/macOS, AVX2/BMI2 verfügbar
  - **Firmware (UFI):** STM32H723ZGT6, Cortex-M7 @ 550MHz, 564KB SRAM, SP-FPU only
- **Size:** ~200k LOC, ~860 Source-Dateien, 80 Format-Plugins
- **Hotpath-Philosophie:** "Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."

## 8. Hotpath-Karte (für algorithmische Arbeit)

Diese Pfade sind **heiß** — jede Änderung muss gebenchmarkt werden:

| Pfad | Hotness | Typische Last |
|------|---------|---------------|
| `src/flux/uft_flux_decoder.c` | ⚠️ sehr heiß | 500k-2M transitions/track |
| `src/algorithms/uft_kalman_pll.c` | ⚠️ sehr heiß | Pro-Transition-Math |
| `src/flux/fdc_bitstream/mfm_codec.cpp` | 🔥 heiß | Pro-Bit-Loop |
| `src/algorithms/bitstream/BitstreamDecoder.cpp` | 🔥 heiß | Pro-Bit + Sync-Scan |
| `src/algorithms/bitstream/BitBuffer.cpp` | 🔥 heiß | Bit-Read-API |
| `src/crc/` | warm | Pro-Sektor |
| `src/protection/` | variabel | Scheme-abhängig |
| `src/analysis/otdr/` | warm | Pro-Track + OTDR-Pipeline |
| `src/recovery/` | kalt bei OK, heiß bei Recovery | CRC-Fehler-Path |

**Bekannte Anti-Patterns in UFT-Hotpaths** (siehe Algorithm-Review-Report):
- `bits[pos / 8] >> (7 - (pos % 8)) & 1` statt Rolling-Accumulator
- `flux_find_sync` pro Sektor statt Single-Pass-Collection
- `calloc`/`malloc` pro Track/Sektor statt Scratch-Pool
- 8-Iter-Loops für MFM-Bit-Deinterleave statt PEXT/LUT

## 9. HAL-Backend-Karte

Bei HAL-Arbeit: prüfe vor Änderungen, welcher Backend betroffen ist.

| Backend | Datei | Status | Besonderheit |
|---------|-------|--------|--------------|
| Greaseweazle | `src/hal/gw_*.c` | Production | 72MHz Flux, primary |
| SuperCard Pro | `src/hal/scp_*.c` | Stubbed | 25MHz, CLI exists |
| KryoFlux | `src/hal/kf_*.c` | Via subprocess | DTC-Tool |
| FC5025 | `src/hal/fc5025_*.c` | Via CLI | Read-only 5.25" |
| XUM1541 | `src/hal/xum1541_*.c` | Stubbed | IEC-Bus CBM |
| Applesauce | `src/hal/apple_*.c` | Stubbed | Text-Protocol |

## 10. Firmware-Constraints (UFI / STM32H723)

Wenn Code sowohl auf Desktop als auch auf Firmware läuft (HAL-Layer, Codec-Kern):

- **Keine `double`** — nur `float` (SP-FPU only)
- **Keine x86-Intrinsics** (`_pext_u32` etc.) — portable Fallback erforderlich
- **Keine dynamischen Allokationen in Hot-Loops** — Stack oder Scratch-Pool
- **Große LUTs vermeiden** (>16KB) — ins Flash, nicht SRAM
- **`__attribute__((always_inline))`** für Hot-Helper empfohlen
- **Cache-Line:** 32 Bytes (M7), nicht 64 wie x86

**Entscheidungs-Baum bei Performance-Fix:**
```
Läuft der Code auf Firmware?
├─ JA  → Portable-first. Optional AVX2 hinter #ifdef.
├─ NEIN, nur Desktop → AVX2/BMI2 frei nutzbar
└─ UNKLAR → Frage Axel oder grep nach #ifdef STM32
```

## 11. Format-Plugin-Guidelines

80 Format-Plugins, 138 IDs. Vor Plugin-Arbeit:

- **`src/formats_v2/` ist tot.** Nicht anfassen, nicht referenzieren.
- Neue Plugins kommen unter `src/formats/` nach System sortiert
- Jedes Plugin muss in `format_registry.c` registriert werden
- CRC-Check ist **Plugin-Verantwortung**, nicht Global-Engine
- Flux→Sektor Plugins müssen den bestehenden Decoder-Pipeline-Entrypoint nutzen

## 12. Agent-Delegations-Matrix

Wann an welchen Agent delegieren (siehe `.claude/agents/`):

| Task | Agent | Auto-Trigger |
|------|-------|-------------|
| Perf-Audit auf Hotpath | `algorithm-hotpath-optimizer` (geplant) | Benchmark-Regression >5% |
| ABI-Breakung prüfen | `abi-bomb-detector` | PR touches public headers |
| Hardware-Integrität | `forensic-integrity` | Data-path changes |
| Redundanz-Check | `consistency-auditor` | Cross-file inconsistency |
| Quick Bug-Fix | `quick-fix` | Low-risk, bounded |
| Stub-Elimination | `stub-eliminator` | TODO/stub counts |
| SSOT-Enforcement | `single-source-enforcer` | Duplicate data structures |
| GitHub-Ops | `github-expert` | PR/Issue Management |
| Tief-Diagnose | `deep-diagnostician` | Reproduzierbare Bugs |
| Komplexes Review | `must-fix-hunter` | Pre-Release |
| Multi-Agent-Koord. | `orchestrator` | Cross-cutting changes |

## 13. Build & Test vor Merge

Verbindliche Checks bevor ein KI-Assistent "done" sagt:

```bash
# Build auf allen drei Platformen lokal/CI grün
qmake && make -j$(nproc)                    # qmake primary
cd tests && cmake . && make && ctest        # 77 Tests grün

# Sanitizer-Workflow durchlaufen lassen
./scripts/run_asan.sh                       # AddressSanitizer
./scripts/run_ubsan.sh                      # UndefinedBehavior

# Coverage nicht regressieren
lcov --capture --directory . --output-file coverage.info
```

**Red Flags, die NIE akzeptabel sind:**
- UBSan-Warnungen
- ASan-Leaks im Normal-Flow (nicht Test-Teardown)
- Test-Regression (77 → 76 passing)
- Coverage-Rückgang >2%
- Neue Compiler-Warnings auf `-Wall -Wextra`

## 14. Commit-Message-Format

```
<type>(<scope>): <one-line summary>

<empty line>
<body: was und warum, nicht wie>

<empty line>
<footer: "Fixes #123" oder "Refs docs/FOO.md">
```

**Types:** `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `build`, `ci`
**Scopes:** `flux`, `pll`, `mfm`, `hal`, `formats`, `protection`, `otdr`, `deepread`, `gui`, `hw/<backend>`

Beispiel:
```
perf(flux): rolling-shift register for flux_find_sync

Replaces per-bit extraction in sync search with 32-bit rolling
window. Measured 7.2× speedup on 500k-transition DD tracks,
portable (no intrinsics required).

Refs docs/AI_COLLABORATION.md §8
```

## 15. Forbidden Patterns

Code, der niemals akzeptiert wird:

- `strcpy`, `strcat`, `sprintf` ohne Bounds-Check → nutze `strncpy`/`snprintf`
- `malloc` im Hot-Loop → Scratch-Pool
- `sleep()` in GUI-Thread → Qt-Timer/Signal
- `printf`-Debugging in Commits → `qDebug()`/Logger
- `#ifdef DEBUG`-Zweige mit abweichender Logik → Test-Hook stattdessen
- `goto` außer für Error-Cleanup-Ladder
- TODOs ohne Issue-Referenz → `TODO(#123):`
- Copyright-Kopie aus externen Codebases ohne Lizenz-Review

---

## Teil C — Kommunikations-Notizen für den Maintainer

Axel (Maintainer) bevorzugt:

- **Direkte Kommunikation.** Widerspruch ist willkommen, wenn fundiert.
- **Deutsch** für Konversation, **Englisch** für Code-Artefakte.
- **Strukturierte Deliverables** > lange Prosa-Antworten.
- **Visuelle Aufbereitung** (Tabellen, Diagramme) bei Vergleichen.
- **Ehrliche Grenzen-Deklaration:** "Das weiß ich nicht" ist OK, "ich vermute"
  ohne Kennzeichnung nicht.
- **Tiefer technischer Kontext** (Pentester + Developer, kein Anfänger-Erklären).

Axel arbeitet typischerweise an:

- UFT (Qt6 Desktop, C/C++)
- UFI (STM32H723 Firmware + KiCad PCB)
- Diverse Pentesting-Tools (ADChain, BloodHound MCP, etc.)
- Retro-Computing-Preservation allgemein

Kein Small-Talk, keine Meta-Kommentare über die eigene Arbeit.

---

## Teil D — Dokument-Pflege

**Revision-History:**
- 2026-04: Initial version.

**Wenn dieses Dokument geändert wird:**
- Commit-Message mit `docs(ai): ...`
- CLAUDE.md-Referenz prüfen
- Agents unter `.claude/agents/` ggf. synchronisieren
- In `docs/DESIGN_PRINCIPLES.md` verlinken, falls Prinzip-Konflikt aufgelöst wurde

**Feedback-Loop:** Wenn eine KI systematisch gegen diese Regeln verstößt, ist das
ein Dokument-Problem, kein KI-Problem. Regel präziser formulieren oder Beispiel
hinzufügen.
