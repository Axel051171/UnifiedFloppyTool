---
name: uft-ml-protection-classifier
description: |
  Use when modifying or extending uft_ml_protection.c — the cosine-
  similarity copy-protection classifier. Trigger phrases: "neue
  protection im ML classifier", "V-MAX/RapidLok/CopyLock reference
  vector", "ml_protection feature engineering", "protection classifier
  threshold", "classifier features hinzufügen". This is the ONE complete
  ML module in UFT — pure C, no external libs. DO NOT use for: heuristic
  protection detection (→ uft-protection-scheme — different layer),
  CNN/neural decoders (→ uft-ml-implement-skeleton), training data
  generation (→ uft-ml-training-data), uft_ml_decoder.h (skeleton, not
  classifier).
---

# UFT ML Protection Classifier

Use this skill when extending `uft_ml_protection.c` — feature-based
copy-protection classifier using cosine similarity against hardcoded
reference vectors. Pure C, no external ML libraries, runs on STM32H723.

## When to use this skill

- Adding a new protection scheme to the classifier reference table
- Adding a new feature dimension (extending `UFT_ML_PROT_FEATURES`)
- Tuning the decision threshold for false-positive control
- Adding the per-track aggregation logic
- Profiling and optimizing the classifier

**Do NOT use this skill for:**
- Heuristic protection detection (specific bit patterns, structural
  tests) — that's a **different layer** in `src/protection/`, use
  `uft-protection-scheme`. The classifier and the heuristic detectors
  COEXIST and complement each other.
- CNN-based decoders for damaged flux — use `uft-ml-implement-skeleton`
  (`uft_ml_decoder.h`)
- Generating labeled training data for the classifier — use
  `uft-ml-training-data`
- Anything in `uft_ml_decoder.h` — those are skeleton functions

## Architecture in one paragraph

For each track, extract a fixed-size feature vector (entropy, sync
density, sector-length variance, etc.) of dimension
`UFT_ML_PROT_FEATURES`. Compare via cosine similarity against a
hardcoded table of reference vectors, one per known protection scheme.
Highest similarity above threshold wins. Below threshold = "unknown but
suspicious" or "clean".

Why this design:
- **Deterministic.** Same disk → same classification.
- **No training at inference time.** All weights are constants.
- **Forensically auditable.** You can print "this disk matched V-MAX
  with 0.87 cosine similarity to reference" — explicit and explainable.
- **Firmware-portable.** Pure `math.h`, no malloc in hot path, fits
  on STM32H723.

## Workflow

### Step 1: Identify what you're adding

| Change type | What to edit |
|-------------|--------------|
| New protection scheme | Add reference vector to `KNOWN_SCHEMES[]` table |
| New feature dimension | Increment `UFT_ML_PROT_FEATURES`, regenerate ALL reference vectors |
| Tune threshold | Adjust `UFT_ML_PROT_THRESHOLD` constant + verify against test set |
| New per-track aggregation | Modify `uft_ml_detect_protection()` aggregation logic |

### Step 2: Add a reference vector for a new scheme

Adding a new protection requires:

