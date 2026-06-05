# proto/hermes-eval/ — hermes-agent Evaluation Spike

**Goal:** `.claude/goals/v4.1.6-hermes-agent-poc.md`
**Started:** 2026-05-26 (Axel-Override während RC1-Window — Trigger 1+2 noch nicht erfüllt)
**Upstream:** https://github.com/NousResearch/hermes-agent (MIT)
**Status:** 🟡 bootstrap (Session #1), Benchmarks pending

## Eine Validierungs-Frage

> Bringt hermes-agent einen messbaren Vorteil für die UFT-Entwicklungs-
> Pipeline, der die Migrations-Kost aus Claude Code + `.claude/` rechtfertigt?

Antwort am Ende: **GO** oder **NO-GO**. Nicht "vielleicht".

## Layout

```
proto/hermes-eval/
├── README.md              ← du bist hier
├── benchmarks/            ← Task-Skripte (in Folge-Session befüllt)
│   ├── task_a_diff_audit.md     (proxy für stub-eliminator)
│   └── task_b_format_plugin.md  (proxy für uft-format-plugin Skill)
├── prompts/               ← identische Prompts für Claude vs hermes
├── results/               ← RESULTS.md mit gemessenen Zahlen
└── upstream/              ← .gitignored — local clone, nicht committen
```

## Setup-Anleitung (für Folge-Session)

### 1. Upstream klonen (nicht committen)

```bash
cd proto/hermes-eval/
git clone https://github.com/NousResearch/hermes-agent.git upstream
# upstream/ ist in proto/hermes-eval/.gitignore
```

### 2. Python venv

```bash
cd upstream/
uv venv .venv
source .venv/bin/activate   # oder .venv\Scripts\activate auf Windows
uv pip install -e .
```

### 3. Modell-Endpoint konfigurieren

Editiere `proto/hermes-eval/.env` (aus `.env.example` kopieren). NIE
echte Keys committen — `.env` ist in `.gitignore`.

Empfohlene Endpoints für den Spike (Reihenfolge der Präferenz):
- **OpenRouter** mit z.B. `anthropic/claude-3.5-sonnet` für fairen
  Apples-to-Apples Vergleich gegen Claude Code
- **Nous Portal** (Native, falls hermes-agent-optimiert)
- Lokales Modell falls verfügbar (mistral, qwen) — disqualifiziert vom
  GO-Vergleich aber gut zum Sanity-Check ob hermes überhaupt läuft

### 4. Smoke-Test

Bevor Benchmarks: einfacher Hallo-Welt-Lauf von hermes-agent gegen den
gewählten Endpoint. Wenn das schon scheitert → STOP-Bedingung "Setup
braucht > 1 Tag" tritt ein.

## Benchmark-Tasks

Identische Prompts an Claude Code UND hermes-agent. Messen:

- **Task A** — Audit der letzten 5 Commits eines disposable Test-Branches
  auf neue Lazy-Stubs (per `agents/stub-eliminator.md` 6-Spalten-Tabelle)
- **Task B** — Scaffold ein neues UFT-Format-Plugin (z.B. "tap" — bewusst
  nicht-real, um keine ungewünschte echte Codebase-Änderung anzuziehen)

Genaue Prompts: `prompts/`. Genaue Bewertungs-Skala: `results/RUBRIC.md`
(in nächster Session anlegen).

## Failure-Mode-Tests (KO-Kriterien)

hermes-agent muss alle drei bestehen, sonst direkter NO-GO:

1. **ARCH-7 Verständnis:** Kann hermes-agent erklären warum ADFCopy und
   Applesauce dieselbe Teensy-VID/PID teilen und warum das gefährlich
   ist, OHNE die Codebase zu durchsuchen?
2. **Anti-Pattern-Erkennung:** Erkennt hermes-agent
   `if (!feature) return DEFAULT;` als verbotenen Workaround-Stub per
   v3-Vollständigkeits-Regel?
3. **Honest-vs-Lazy:** Kann hermes-agent die 6 Spalten der Honest-Stub-
   Definition aus `agents/stub-eliminator.md` aufzählen?

Test-Prompts: `prompts/failure_modes/` (Session #2).

## GO-/NO-GO-Schwellen (aus Goal-File)

| Outcome | Schwelle |
|---|---|
| GO | hermes ≥ 2× besser in Wall-Clock **ODER** Output-Qualität bei vergleichbarer Korrektheit |
| NO-GO | verfehlt ≥ 1 Failure-Mode-Test ODER > 30 % schlechter Wall-Clock |
| Mittlere Werte | NO-GO (sunk-cost-Verteidigung) |

## STOP-Bedingungen für den Spike

- Setup > 1 Tag (Reife-Annahme falsch)
- hermes crasht auf Tasks (Reife-Annahme falsch)
- Token-Cost > 5× Claude-Code-Cost
- Spike will mehr als 50 Files außerhalb `proto/hermes-eval/` anfassen
- Eine `.claude/`-Datei müsste geändert werden für den Spike selbst

## Was diese Session geliefert hat (Session #1)

- ✅ Skeleton: `proto/hermes-eval/{benchmarks,results,prompts}/`
- ✅ `.gitignore` für upstream-clone, venv, .env
- ✅ `.env.example` Template (ohne echte Keys)
- ✅ README mit Setup-Schritten, Schwellen, STOP-Bedingungen

## Was Session #2 liefern muss (next-up)

- Upstream-Clone + venv lokal aufsetzen (manuell — braucht Internet + Disk)
- Smoke-Test gegen ein Modell-Endpoint
- `prompts/task_a_diff_audit.md` mit echtem Prompt
- `prompts/task_b_format_plugin.md` mit echtem Prompt
- `prompts/failure_modes/{arch7,anti_pattern,honest_lazy}.md`
- Bewertungs-Rubrik `results/RUBRIC.md`

## Was Session #3+ liefern muss (PoC-Lauf)

- Tatsächliche Benchmark-Läufe — beide Tools, identische Prompts
- Token-Cost-Messung pro Lauf
- Wall-Clock-Messung pro Lauf
- Output-Qualität manuell bewerten (1-5)
- `results/RESULTS.md` mit GO/NO-GO-Verdict + Daten

## Annahmen, die der PoC NICHT validiert

(per Goal-File — bewusst weggelassen, Scope-Disziplin):

- Long-term Wartbarkeit von hermes-agent (Single-Vendor Nous Research)
- Performance unter Multi-User-Last (UFT ist Single-User Desktop — irrelevant)
- Lizenz-Kompatibilität mit UFT downstream
- Migration der ML-Skill-Subset (uft-ml-*) — eigene spätere Frage
- Multi-Platform-Messaging-Features (UFT braucht das nicht)
