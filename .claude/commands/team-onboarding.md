# /team-onboarding — Neue Session ins UFT-Projekt einarbeiten

**Zweck:** Jede neue Claude Code Session (oder neuer KI-Mitarbeiter) durchläuft
diesen Command **vor der ersten Code-Änderung**. Ergebnis: deterministischer
Kontext-Stand, korrekte Tool-Auswahl (welcher Agent / welcher Skill für
welche Frage), keine Confabulation über Projekt-Stand.

**Wann aufrufen:**
- Session-Start, *bevor* die erste echte Aufgabe beginnt
- Nach einem Branch-Wechsel oder größeren Merge
- Wenn du als User merkst „die Session weiß nicht mehr was los ist"
- Nach einem Release-Tag (Stand frisch laden)

**Wann NICHT:**
- Innerhalb einer laufenden Aufgabe (würde Kontext überschreiben)
- Bei reinen Bugfix-Sessions mit konkretem File-Scope
  (dort: direkt das File öffnen, Onboarding ist zu schwer)

---

## Phase 1 — Pflicht-Leseliste (in dieser Reihenfolge)

Lies **vollständig**, nicht überfliegen. Notiere für dich was unklar bleibt
und welche Fragen offen sind — die kommen in Phase 3.

| # | Datei | Warum |
|---|---|---|
| 1 | `CLAUDE.md` (Root) | Projekt-Identität, Mission, Kern-Metriken |
| 2 | `docs/DESIGN_PRINCIPLES.md` | 7 verbindliche Prinzipien — bei Konflikt **gewinnen** sie |
| 3 | `docs/AI_COLLABORATION.md` | Arbeits-Reihenfolge, Output-Format, Konfidenz-Regeln |
| 4 | `.claude/CONSULT_PROTOCOL.md` | Wie Agenten zusammenarbeiten, Richtungsregel |
| 5 | `docs/KNOWN_ISSUES.md` | Was aktuell NICHT funktioniert + Plan dazu |
| 6 | `docs/REFACTOR_TASKS.md` | Aktueller Stand P0…P3, was offen ist |
| 7 | `CHANGELOG.md` (nur Top-Section) | Was zuletzt gemerged wurde |
| 8 | `VERSION.txt` + git tag (latest) | Aktuelle Version + letzter Tag |

**Bei v4.1.5-Kontext zusätzlich:**

| # | Datei | Warum |
|---|---|---|
| 9 | `docs/M3_HAL_PLAN.md` | Welche HW-Backends fehlen, M3.1…M3.7 Status |
| 10 | `audit/MASTER_REPORT.md` | ARCH-1…ARCH-9 Findings, was offen/closed |
| 11 | `V415_GOAL_PLAN.md` (falls vorhanden) | v4.1.5 Sub-Goals + Akzeptanzkriterien |

---

## Phase 2 — Context-Scan (deterministisch)

Führe folgende Befehle aus und merke dir die Ergebnisse:

```bash
# Wo stehen wir?
git status
git log --oneline -10
git tag --sort=-creatordate | head -5
cat VERSION.txt

# Welche Branches gibt es lokal/remote?
git branch -a | head -15

# Open PRs (falls gh-cli verfügbar)
gh pr list --state open 2>/dev/null || echo "no gh cli"

# Letzte CI-Status (falls relevant)
gh run list --limit 5 2>/dev/null || true

# Konsistenz-Schnellcheck
python3 scripts/check_consistency.py
python3 scripts/verify_build_sources.py
```

**Erwartete Antworten** (passt eine nicht: STOPP, User informieren):
- `check_consistency.py` → `0/0/0/0 OK`
- `verify_build_sources.py` → `NEW regressions A/B: 0/0`
- Aktuelle Version in `VERSION.txt` matched den jüngsten Release-Tag oder
  ist im Übergangszustand (z.B. `4.1.4` mit `v4.1.4-rc1` als jüngstem Tag)

---

## Phase 3 — Knowledge-Check (an dich selbst)

**Beantworte die folgenden Fragen für dich, bevor du dem User etwas
zusagst.** Wenn du eine Frage nicht beantworten kannst: zurück zu Phase 1
oder den User fragen — **nicht** raten.

1. **Was ist die aktuelle UFT-Version laut `VERSION.txt`?**
2. **Was war der letzte Release-Tag und wann wurde er erstellt?**
3. **Auf welchem Branch arbeite ich gerade?**
4. **Welche P-Aufgaben sind in `REFACTOR_TASKS.md` aktuell offen
   (⬜ oder 🔄)?**
5. **Welche 3 Design-Prinzipien wären verletzt wenn ich…**
   - … einen Decoder schreibe der bei CRC-Fehler einen Default-Wert zurückgibt?
   - … ein Format-Plugin registriere ohne `spec_status` zu setzen?
   - … einen Hardware-Provider stub mit `return UFT_OK` ausstatte?
6. **Welcher Agent wäre zuständig für…**
   - … einen Hinweis dass `xum1541_provider_v2.cpp` einen Feld-Alias schreibt?
   - … ABI-Risiken bei einem neuen Feld in `uft_format_plugin_t`?
   - … einen Stub der nur `return 0;` macht?
7. **Welches Skill würde ich öffnen wenn ich…**
   - … einen neuen MFM-Decoder für ein japanisches Format schreibe?
   - … einen neuen Kopierschutz-Erkenner brauche?
   - … einen HAL-Backend für ein neues USB-Gerät scaffolden will?

