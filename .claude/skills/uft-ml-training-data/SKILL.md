---
name: uft-ml-training-data
description: |
  Use when generating labeled training data for UFT's ML modules — flux
  captures with ground truth, augmented synthetic patterns, or
  protection-classifier reference samples. Trigger phrases: "training
  data für ML decoder", "labeled flux samples generieren", "ground
  truth pairs", "synthetic flux augmentation", "training set für
  classifier", "ml training generator implementieren", "ml_training_gen
  Funktion". Bridges flux fixtures and ML inputs. DO NOT use for: just
  test fixtures (→ uft-flux-fixtures), generating new disk-image
  formats (→ uft-flux-fixtures), implementing the ML decoder itself
  (→ uft-ml-implement-skeleton), labeling existing flux (different —
  that's manual annotation).
---

# UFT ML Training Data

Use this skill when producing labeled training data for UFT's ML modules.
Different from `uft-flux-fixtures` (which makes test inputs) — training
data needs **ground truth labels** alongside the flux.

## When to use this skill

- Implementing functions from `uft_ml_training_gen.h` (21 skeleton fns)
- Generating reference samples for `uft_ml_protection.c` classifier
- Producing flux + sector pairs for the `uft_ml_decoder` (when
  implemented)
- Adding augmentation to expand limited real-disk samples
- Building a held-out validation set for threshold tuning

**Do NOT use this skill for:**
- Pure test fixtures without labels — use `uft-flux-fixtures`
- Generating new disk image formats — use `uft-flux-fixtures` generators
- Implementing the ML decoder/classifier itself — use
  `uft-ml-implement-skeleton` or `uft-ml-protection-classifier`
- Manual labeling of real captures (UI-driven annotation) — different
  workflow, not scripted

## Three training-data classes

| Class | What | Used by |
|-------|------|---------|
| **Real captures + ground truth** | Real disk → flux + verified sectors | Decoder fine-tuning |
| **Synthetic perfect flux** | Programmatically generated, label-by-construction | Decoder pre-training |
| **Augmented (noisy synthetic)** | Synthetic + jitter/dropouts/weak-bits | Robustness training |

You usually need all three: real for realism, synthetic for volume,
augmented for robustness.

## Workflow

### Step 1: Choose what to label and how

| Label content | When you need it |
|---------------|------------------|
| Decoded sectors per track | Decoder training |
| Protection scheme name | Protection classifier training |
| Per-bit confidence | Weak-bit detector training |
| Sector boundaries (offsets) | Sync detector training |

A training sample = `(input_flux, label)` pair. The label format
matters — be specific.

### Step 2: Output format

UFT convention: training samples go to `tests/training/<class>/<n>/`
with a paired structure:

```
tests/training/protection_classifier/
├── vmax_dungeon_master_1.scp        # input
├── vmax_dungeon_master_1.label.json # label
├── vmax_dungeon_master_2.scp
├── vmax_dungeon_master_2.label.json
├── clean_amigados_1.scp
└── clean_amigados_1.label.json
```

Label JSON schema:

```json
{
  "format": "scp",
  "ground_truth": {
    "protection_scheme": "VMAX",
    "track_count": 80,
    "weak_sectors": [{"track": 18, "side": 0, "sector": 3}],
    "verified_decoded_sectors": [
      {"track": 0, "side": 0, "sector": 0,
       "data_md5": "abc123...",
       "data_size": 256}
    ]
  },
  "provenance": {
    "source": "synthetic" | "real_capture" | "augmented",
    "synthetic_seed": "0x4E554654",
    "real_capture_metadata": null,
    "augmentation_chain": ["jitter:25ns", "dropout:0.5%"]
  }
}
```

The provenance section is **mandatory**. Without it, you can't tell if
your model is trained on real data or its own synthetic biases.

### Step 3: Pick the generation strategy

For each class:

#### Real captures + ground truth

Use `uft_ml_training_gen_extract_from_capture()` (currently a skeleton
function — see `uft-ml-implement-skeleton`). Workflow:

1. Read flux file (.scp / .raw / .hfe)
2. Decode with the **standard** decoder to get sectors
3. Verify sectors against a known-good source (D64/IMG comparison)
4. Emit (flux, verified_sectors) pair only if verification passes
5. Discard ambiguous/unverifiable tracks — bad labels poison training

This requires having a separately-verified source for each capture.
"Decoded by UFT" is not ground truth — that's circular.

#### Synthetic perfect flux

Use the existing flux fixture generators (`uft-flux-fixtures`) plus a
label emitter:

```python
# scripts/gen_synthetic_training.py
from uft_flux_fixtures import gen_scp_fixture
import json, hashlib

def gen_labeled_pair(track_data, seed):
    scp_bytes = gen_scp_fixture.synthesize(track_data, seed=seed)

    label = {
        "format": "scp",
        "ground_truth": {
            "decoded_sectors": [
                {"track": t.num, "data_md5": hashlib.md5(t.bytes).hexdigest(),
                 "data_size": len(t.bytes)}
                for t in track_data
            ]
        },
        "provenance": {"source": "synthetic", "synthetic_seed": hex(seed)}
    }
    return scp_bytes, label
```

Synthetic data is **truly labeled** — the ground truth is the input
spec. Use this for volume; pair with real captures for realism check.

#### Augmented (noisy synthetic)

Augmentation chain: synthetic input → apply jitter/dropouts/weak-bits
→ label is the ORIGINAL pre-augmentation truth.

| Augmentation | Effect | Realism range |
|--------------|--------|---------------|
| Timing jitter | ±N ns randomness on flux deltas | 25-200 ns |
| Dropout | Skip M flux events per N | 0.1%-2% |
| Weak bits | Flip bits with probability p | 0.05%-0.5% |
| Speed variance | Multiply timing by 1±x | ±2% |
| Multi-revolution divergence | Slight differences per rev | gauss(σ=50ns) |

Use seeded random — same training run produces same augmented set.

### Step 4: Validate the training set

Before using for training:

```bash
# 1. All pairs have matching label files
for scp in tests/training/*/*.scp; do
    [[ -f "${scp%.scp}.label.json" ]] || echo "MISSING LABEL: $scp"
done

# 2. Labels are valid JSON
for label in tests/training/*/*.label.json; do
    python3 -m json.tool "$label" > /dev/null || echo "BAD JSON: $label"
done

# 3. Provenance is set
python3 -c "
import json, glob, sys
for f in glob.glob('tests/training/*/*.label.json'):
    label = json.load(open(f))
    if 'provenance' not in label:
        print(f'NO PROVENANCE: {f}')
        sys.exit(1)
"

# 4. Class balance — no single class >70% of samples
ls tests/training/protection_classifier/ | grep -oE '^[^_]+' | sort | uniq -c
```

### Step 5: Split train/val/test

Standard split: 70% train, 15% validation, 15% test. **By disk title,
not by track**. Two tracks from the same disk in train and test = data
leakage.

```python
import random
random.seed(0x4E554654)
disks_by_class = {...}  # {scheme: [disk1, disk2, ...]}
for scheme, disks in disks_by_class.items():
    random.shuffle(disks)
    n = len(disks)
    train = disks[:int(n*0.7)]
    val   = disks[int(n*0.7):int(n*0.85)]
    test  = disks[int(n*0.85):]
```

## Verification

```bash
# 1. Generator runs deterministically
python3 .claude/skills/uft-ml-training-data/scripts/gen_synthetic_training.py
md5sum tests/training/synthetic/*.scp > /tmp/run1
python3 .claude/skills/uft-ml-training-data/scripts/gen_synthetic_training.py
md5sum tests/training/synthetic/*.scp > /tmp/run2
diff /tmp/run1 /tmp/run2 && echo "deterministic"

# 2. Pair count matches
n_inputs=$(find tests/training -name "*.scp" | wc -l)
n_labels=$(find tests/training -name "*.label.json" | wc -l)
[[ $n_inputs -eq $n_labels ]] && echo "paired"

# 3. No leakage between train/val/test
python3 .claude/skills/uft-ml-training-data/scripts/check_split_integrity.py

# 4. Class distribution reasonable
python3 -c "
import json, glob, collections
classes = collections.Counter()
for f in glob.glob('tests/training/protection_classifier/*.label.json'):
    label = json.load(open(f))
    cls = label.get('ground_truth', {}).get('protection_scheme', 'UNKNOWN')
    classes[cls] += 1
total = sum(classes.values())
for cls, n in classes.most_common():
    print(f'{cls}: {n} ({100*n/total:.0f}%)')
"
```

## Common pitfalls

### Training on UFT's own decoder output

If your "ground truth" came from running UFT's decoder on the flux,
training on it teaches the model UFT's existing biases. Use external
verification (D64 sector hashes from a Kryoflux/SCP-Direct dump that
hasn't been touched by UFT).

### Class imbalance

90% clean disks, 5% V-MAX, 5% other → model predicts "clean" always
and gets 90% accuracy without learning anything. Either oversample
minorities or use class weights. Check class distribution before
training.

### Synthetic-only training

A model trained only on synthetic data won't generalize to real
captures. Real disks have artifacts (drift, oxide noise, mechanical
quirks) that synthetic generators don't simulate. Always include
real captures in the training mix.

### Label leak through filenames

If your filename contains the label (`vmax_dungeon_master_1.scp`),
your model can "cheat" by reading the filename. Never include label
in the input path the model sees.

### Re-using validation set for threshold tuning

Validation set tunes hyperparameters (threshold, learning rate). Test
set evaluates final performance. They must be separate. Re-running
threshold tuning against the test set is data leakage.

## Related

- `.claude/skills/uft-flux-fixtures/` — generates the flux side of pairs
- `.claude/skills/uft-ml-protection-classifier/` — consumes training data
  for new reference vectors
- `.claude/skills/uft-ml-implement-skeleton/` — implements the
  uft_ml_training_gen.h functions this skill provides input shape for
- `docs/AI_COLLABORATION.md` §3 (Konfidenz-Regel) — applies to label
  confidence claims
- `include/uft/ml/uft_ml_training_gen.h` — skeleton header listing
  training-data functions
