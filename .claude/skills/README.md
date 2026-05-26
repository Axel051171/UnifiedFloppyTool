# UFT Skills

20 UFT-Skills + 1 HTB-Skill für die Arbeit an UFT mit Claude Code (oder
anderen LLMs, via `uft-llm-prompt-engineering`). Jeder Skill kapselt
einen wiederkehrenden Workflow als Trigger + Templates + Verifikation.

Authoring-Konventionen: `docs/SKILL_AUTHORING_GUIDE.md`.

## Index

### Code-Korrektheit & Datenintegrität

| Skill | Wann |
|-------|------|
| `uft-recovery-integrity` | Code in `src/recovery/` — Sektor/Bitstream-Recovery, Voting, CRC-Korrektur |
| `uft-crc-engine` | CRC/Checksum-Implementierung oder -Debug |
| `uft-format-plugin` | Neuer Format-Plugin (80+ existieren bereits) |
| `uft-format-converter` | Format → Format Konvertierung |
| `uft-deepread-module` | Neues DeepRead/OTDR-Pipeline-Modul |
| `uft-filesystem` | Filesystem-Layer (FAT, ProDOS, AmigaDOS, …) |
| `uft-protection-scheme` | Kopierschutz-Detektor / Klassifizierer |
| `uft-coding-standards-compliance` | Master-Coding-Standards-Lücken schließen (H-1/H-2/H-9, F-2/F-4, D-2, SPEC_STATUS, capability manifest) |

### Build / Test / Performance

| Skill | Wann |
|-------|------|
| `uft-cross-platform-build` | Build-Fehler auf Linux (Ubuntu/Debian) / macOS / Windows (MinGW/MSVC), CI-Matrix red, Qt-Versions-Bump |
| `uft-debug-session` | Bug-Repro → Isolieren → Fix → Regression-Test |
| `uft-benchmark` | Hotpath-Performance messen, Range statt Punktwert |
| `uft-flux-fixtures` | Synthetische Testdaten (SCP, HFE, ADF, D64, KryoFlux) |
| `uft-stm32-portability` | Firmware-Portability-Check (dual-target Code) |
| `uft-release` | Version-Bump, Pre-Release-Checks |

### UI / HAL

| Skill | Wann |
|-------|------|
| `uft-qt-widget` | Neues Qt6-Widget für UFT-GUI |
| `uft-hal-backend` | Hardware-Abstraktion (Greaseweazle, FluxEngine, SCP, …) |

### Machine Learning

| Skill | Wann |
|-------|------|
| `uft-ml-implement-skeleton` | Skeleton-Header → echte Implementierung |
| `uft-ml-protection-classifier` | Cosine-Similarity-Classifier erweitern |
| `uft-ml-training-data` | Labeled training data generieren |

### Meta

| Skill | Wann |
|-------|------|
| `uft-llm-prompt-engineering` | LLM auf UFT-Aufgabe briefen (Templates für 6 Aufgabentypen) |

### Nicht-UFT (separat verwaltet)

| Skill | Wann |
|-------|------|
| `htb-mentor` | HackTheBox/CTF-Tutoring — sokratisches Lernen statt Lösungen, getrennter Kontext, nicht für UFT-Tasks |

## Welcher Skill für welche Aufgabe?

Mehr als ein Skill kann zutreffen. Faustregel:

- **Bytes erzeugen, die der User später vertraut** → `uft-recovery-integrity`
- **Neue Datei lesen können** → `uft-format-plugin`
- **Bestehende Datei woandershin schreiben** → `uft-format-converter`
- **Build kaputt auf irgendeiner Platform** → `uft-cross-platform-build`
- **Etwas geht kaputt, das vorher ging** (Build OK) → `uft-debug-session`
- **Etwas ist langsam** → `uft-benchmark` (nicht `uft-debug-session`)
- **Test braucht Eingabe** → `uft-flux-fixtures`
- **Code muss auf STM32 laufen** → `uft-stm32-portability`
- **Anderes LLM auf UFT briefen** → `uft-llm-prompt-engineering`

## Was diese Skills NICHT sind

- **Keine LLM-Integration in UFT selbst** — UFT ruft keine LLM-APIs auf.
  Diese Skills sind für deine Arbeit *mit* Claude/GPT/Gemini am
  UFT-Code, nicht für UFT-interne LLM-Calls.
- **Kein Replacement für `AI_COLLABORATION.md`** — die Skills
  referenzieren und ergänzen das Dokument.
- **Keine generische C-Best-Practice** — jeder Skill ist UFT-spezifisch.
  Wenn etwas in jedem C-Projekt gilt, gehört es nicht hierher (sondern
  in `CLAUDE.md`).

## Installation

```bash
cd <UFT-repo-root>
unzip uft_skills_v2.zip
mkdir -p .claude/skills
mv skills/uft-* .claude/skills/
mkdir -p .claude/docs
cp -r skills/docs/* .claude/docs/   # falls noch nicht vorhanden
```

## Verifikation

```bash
# Skill-Anzahl
ls .claude/skills/uft-*/SKILL.md | wc -l   # erwartet: 20

# Keine TODO-Marker
grep -l "TODO" .claude/skills/uft-*/SKILL.md   # erwartet: leer

# Cross-References valide
for f in .claude/skills/uft-*/SKILL.md; do
  grep -oE '\.claude/skills/uft-[a-z0-9-]+' "$f" | while read ref; do
    name=${ref##*/}
    [ -d ".claude/skills/$name" ] || echo "$f → $name MISSING"
  done
done   # erwartet: leer

# Lint-Recovery-Script auf einer Recovery-Datei laufen lassen
bash .claude/skills/uft-recovery-integrity/scripts/lint_recovery.sh \
    src/recovery/uft_sector_recovery.c
```

## Was sich gegenüber v1 geändert hat

Siehe `CHANGELOG.md` neben dieser README.

## Kontext für andere LLMs

Wenn du einen anderen LLM (GPT, Gemini, lokales Modell) auf UFT briefen
willst:

1. Passenden Template aus `uft-llm-prompt-engineering/templates/` paste
   (es gibt jetzt 6: `code_review`, `perf_review`, `skeleton_impl`,
   `new_plugin`, `refactor`, `bug_repro`)
2. Relevanten Block aus `AI_COLLABORATION.md` paste
3. Konkrete Datei-Referenzen mit `file:line`

Erwarte vom anderen LLM:

- TL;DR (≤2 Sätze)
- Findings mit `file:line`
- Ranges statt Punktwerte für Performance-Claims
- "Not checked"-Sektion am Ende

Wenn diese Struktur fehlt → das LLM hat den Prompt nicht ernst
genommen.
