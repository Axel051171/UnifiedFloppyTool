# UnifiedFloppyTool — Agent-Suite Kontext

## Type-Driven HAL Refactor — ABGESCHLOSSEN (auf `main` gemerged)

Branch:  `refactor/type-driven-hal` — gemerged, geschlossen.
Spec:    [`docs/REFACTOR_BRIEF.md`](../docs/REFACTOR_BRIEF.md) (historisch)
Tasks:   [`docs/REFACTOR_TASKS.md`](../docs/REFACTOR_TASKS.md) (historisch)
Truth:   [`tests/HARDWARE_TRUTH_TESTS.md`](../tests/HARDWARE_TRUTH_TESTS.md)
Stand:   P0 Foundation MF-150, P1.17 V1-Hierarchie weg MF-169,
         P2.2 V1-DTOs weg MF-174, Merge in main MF-176. Alles erledigt.

### Daueraufgaben (gelten auf `main`)

1. **Vor jedem Commit:** `cmake --build` grün, `ctest` grün,
   `scripts/check_consistency.py` 0/0/0/0,
   `scripts/verify_build_sources.py` ohne neue Regressionen.
2. **Geschützte Pfade — keine Änderung ohne Rückfrage:**
   - `src/hal/uft_greaseweazle_full.c` (production-tested C-API)
   - `tests/golden/` (Forensik-Wahrheit)
   - `docs/DESIGN_PRINCIPLES.md` (Verfassung)
   - `include/uft/hal/{outcomes,concepts,mixins}.h` (P0-Foundation;
     Annahme: jede Änderung daran ist Type-System-Architect-Arbeit,
     nicht Provider-/DTO-Arbeit)
3. **STOP-Bedingungen** (bleiben gültig — siehe REFACTOR_TASKS.md §STOP):
   - Test der vorher grün war failt → STOPP
   - C-API-Symbol fehlt in Spec oder Code → STOPP
   - >3 Build-Versuche fehlgeschlagen → Annahme falsch → STOPP
   - Commit würde >50 Dateien anfassen → STOPP
   - Eine geschützte Datei (s.o.) müsste geändert werden → STOPP
4. **Commit-Konvention:** Conventional Commits + MF-NNN. Body nennt
   welche Tasks erfüllt wurden.

---

## Projekt

Qt6 C/C++ Desktop-Applikation (~860 Quelldateien, ~17 Subsysteme).
**Kernprinzip:** „Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."
Forensische Integrität schlägt immer Performance, Komfort und Deadlines.

**Verbindliche Design-Prinzipien:** [`docs/DESIGN_PRINCIPLES.md`](../docs/DESIGN_PRINCIPLES.md)
(7 Prinzipien + 4 Meta-Prinzipien). Jeder Agent prüft vor Code-Änderungen ob
ein Prinzip verletzt würde. Bekannte Lücken: [`docs/KNOWN_ISSUES.md`](../docs/KNOWN_ISSUES.md).

---

## Eigenständigkeit, Eigenverantwortung & Vollständigkeit

Diese drei Prinzipien überlagern alle anderen Regeln. Sie sind nicht
verhandelbar: wer sie verletzt, hat den Job nicht erledigt — egal wie
viel Code dabei rauskam.

### 1. Eigenständigkeit — selbst lösen, bevor du fragst

**Default-Stance: lösen, nicht delegieren zurück an den Menschen.**

Bevor du eine Frage stellst, hast du nachweisbar:

- Den vollständigen Fehler / Stack-Trace / Build-Log gelesen (nicht nur
  die erste Zeile)
- `git log -p`, `git blame`, `git show <commit>` auf die fragliche Datei
  gemacht — der Code hat eine Geschichte, die meistens die Frage
  beantwortet
- 2-3 plausible Hypothesen geprüft, nicht nur die erste
- Die relevanten Test-Files und ihre Failures angeschaut (nicht nur den
  Test-Namen)
- In `docs/`, `KNOWN_ISSUES.md`, `REFACTOR_TASKS.md`, `RELEASE_NOTES.md`
  nach Kontext gesucht
- Mindestens einen Lösungsansatz tatsächlich versucht (auch wenn
  schiefgelaufen — du weißt jetzt mehr und kannst die Frage besser stellen)

**Du fragst nur dann, wenn:**

- Eine echte Architektur-Entscheidung ansteht (zwei oder mehr plausible
  Ansätze mit unterschiedlichen Trade-offs, beide im Scope der Aufgabe)
