# UFT LLM/ML Skills Package

4 Skills für die Arbeit an UFT mit LLMs und ML-Modulen.

## Inhalt

```
skills/
├── uft-ml-implement-skeleton/      # Skeleton-Header zu echten Implementations
├── uft-ml-protection-classifier/   # Erweitern von uft_ml_protection.c
├── uft-ml-training-data/           # Labeled training data generieren
└── uft-llm-prompt-engineering/     # Prompts für LLM-Briefings
```

## Welcher Skill für welche Aufgabe?

| Wenn du... | Skill |
|------------|-------|
| Eine Funktion aus `uft_ml_decoder.h` oder `uft_ml_training_gen.h` implementieren willst | `uft-ml-implement-skeleton` |
| Ein neues Protection-Schema zum Cosine-Classifier hinzufügen willst | `uft-ml-protection-classifier` |
| Trainingsdaten für die ML-Module brauchst | `uft-ml-training-data` |
| Eine Aufgabe an Claude/GPT delegierst und gute Prompts willst | `uft-llm-prompt-engineering` |

## Was diese Skills NICHT sind

- **Keine LLM-Integration in UFT selbst** — UFT ruft keine LLM-APIs auf.
  Diese Skills sind für deine Arbeit *mit* Claude/GPT/Gemini am UFT-Code,
  nicht für UFT-interne LLM-Calls.
- **Kein Replacement für AI_COLLABORATION.md** — die Skills referenzieren
  und ergänzen das Dokument, aber ersetzen es nicht.

## Was du wissen musst

### Skeleton-Status in der heutigen UFT-Version

Mein Audit-Script hat **98 Header mit `UFT_SKELETON_PLANNED` Banner**
gefunden — nicht nur im ML-Bereich. Die zwei ML-spezifischen sind:

- `include/uft/ml/uft_ml_decoder.h` — 20 Funktionen, alle skeleton
- `include/uft/ml/uft_ml_training_gen.h` — 21 Funktionen, alle skeleton

Plus die **eine implementierte ML-Datei**:
- `src/analysis/uft_ml_protection.c` — Cosine-Similarity-Classifier,
  pure C, firmware-portabel

### Prompt-Engineering-Realität

Der Skill `uft-llm-prompt-engineering` adressiert die 7 LLM-Failure-Modes,
die ich in unserer Konversation tatsächlich erlebt habe — siehe
`reference/common_llm_mistakes.md`. Das Dokument ist eine ehrliche
Bestandsaufnahme, nicht Theorie.

## Installation

```bash
cd <UFT-repo-root>
unzip uft_llm_skills.zip
mv skills/* .claude/skills/
# oder neben die existierenden 13 Skills setzen
```

Verifikation:

```bash
# Audit-Script auf der echten Codebase
python3 .claude/skills/uft-ml-implement-skeleton/scripts/audit_skeleton.py
# zeigt alle Skeleton-Header und ob Banner-Counts stimmen

# Training-Data-Generator
python3 .claude/skills/uft-ml-training-data/scripts/gen_synthetic_training.py --n-clean 2 --n-protected 1
# erzeugt Samples in tests/training/synthetic/
```

## Kontext für andere LLMs

Wenn du einen anderen LLM (GPT, Gemini, lokales Modell) auf UFT briefen
willst, paste:

1. Den passenden Template aus `uft-llm-prompt-engineering/templates/`
2. Den relevanten Block aus `AI_COLLABORATION.md`
3. Konkrete Datei-Referenzen mit file:line

Erwarte vom anderen LLM:
- TL;DR (≤2 Sätze)
- Findings mit file:line
- Ranges statt Punktwerte für Performance-Claims
- "Not checked"-Sektion am Ende

Wenn diese Struktur fehlt → das LLM hat den Prompt nicht ernst genommen.