1. **Capture training samples.** At least 5 disks with the protection,
   ideally from different software titles (avoid overfitting to one
   game's specific quirks).

2. **Extract features from each sample.** Run
   `uft_ml_extract_features()` on every track of every sample. Average
   across tracks within one disk, then across disks.

3. **Add to reference table:**

```c
/* In uft_ml_protection.c */
static const uft_ml_prot_reference_t KNOWN_SCHEMES[] = {
    /* Existing entries */
    { "V-MAX!",     { 0.82f, 0.41f, ... }, UFT_PROT_VMAX },
    { "RapidLok",   { 0.71f, 0.83f, ... }, UFT_PROT_RAPIDLOK },
    /* NEW: */
    { "MyScheme",   { 0.65f, 0.52f, ... }, UFT_PROT_MYSCHEME },
    /* sentinel */
    { NULL,         { 0 },                  UFT_PROT_NONE },
};
```

4. **Validate cross-confusion.** New reference must NOT have >0.85
   cosine similarity to any existing reference. If it does, your
   feature space can't distinguish them — go to step 4.

### Step 3: Regenerate threshold if features changed

If you add a feature dimension:
- All existing reference vectors become invalid (different
  dimensionality)
- Re-extract from training samples
- Re-tune `UFT_ML_PROT_THRESHOLD` against held-out test set

The default threshold target: false-positive rate <2% on clean disks,
true-positive rate >85% on protected disks.

### Step 4: Verify cross-confusion matrix

```bash
# After change, run on test set:
./uft ml-classify --debug \
    tests/vectors/protected/vmax_*.scp \
    tests/vectors/protected/rapidlok_*.scp \
    tests/vectors/protected/myscheme_*.scp \
    tests/vectors/clean/*.scp \
    > /tmp/confusion.txt

# Check matrix
python3 scripts/build_confusion_matrix.py /tmp/confusion.txt
# Goal: diagonal dominates, off-diagonals < 0.3
```

### Step 5: Document the addition

In the `.c` file, ABOVE the new reference entry, add a comment block:

```c
/* MyScheme — first seen in <game>, <year>, <publisher>.
 * Reference vector trained from N=<count> disks.
 * Distinguishing features: <which dimensions matter most>.
 * Known false-positives: <list> (cosine sim < 0.65 these).
 * Known false-negatives: <list>.
 * Reference: <URL or paper> */
```

This is forensic evidence. Without provenance, the classifier is a
black box and the output isn't forensically defensible.

## Verification

```bash
# 1. Compile
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/analysis/uft_ml_protection.c

# 2. Existing tests still pass (regression)
cd tests && make test_ml_protection && ./test_ml_protection

# 3. New scheme is detected on positive samples
./uft ml-classify tests/vectors/protected/myscheme_*.scp 2>&1 \
    | grep -c "MyScheme"
# expect: equal to number of input files

# 4. New scheme NOT detected on clean samples
./uft ml-classify tests/vectors/clean/*.scp 2>&1 | grep "MyScheme"
# expect: empty

# 5. Existing schemes still detected (no regression)
for scheme in vmax rapidlok copylock speedlock; do
    found=$(./uft ml-classify tests/vectors/protected/${scheme}_*.scp 2>&1 \
            | grep -c -i "$scheme")
    echo "  $scheme: $found detections"
done

# 6. Firmware portability — protection classifier is dual-target
bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh \
    src/analysis/uft_ml_protection.c
```

## Common pitfalls

### Reference vector overfit to one disk

If you train on samples of ONE game with V-MAX, the reference vector
encodes that game's quirks (specific sector sizes, specific load
patterns) rather than V-MAX itself. Always train on samples from
multiple titles.

### Adding a feature without regenerating references

`UFT_ML_PROT_FEATURES = 8` → `9`: every existing reference vector now
has wrong dimensions. Cosine similarity will silently use only 8 of 9
dimensions or read garbage. Always regenerate.

### Cosine sim of zero vector

```c
/* WRONG — undefined for zero magnitude */
float sim = cosine_similarity(track_features, ref_vector, n);

/* RIGHT */
if (vector_magnitude(track_features, n) < 1e-6f) {
    return UFT_PROT_INSUFFICIENT_DATA;  /* not classifiable */
}
```

A track with no features (silent track, all zero) has zero magnitude
and undefined cosine. Reject before classification.

### Threshold tuning on training set

If you tune `UFT_ML_PROT_THRESHOLD` to maximize accuracy on the same
samples you trained on, you've leaked test data into training.
Threshold tuning needs a separate held-out validation set.

### Float32 vs Float64 for cosine

`uft_ml_protection.c` uses `float` (single-precision) for STM32H723
compatibility. Don't switch to `double` even on desktop — that breaks
firmware portability (see `uft-stm32-portability`). If precision is
truly insufficient, use scaled `int32_t`.

## Cross-skill boundary

The protection layer in UFT has TWO complementary detectors:

```
                    ┌─────────────────────────┐
disk in →           │ Heuristic detectors     │  ← uft-protection-scheme skill
                    │ (specific bit patterns) │     (src/protection/uft_*.c)
                    └─────────────────────────┘
                              │
                              ▼ (if no match or low confidence)
                    ┌─────────────────────────┐
                    │ ML classifier           │  ← THIS skill
                    │ (cosine similarity)     │     (src/analysis/uft_ml_protection.c)
                    └─────────────────────────┘
                              │
                              ▼
              { protection_id, confidence, evidence }
```

The classifier is a **fallback** for protections too varied for
heuristic patterns. Adding a heuristic detector for a known specific
pattern → use `uft-protection-scheme`. Adding ML coverage for a
hard-to-pattern protection → use this skill.

## Related

- `.claude/skills/uft-protection-scheme/` — heuristic detectors (different layer)
- `.claude/skills/uft-ml-training-data/` — generates labeled samples
- `.claude/skills/uft-ml-implement-skeleton/` — different ML module (decoder, not classifier)
- `.claude/skills/uft-stm32-portability/` — required check (this is dual-target)
- `src/analysis/uft_ml_protection.c` — the implementation (~400 LOC)
- `include/uft/analysis/uft_ml_protection.h` — public API
- `docs/AI_COLLABORATION.md` §3 (Konfidenz-Regel) — applies to classifier output