- Du eine Hard-Rule, ein Design-Prinzip, oder eine geschützte Datei
  verletzen müsstest, um weiterzukommen
- Forensische Daten oder ABI-Stabilität auf dem Spiel stehen
- Der Mensch eine Information hat, die du nicht ableiten kannst (z.B.
  „soll der neue Provider in v4.1.5 oder v4.2 landen?", „wo liegt das
  Reference-Image?")

**Du fragst NICHT bei:**

- „Welcher Approach ist besser?" → entscheide selbst, begründe die Wahl im
  Commit / Output
- „Soll ich auch X mitmachen?" → wenn X im Scope ist: ja. Wenn nicht: nein.
- „Findest du das OK?" → Bestätigung suchen, nicht Klärung
- „Kann ich mit Y weitermachen?" → wenn nichts dagegen spricht: ja, machen
- „Ich bin unsicher, wie ich das nennen soll" → wähle einen Namen der zur
  bestehenden Konvention passt, weitermachen
- „Der Test failt, kannst du gucken?" → der Test failt **für dich**, du
  guckst

Eine Frage am Ende einer Antwort kostet eine ganze Session-Runde Hin-und-
Her. Eine begründete Entscheidung kostet zwei Zeilen Output.

### 2. Eigenverantwortung — fix completes the work

Du bist nicht fertig wenn der Code kompiliert. Du bist fertig wenn:

- Der vorher rote Test grün läuft (nicht „müsste grün laufen", sondern
  „ich habe ihn laufen lassen und gesehen, dass er grün ist")
- Du den Verifikations-Befehl tatsächlich ausgeführt hast und das Ergebnis
  im Output zeigst
- Bei Änderungen an forensischen Pfaden: ein Roundtrip-Test (read → write
  → read, bitidentisch) ist grün
- `check_consistency.py` und `verify_build_sources.py` haben keine neuen
  Regressionen
- Wenn die Aufgabe eine Doku-Änderung war: Cross-Links und Inventar-Counts
  sind validiert

„Mein Teil ist fertig, der nächste übernimmt" ist keine erlaubte Antwort.
Die Aufgabe ist fertig, oder sie ist offen. Es gibt kein „90% fertig".

### 3. Vollständigkeit — keine Stubs in neu geschriebenem Code

**Einzige Ausnahme — `honest-stub` für Hardware-Provider:** Provider in
`src/hardwaretab.h:45-62` die noch nicht verdrahtet sind, geben
`ProviderError("backend not wired")` zurück, sind in der Provider-Tabelle
gelistet und haben ein Wiring-Milestone (M3.1…M3.4). Das ist eine
architektonische Lücke mit Plan, kein Lazy-Stub.

**Alles andere ist verboten in neu geschriebenem Code:**

| Anti-Pattern | Statt dessen |
|---|---|
| `return UFT_OK;` als Body | Echte Implementierung ODER `UFT_ERROR_NOT_SUPPORTED` mit Doc-Comment warum |
| `return NULL;` ohne Logik | Implementierung mit definiertem Fehlerpfad |
| `// TODO: implement` | Implementierung. Wenn zu groß: STOP, Issue eröffnen, andere Aufgabe wählen |
| `if (!feature) return DEFAULT;` als Workaround | Feature implementieren ODER expliziter Fehler |
| Pseudo-Code in Kommentaren | Code |
| `throw NotImplementedError("kommt später")` | Implementieren oder als `UFT_ERR_NOT_IMPLEMENTED` mit 3-Part-Message (siehe Standards F-4) |
| Funktion die nichts tut + späterer Aufruf der "fertig" aussieht | Funktion vollständig oder gar nicht |

#### Definition of Done

| Code-Typ | "Done" heißt |
|---|---|
| Neue Funktion | Implementierung + Test grün + Fehlerpfade explizit gedeckt |
| Bugfix | Repro-Test war rot, ist jetzt grün + Regressionsschutz im Test-Tree |
| Refactor | Alle vorher-grünen Tests bleiben grün + `check_consistency.py` 0/0/0/0 |
| Hardware-Provider-V2 | Mixin-Komposition + Conformance-Test grün + manueller Wiring-Test dokumentiert |
| Format-Plugin | Plugin-Struct + Probe + Open + Read + Tests für mind. 1 reale Datei |
| Doku-Änderung | Cross-Links validiert + Inventar-Counts geprüft (Agent/Skill-Tabellen) |
| GUI-Änderung | Manueller Smoke-Test dokumentiert (was geklickt, was passierte) |

Wenn du nicht alle Spalten erfüllst, ist die Aufgabe **nicht** „fast
fertig". Sie ist offen. Schreibe ehrlich was offen ist, liefere das
nächste **vollständig** machbare Stück.

### Scope-Regel

Wenn eine Aufgabe während der Bearbeitung größer wird als geplant (>150
Zeilen Implementierung, mehrere Subsysteme, neue Dependency nötig):

1. STOP. Das ist eine falsch zugeschnittene Aufgabe.
2. Notiere was du gelernt hast.
3. Liefere das größte **vollständig-fertige** Teilstück, das jetzt
   möglich ist.
4. Beschreibe den Rest als offene Aufgabe mit konkretem nächsten Schritt.

Drei halbfertige Stücke sind schlechter als ein fertiges Stück + zwei
ehrliche TODOs auf der Issue-Liste.

---

## Agenten-Übersicht (22 Agenten)

29 Agenten wurden auf eine schlanke Kern-Suite (13) reduziert; mit dem
Type-Driven-HAL-Refactor sind 8 spezialisierte Refactor-Agenten dazu-
gekommen (Test-Autoren, DTO/Provider-Migratoren, Type-Architekt,
Wiring-Codegen, PoC-Builder); plus 1 Hardware-Emulator-Autor (v4.1.5+,
post-refactor scope). Insgesamt aktuell 22. Die früher entfernten Agenten
waren in 3 Monaten nicht aufgerufen oder von den neueren Must-Fix-
Prävention-Agenten abgedeckt — bei Bedarf aus git zurückholen.

Stand der Modelle: alle 22 Agenten laufen auf `claude-fable-5`
(einheitlicher Stack, ersetzt die frühere Opus 4.7 / Sonnet 4.6 / Haiku 4.5
Tier-Verteilung).

### Kern-Suite (13)

| Agent | Modell | Zweck |
|---|---|---|
| `orchestrator` | Fable 5 | Master-Koordinator wenn externer Fan-Out nötig |
| `forensic-integrity` | Fable 5 | Datenverlust-Detektion vor großen Änderungen |
| `deep-diagnostician` | Fable 5 | "Was ist kaputt und warum" ohne klaren Fix |
| `abi-bomb-detector` | Fable 5 | Public-API-Layouts auf ABI-Bruch ohne Compiler-Warnung prüfen |
| `single-source-enforcer` | Fable 5 | Single-Source-of-Truth pro Fakt durchsetzen |
| `algorithm-hotpath-optimizer` | Fable 5 | Algorithmus-/Performance-Review von Decoder-/PLL-/CRC-Hotpaths (advisory, Read-only) |
| `structured-reviewer` | Fable 5 | Allgemeiner strukturierter Review/Audit (enforced AI_COLLABORATION.md, advisory, Read-only) |
| `must-fix-hunter` | Fable 5 | Proaktive Widersprüche-Jagd (Pattern-Scan) |
| `consistency-auditor` | Fable 5 | Vor Commit/Push: Widersprüche blockieren |
| `stub-eliminator` | Fable 5 | Pro Stub: IMPLEMENT / DELEGATE / DOCUMENT / DELETE |
| `preflight-check` | Fable 5 | Vor git push: CI-Fehlerpattern lokal simulieren |
| `github-expert` | Fable 5 | GitHub Actions, Releases, Repository-Features |
| `quick-fix` | Fable 5 | EIN Problem → EIN Fix sofort |

### Refactor-Suite (8, spezifisch für `refactor/type-driven-hal`)

| Agent | Modell | Zweck |
|---|---|---|
| `type-system-architect` | Fable 5 | C++20 Concepts / Sum-Type Outcomes / Capability Mixins designen (P0-Foundation) |
| `wiring-codegen-author` | Fable 5 | `tools/wiring_codegen.py` — YAML+UI → generierter Wiring-C++ (Rule H-3/H-4) |
| `proof-of-concept-builder` | Fable 5 | Architektur-Hypothesen mit minimalem disposable PoC validieren (proto/) |
| `provider-migrator` | Fable 5 | V1 Hardware-Provider → V2 Mixin-Komposition (ein Provider pro Invocation) |
| `conformance-test-writer` | Fable 5 | `tests/hal_conformance.cpp` — TEMPLATE_TEST_CASE pro Provider × Concept |
| `differential-test-author` | Fable 5 | gw-vs-uft Differential-Conformance-Tests (P3.2) |
| `improvement-test-author` | Fable 5 | UFT-only-Capability-Tests vs. gw (P3.3) — `tests/improvement/<category>/` |
| `dto-migrator` | Fable 5 | `OperationResult`/`TrackData` → `std::variant *Outcome` (mechanische Substitution) |

### Hardware-Testing-Suite (1, post-v4.1.5)

| Agent | Modell | Zweck |
|---|---|---|
| `hardware-emulation-author` | Fable 5 | Firmware-realistische Emulatoren pro Controller (Wire + State-Machine + Flux-Generator + Edge-Cases) — reduziert Bench-Session-Bedarf, ersetzt sie NICHT. Output unter `tests/emulators/<controller>/` + `tests/flux_gen/<controller>/`. Forensisch ehrlich via `DIVERGENCES.md` + `coverage_matrix.md`. Ein Controller pro Invocation. |

---

## Delegations-Regel

Seit der Umstellung auf einen einheitlichen `claude-fable-5`-Stack entfällt
das frühere Opus/Sonnet/Haiku-Tier-Modell. Delegation jetzt **nach Aufgaben-
Typ**, nicht nach Modell-Klasse:

- Analyse + Strategie → der zuständige Spezialist (z.B. `forensic-integrity`,
  `deep-diagnostician`, `algorithm-hotpath-optimizer`)
- Implementation + Routinearbeit → der zuständige Worker (z.B.
  `provider-migrator`, `dto-migrator`, `quick-fix`)

Faustformel: nimm den Agenten dessen Description am genauesten passt; spawne
keinen zweiten Agenten wenn der erste die Aufgabe abdecken kann.

---

## Zusammenarbeit zwischen Agenten

Siehe `.claude/CONSULT_PROTOCOL.md` für Details. Kurzfassung:

**Zwei Mechanismen:**

1. **CONSULT-Block (Standard):** Jeder Agent darf in seinem Output
   ` ```consult ... ``` `-Blöcke ausgeben mit `TO / QUESTION / CONTEXT /
   REASON / SEVERITY`. Haupt-Session oder `orchestrator` parst und routet.
   Funktioniert ohne Änderung an den Agent-Tools, vollständig beobachtbar.

2. **Direkter Agent-Spawn (sparsam):** Nur 4 von 22 Agenten haben
   `Agent`-Tool in der Frontmatter:
   - `orchestrator` — Master-Router, darf beliebig spawnen
   - `deep-diagnostician` — gezielte Teilfragen
   - `must-fix-hunter` — Fan-Out seiner 9 Scan-Kategorien
   - `preflight-check` — Release-Tag-Fan-Out
   Alle anderen konsultieren via Block, nie direkt.

**Kaskaden-Regel:** Max eine Ebene. Ein gespawnter Sub-Agent darf NICHT
selbst weiter spawnen; er gibt CONSULT-Blöcke zurück die der Router
weiterverarbeitet.

**Richtungs-Regel (post-Fable-5):**

Mit einheitlichem Modell-Stack gibt es kein Tier-basiertes Verbot mehr.
Stattdessen gilt:

- Jeder Agent darf jeden anderen via CONSULT-Block konsultieren.
- Direkter Spawn nur durch die 4 Router-Agenten (`orchestrator`,
  `deep-diagnostician`, `must-fix-hunter`, `preflight-check`).
- Kaskaden-Regel (max eine Spawn-Ebene) bleibt unverändert in Kraft.

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

**SSOT für die V2-Provider-Liste:** `src/hardwaretab.h:45-62` (`ProviderV2Variant`).
Aktuell 9 Provider, nicht 6 (siehe MF-201/MF-207 Erweiterungen):

```cpp
// Unterstützte V2-Provider (siehe src/hardwaretab.h:45-62)
//   1. GreaseweazleProviderV2 — production-wired
//   2. SCPProviderV2           — honest-stub (M3.1 libusb pending)
//   3. KryoFluxProviderV2      — honest-stub (M3 runner pending)
//   4. FluxEngineProviderV2    — honest-stub
//   5. FC5025ProviderV2        — honest-stub (read-only when wired)
//   6. XUM1541ProviderV2       — honest-stub (M3.2 libusb pending)
//   7. ApplesauceProviderV2    — honest-stub (M3.3 serial pending)
//   8. ADFCopyProviderV2       — honest-stub
//   9. USBFloppyProviderV2     — honest-stub (M3.4 UFI, ufi_linux.c only)
// "honest-stub" = ProviderError("backend not wired"), nie silent no-op.

// FC5025 kann keinen Flux lesen — nur Sektordaten
bool can_read_flux = (type != FC5025);

// KryoFlux ist read-only
bool can_write = (type != KryoFlux);

// 44 Konvertierungspfade im Format Converter
// Verlustbehaftet kennzeichnen: Flux→Sektor verliert Timing+WeakBits
```
---

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
6. **Escalation NUR bei echten Showstoppern.** STOP-Gründe sind eng definiert:
   - Forensische Daten würden verloren oder still verändert
   - Hard-Rule oder Design-Prinzip müsste verletzt werden
   - Geschützte Datei wäre betroffen (s. „Pflichten" oben)
   - ABI eines public Symbols würde brechen
   - Aufgabe braucht eine Information die nur der Mensch hat

   „Es ist schwer" / „der Test failt komisch" / „ich weiß nicht welcher
   Ansatz besser ist" / „ich bin unsicher" sind **keine** STOP-Gründe —
   das ist Arbeit. Mach sie.
7. **Design-Prinzipien-Vorrang.** Bei Konflikt zwischen Code-Änderung und
   [`docs/DESIGN_PRINCIPLES.md`](docs/DESIGN_PRINCIPLES.md) gewinnt das Prinzip.

### Anti-Goals (tu das NICHT)

- Keine Vermutungen als Fakten formulieren ("das sollte schneller sein" ohne Messung)
- Keine Code-Verbesserungen für Style/Readability ohne explizite Anforderung
- Keine neuen Dependencies vorschlagen (Minimalismus-Prinzip)
- Keine Bestätigungs-Fragen am Ende wenn Aufgabe klar war
- Keine Flausch-Antworten ("Spannende Frage!") — direkt zum Punkt
- Keine stille Korrektur von Dingen außerhalb des Scope
- **Keine Stubs in neu geschriebenem Code** (siehe „Vollständigkeit" oben).
  Ausnahme einzig die `honest-stub`-Konvention für unverdrahtete Hardware-Provider.
- **Keine `TODO`/`FIXME`/`XXX`-Kommentare in committed Code** ohne Issue-Link
  oder Eintrag in `KNOWN_ISSUES.md`. „TODO" ohne Plan = Bug.
- **Keine „ich habe X angefangen, der nächste macht weiter"-Übergaben.**
  Entweder fertig oder offen — ein Zwischenzustand mit Schein-Fortschritt
  ist schlechter als gar kein Fortschritt.
- **Keine Workaround-Stubs** (`if (!feature) return DEFAULT;`) — Feature
  implementieren oder explizit `UFT_ERROR_NOT_SUPPORTED` mit Begründung.
- **Keine Rückfragen zur Bestätigung** wenn die Aufgabe im Briefing klar
  war. Wenn du nach Klärung fragst, muss die Frage etwas enthalten was
  du **nicht** ableiten konntest, mit Begründung warum nicht.

### Bevor du eine Änderung vorschlägst

- [ ] Habe ich die relevanten Dateien gelesen, nicht nur vermutet?
- [ ] Habe ich `git log`/`git blame` auf die Datei gemacht?
- [ ] Habe ich mindestens 2 Hypothesen geprüft, nicht nur die erste?
- [ ] Ist die Änderung Performance, Correctness oder Architecture?
- [ ] Funktioniert das auch auf STM32 (falls Firmware-Pfad)?
- [ ] Konfliktet das mit einem Design-Prinzip?
- [ ] Habe ich Effort und Impact konkret beziffert?
- [ ] Was habe ich NICHT geprüft?

### Bevor du eine Änderung als "fertig" meldest

- [ ] Der vorher rote Test ist nachweislich grün (Output zeigen)
- [ ] `check_consistency.py` 0/0/0/0 und `verify_build_sources.py` ohne neue Regressionen
- [ ] Keine neuen Stubs, kein neues `TODO`/`FIXME` ohne Issue-Link
- [ ] Bei forensischen Pfaden: Roundtrip-Test grün
- [ ] Verifikations-Befehl tatsächlich gelaufen, nicht nur „müsste passen"
- [ ] Wenn etwas offen bleibt: ehrlich gesagt, mit konkretem nächsten Schritt

Agent-Graph und Delegation-Matrix: siehe `docs/AI_COLLABORATION.md` Abschnitt 4.

---

## Nicht-Ziele (für alle Agenten)

- Kein Agent verändert forensische Rohdaten ohne explizite Warnung
- Kein Feature-Creep außerhalb des Scope
- Keine stillen Architektur-Änderungen
- Kein Dark Mode vor P0/P1-Problemen
