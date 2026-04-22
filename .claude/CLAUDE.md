# UnifiedFloppyTool — Agent-Suite Kontext

## Projekt

Qt6 C/C++ Desktop-Applikation (~860 Quelldateien, ~17 Subsysteme).
**Kernprinzip:** „Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."
Forensische Integrität schlägt immer Performance, Komfort und Deadlines.

**Verbindliche Design-Prinzipien:** [`docs/DESIGN_PRINCIPLES.md`](../docs/DESIGN_PRINCIPLES.md)
(7 Prinzipien + 4 Meta-Prinzipien). Jeder Agent prüft vor Code-Änderungen ob
ein Prinzip verletzt würde. Bekannte Lücken: [`docs/KNOWN_ISSUES.md`](../docs/KNOWN_ISSUES.md).

---

## Agenten-Übersicht (11 Agenten)

29 Agenten wurden reduziert auf 11 aktiv genutzte. Die entfernten Agenten
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

## Nicht-Ziele (für alle Agenten)

- Kein Agent verändert forensische Rohdaten ohne explizite Warnung
- Kein Feature-Creep außerhalb des Scope
- Keine stillen Architektur-Änderungen
- Kein Dark Mode vor P0/P1-Problemen
