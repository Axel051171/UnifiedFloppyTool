# UnifiedFloppyTool — Agent-Suite Kontext

## Projekt

Qt6 C/C++ Desktop-Applikation (~860 Quelldateien, ~17 Subsysteme).
**Kernprinzip:** „Kein Bit verloren." Forensische Integrität schlägt immer Performance, Komfort und Deadlines.

---

## Agenten-Übersicht (konsolidiert auf 10)

29 Agenten wurden reduziert auf 10 aktiv genutzte. Die entfernten Agenten
waren in 3 Monaten nicht aufgerufen oder von den neueren Must-Fix-Prävention-
Agenten abgedeckt (siehe git-history für Details). Wenn ein seltener Einzelfall
doch einen Spezialisten braucht, aus git zurückholen.

| Agent | Modell | Zweck |
|---|---|---|
| `orchestrator` | Opus | Master-Koordinator wenn externe Fan-Out nötig |
| `forensic-integrity` | Opus | Datenverlust-Detektion vor großen Änderungen |
| `deep-diagnostician` | Opus | "Was ist kaputt und warum" ohne klaren Fix |
| `must-fix-hunter` | Opus | Proaktive Widersprüche-Jagd (Must-Fix-Prävention) |
| `single-source-enforcer` | Opus | Single-Source-of-Truth durchsetzen |
| `consistency-auditor` | Sonnet | Vor Commit/Push: Widersprüche blockieren |
| `stub-eliminator` | Sonnet | Pro Stub: IMPLEMENT / DELEGATE / DOCUMENT / DELETE |
| `preflight-check` | Sonnet | Vor git push: CI-Fehlerpattern lokal simulieren |
| `github-expert` | Sonnet | GitHub Actions, Releases, Repository-Features |
| `quick-fix` | Sonnet | EIN Problem → EIN Fix sofort |

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
