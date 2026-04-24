# UnifiedFloppyTool — Agent-Suite Kontext

## Projekt

Qt6 C/C++ Desktop-Applikation (~860 Quelldateien, ~17 Subsysteme).
**Kernprinzip:** „Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."
Forensische Integrität schlägt immer Performance, Komfort und Deadlines.

**Verbindliche Design-Prinzipien:** [`docs/DESIGN_PRINCIPLES.md`](../docs/DESIGN_PRINCIPLES.md)
(7 Prinzipien + 4 Meta-Prinzipien). Jeder Agent prüft vor Code-Änderungen ob
ein Prinzip verletzt würde. Bekannte Lücken: [`docs/KNOWN_ISSUES.md`](../docs/KNOWN_ISSUES.md).

---

## Agenten-Übersicht (13 Agenten)

29 Agenten wurden reduziert auf 13 aktiv genutzte. Die entfernten Agenten
waren in 3 Monaten nicht aufgerufen oder von den neueren Must-Fix-Prävention-
Agenten abgedeckt (siehe git-history für Details). Wenn ein seltener Einzelfall
doch einen Spezialisten braucht, aus git zurückholen.

Stand der Modelle: `claude-opus-4-7` / `claude-sonnet-4-6` (aktuelle
Flaggschiffe).

| Agent | Modell | Zweck |
|---|---|---|
| `orchestrator` | Opus 4.7 | Master-Koordinator wenn externer Fan-Out nötig |
| `forensic-integrity` | Opus 4.7 | Datenverlust-Detektion vor großen Änderungen |
| `deep-diagnostician` | Opus 4.7 | "Was ist kaputt und warum" ohne klaren Fix |
| `abi-bomb-detector` | Opus 4.7 | Public-API-Layouts auf ABI-Bruch ohne Compiler-Warnung prüfen |
| `single-source-enforcer` | Opus 4.7 | Single-Source-of-Truth pro Fakt durchsetzen |
| `algorithm-hotpath-optimizer` | Opus 4.7 | Algorithmus-/Performance-Review von Decoder-/PLL-/CRC-Hotpaths (advisory, Read-only) |
| `structured-reviewer` | Opus 4.7 | Allgemeiner strukturierter Review/Audit (enforced AI_COLLABORATION.md, advisory, Read-only) |
| `must-fix-hunter` | Sonnet 4.6 | Proaktive Widersprüche-Jagd (Pattern-Scan, Sonnet reicht) |
| `consistency-auditor` | Sonnet 4.6 | Vor Commit/Push: Widersprüche blockieren |
| `stub-eliminator` | Sonnet 4.6 | Pro Stub: IMPLEMENT / DELEGATE / DOCUMENT / DELETE |
| `preflight-check` | Sonnet 4.6 | Vor git push: CI-Fehlerpattern lokal simulieren |
| `github-expert` | Sonnet 4.6 | GitHub Actions, Releases, Repository-Features |
| `quick-fix` | Sonnet 4.6 | EIN Problem → EIN Fix sofort |

---

## Kosten-Regel (wichtig)

**Opus → Sonnet für Sub-Tasks.** Opus-Agenten rufen bei Bedarf Sonnet-Agenten auf,
nicht andere Opus-Agenten.

Faustformel:
- Analyse + Strategie = Opus
- Implementation + Routinearbeit = Sonnet

---

## Zusammenarbeit zwischen Agenten

Siehe `.claude/CONSULT_PROTOCOL.md` für Details. Kurzfassung:

**Zwei Mechanismen:**

1. **CONSULT-Block (Standard):** Jeder Agent darf in seinem Output
   ` ```consult ... ``` `-Blöcke ausgeben mit `TO / QUESTION / CONTEXT /
   REASON / SEVERITY`. Haupt-Session oder `orchestrator` parst und routet.
   Funktioniert ohne Änderung an den Agent-Tools, vollständig beobachtbar.

2. **Direkter Agent-Spawn (sparsam):** Nur 4 von 13 Agenten haben
   `Agent`-Tool in der Frontmatter:
   - `orchestrator` — Master-Router, darf beliebig spawnen
   - `deep-diagnostician` — gezielte Teilfragen
   - `must-fix-hunter` — Fan-Out seiner 9 Scan-Kategorien
   - `preflight-check` — Release-Tag-Fan-Out
   Alle anderen konsultieren via Block, nie direkt.

**Kaskaden-Regel:** Max eine Ebene. Ein gespawnter Sub-Agent darf NICHT
selbst weiter spawnen; er gibt CONSULT-Blöcke zurück die der Router
weiterverarbeitet.

**Richtungs-Regel (Opus↔Sonnet):**

