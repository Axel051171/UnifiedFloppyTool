# UnifiedFloppyTool — Agent-Suite Kontext

## Projekt

Qt6 C/C++ Desktop-Applikation (~860 Quelldateien, ~17 Subsysteme).
**Kernprinzip:** „Kein Bit verloren." Forensische Integrität schlägt immer Performance, Komfort und Deadlines.

---

## Agenten-Übersicht

| Agent | Modell | Zweck |
|---|---|---|
| `orchestrator` | Opus | Master-Koordinator, Fan-Out zu Spezialisten |
| `architecture-guardian` | Opus | Struktur-Analyse, läuft VOR allen anderen |
| `forensic-integrity` | Opus | Datenverlust-Detektion, kritischster Agent |
| `format-decoder` | Opus | 44+ Formate, Parser-Korrektheit |
| `hal-hardware` | Opus | HAL + Controller-Protokolle (GW/SCP/KF/FC5025) |
| `platform-expert` | Opus | C64/Amiga/Atari/AppleII Encoding-Details |
| `copyprotection-analyst` | Opus | Kopierschutz-Erkennung + Forensik |
| `flux-signal-analyst` | Opus | PLL, Jitter, OTDR-Pipeline |
| `info-finder` | Opus | Taktische Web-Intelligence (CVE, Firmware, Specs) |
| `research-roadmap` | Opus | Strategische Roadmap, Wettbewerb |
| `innovation-catalyst` | Opus | Ideen + Spezialisten-Review |
| `security-robustness` | Sonnet | Parser-Vulnerabilities, Integer-Overflows |
| `refactoring-agent` | Sonnet | C++/Qt6 Code-Qualität |
| `test-master` | Sonnet | Coverage-Analyse + synthetische Flux-Vektoren |
| `performance-memory` | Sonnet | Profiling, Memory, Threading |
| `gui-expert` | Sonnet | Qt6 GUI: Workflow + Strukturkonflikte |
| `compatibility-import-export` | Sonnet | 44 Konvertierungspfade, Roundtrip |
| `dependency-scanner` | Sonnet | Lizenzen, CVEs, Qt6/LGPL-Risiken |
| `build-ci-release` | Sonnet | qmake/CMake, CI, Cross-Platform |
| `github-expert` | Sonnet | GitHub Actions, Releases, Security |
| `release-manager` | Sonnet | End-to-End Release-Koordination |
| `documentation` | Sonnet | Docs-Gap-Analyse, Onboarding |
| `preflight-check` | Sonnet | Vor git push: alle 62 CI-Fehler lokal simulieren |
| `header-consolidator` | Opus | L-Refactoring: Duplikate, Error-Systeme, Format-Registries |
| `code-auditor` | Sonnet | C++/Qt6 Audit → findings.json → Routing |
| `quick-fix` | Sonnet | EIN Problem → EIN Fix sofort |
| `code-auditor` | Sonnet | Python-Tool Audit + Routing |
| `code-fixer` | Sonnet | Python-Tool Fixes (Exceptions, Backup, Audit) |
| `threading-fixer` | Sonnet | Python GUI-Blocker → Thread-Worker |
| `test-fixer` | Sonnet | Python Selftest + Roundtrip hinzufügen |

---

## Kosten-Regel (wichtig)

**Opus → Sonnet für Sub-Tasks.** Opus-Agenten rufen bei Bedarf Sonnet-Agenten auf, nicht andere Opus-Agenten. Ausnahme: forensic-integrity und security-robustness können sich gegenseitig konsultieren, da beide Opus sind und sicherheitskritisch.

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