**Wenn du eine Frage > 2 mal nicht beantworten kannst, ist Phase 1 nicht
ausreichend gelesen worden — wiederhole.**

---

## Phase 4 — Tool-Inventur

Liste auf was dir zur Verfügung steht. Format:

```
AGENTS (21 in .claude/agents/):
  Must-Fix-Prävention:    consistency-auditor, must-fix-hunter, single-source-enforcer, stub-eliminator
  Coordination:           orchestrator, deep-diagnostician, preflight-check, quick-fix
  Architecture:           type-system-architect, abi-bomb-detector, structured-reviewer
  Forensics:              forensic-integrity
  Refactor-spezifisch:    dto-migrator, provider-migrator, wiring-codegen-author
  Test:                   conformance-test-writer, differential-test-author, improvement-test-author
  Build/Perf:             proof-of-concept-builder, algorithm-hotpath-optimizer, github-expert

SKILLS (21 = 20 UFT + 1 HTB in .claude/skills/):
  Code-Korrektheit:    uft-recovery-integrity, uft-crc-engine, uft-format-plugin,
                       uft-format-converter, uft-deepread-module, uft-filesystem,
                       uft-protection-scheme, uft-coding-standards-compliance
  Build/Test/Perf:     uft-cross-platform-build, uft-debug-session, uft-benchmark,
                       uft-flux-fixtures, uft-stm32-portability, uft-release
  UI/HAL:              uft-qt-widget, uft-hal-backend
  Machine Learning:    uft-ml-implement-skeleton, uft-ml-protection-classifier,
                       uft-ml-training-data
  Meta:                uft-llm-prompt-engineering
  Nicht-UFT:           htb-mentor

PROTOKOLLE:
  CONSULT_PROTOCOL.md  → Cross-Agent-Konsultation (Block-Format, Richtungsregel)
  AI_COLLABORATION.md  → 7-Schritt-Arbeits-Pipeline, Output-Format
```

---

## Phase 5 — Output an den User

Antworte am Ende **knapp** im folgenden Format (keine Wall of Text):

```
═══ ONBOARDING ABGESCHLOSSEN ═══

Stand:
  Version:    <VERSION.txt>
  Branch:     <git branch>
  Letzter Tag: <git tag latest>
  Letzter Commit: <SHA + Titel>

Offene P-Tasks: <Anzahl ⬜> open, <Anzahl 🔄> in progress

CI: <grün/rot>  ·  Consistency: <OK/Fail>  ·  Build-Parity: <OK/Drift>

Frischer Befund / Unklarheit:
  - <falls etwas im Knowledge-Check unklar blieb>
  - <falls Phase-2-Check unerwartete Werte zeigte>

Bereit für: <konkrete Aufgabentypen für die ich jetzt
            gerüstet bin: z.B. "M3.1 SCP libusb Wiring",
            "ARCH-7 VID/PID Fix", "Loss-Report Wiring">

Welche Aufgabe nehmen wir?
```

---

## Was dieser Command NICHT tut

- **Keine Code-Änderungen.** Onboarding ist read-only.
- **Keine Sub-Agent-Spawns.** Wenn beim Lesen ein Verdacht entsteht
  („das müsste der must-fix-hunter prüfen"), notiere ihn — der User
  entscheidet ob gespawnt wird.
- **Kein Commit.** Auch nicht `update_version.sh` o.ä.
- **Keine Branch-Wechsel.** Die Session bleibt auf dem Branch wo sie
  startete.
- **Kein Tag, kein Push, kein PR-Merge.** Niemals.

---

## Failure-Modes (was tun wenn…)

| Symptom | Aktion |
|---|---|
| `CLAUDE.md` nicht im Root gefunden | Wir sind im falschen Repo. Abbrechen, User fragen. |
| `check_consistency.py` 0/0/0/0 ≠ wahr | STOPP. Nicht weitermachen. User informieren — möglicherweise gemerged während Drift. |
| `REFACTOR_TASKS.md` zeigt P-Task in 🔄 ohne Commit-Marker | Nachfragen ob die Aufgabe abgeschlossen oder unterbrochen wurde. |
| Knowledge-Check Frage >2x unbeantwortbar | Re-read Phase 1. Falls weiterhin: User fragen. |
| Agent / Skill in der Inventur fehlt | Notieren — möglicherweise wurde was gelöscht oder noch nicht committed. User informieren. |

---

## Optional: Schneller Pfad (`/team-onboarding --quick`)

Wenn nur Phase 2 + Output (5) reicht (z.B. Session lief gerade, nur State-Refresh):

```bash
git status && git log --oneline -3 && cat VERSION.txt
python3 scripts/check_consistency.py
```

Dann direkt Output-Block ausgeben, Knowledge-Check skip.

**Verwende den Quick-Pfad NICHT bei einer komplett neuen Session.** Phase 1
+ 3 sind die Versicherung gegen confabulation — die kostet ~2-3 Min, spart
aber später 30+ Min „warum hast du das so gemacht?"-Diskussionen.

---

## Versionierung

Command-Version: **v1.0**
Letzte Änderung: 2026-05
Pflege: Synchron mit `CLAUDE.md` und `docs/AI_COLLABORATION.md` halten.