| von → zu | erlaubt |
|---|---|
| Sonnet → Sonnet | ja |
| Sonnet → Opus | ja (Architektur-Frage nach oben) |
| Opus → Sonnet | ja (Standard-Delegation) |
| Opus → Opus | nein (außer über `orchestrator`) |

---

## Konflikt-Hierarchie

| Konflikt | Gewinner |
|---|---|
| Forensik vs. Performance | Forensik |
| Forensik vs. UX | Forensik |
| Security vs. Kompatibilität | Security |
| Architektur vs. Quick-Fix | Architektur (außer P0-Crash) |

---

## Dateipfade (Standard)

```
~/uft/
  src/           Qt6 C++ Quellcode
  tests/
    vectors/     synthetische Flux-Vektoren (test-master)
  docs/
  .claude/
    CLAUDE.md    ← diese Datei
    agents/      ← alle Agenten
```

---

## Wichtige Konstanten

```cpp
// Unterstützte Controller
enum ControllerType { Greaseweazle, SuperCardPro, KryoFlux, FC5025, FluxEngine, Applesauce };

// FC5025 kann keinen Flux lesen — nur Sektordaten
bool can_read_flux = (type != FC5025);

// KryoFlux ist read-only
bool can_write = (type != KryoFlux);

// 44 Konvertierungspfade im Format Converter
// Verlustbehaftet kennzeichnen: Flux→Sektor verliert Timing+WeakBits
```
---

```markdown
## AI-Zusammenarbeit

Dieses Projekt hat verbindliche Arbeits-Prinzipien für alle KI-Assistenten
(Claude Code, Claude Chat, GPT, Gemini, etc.). Vollständiges Dokument:
[`docs/AI_COLLABORATION.md`](docs/AI_COLLABORATION.md).

### Hard-Rules (gelten immer, keine Ausnahmen)

1. **Lies bevor du antwortest.** Keine Aussagen über ungelesenen Code.
   Verwende `Read`/`Grep`/`Glob` aktiv, nicht passiv.
2. **Struktur vor Prosa.** Output-Format: TL;DR → nummerierte Findings mit
   `file:line` → Vorher/Nachher-Code → Effort/Impact → "Nicht geprüft".
3. **Konfidenz als Range.** Speedup immer "5-8×" nie "Faktor 7". Wenn nicht
   schätzbar: "needs measurement". Keine erfundenen Zahlen.
4. **Perf ≠ Korrektheit.** Fixes klassifizieren: Performance / Correctness /
   Architecture. Correctness-Fixes erfordern Tests.
5. **Hardware-Dualität respektieren.** Desktop (x86-64, AVX2, BMI2) vs. STM32H723
   (Cortex-M7, SP-FPU, 564KB SRAM). Immer fragen: "läuft das auf Firmware?"
6. **Escalation statt Alleingang.** Bei Korrektheits-Bugs, Undefined Behavior
   oder nötigem API-Break: STOP und flaggen, nicht weiterarbeiten.
7. **Design-Prinzipien-Vorrang.** Bei Konflikt zwischen Code-Änderung und
   [`docs/DESIGN_PRINCIPLES.md`](docs/DESIGN_PRINCIPLES.md) gewinnt das Prinzip.

### Anti-Goals (tu das NICHT)

- Keine Vermutungen als Fakten formulieren ("das sollte schneller sein" ohne Messung)
- Keine Code-Verbesserungen für Style/Readability ohne explizite Anforderung
- Keine neuen Dependencies vorschlagen (Minimalismus-Prinzip)
- Keine Bestätigungs-Fragen am Ende wenn Aufgabe klar war
- Keine Flausch-Antworten ("Spannende Frage!") — direkt zum Punkt
- Keine stille Korrektur von Dingen außerhalb des Scope

### Bevor du eine Änderung vorschlägst

- [ ] Habe ich die relevanten Dateien gelesen, nicht nur vermutet?
- [ ] Ist die Änderung Performance, Correctness oder Architecture?
- [ ] Funktioniert das auch auf STM32 (falls Firmware-Pfad)?
- [ ] Konfliktet das mit einem Design-Prinzip?
- [ ] Habe ich Effort und Impact konkret beziffert?
- [ ] Was habe ich NICHT geprüft?

Agent-Graph und Delegation-Matrix: siehe `docs/AI_COLLABORATION.md` Abschnitt 4.
```

---

## Nicht-Ziele (für alle Agenten)

- Kein Agent verändert forensische Rohdaten ohne explizite Warnung
- Kein Feature-Creep außerhalb des Scope
- Keine stillen Architektur-Änderungen
- Kein Dark Mode vor P0/P1-Problemen
