---
name: uft-ml-implement-skeleton
description: |
  Use when implementing functions from a UFT_SKELETON_PLANNED header
  (uft_ml_decoder.h, uft_ml_training_gen.h, or any future skeleton).
  Trigger phrases: "implementiere uft_ml_decoder", "ML decoder fertig
  bauen", "training_gen Funktion implementieren", "skeleton ausfüllen",
  "promote skeleton to real implementation", "M1 MF-011 implement".
  Enforces the "no phantom features" rule: implement the function or
  remove the prototype, never both. DO NOT use for: implementing
  uft_ml_protection.c (already complete → uft-ml-protection-classifier),
  designing new ML APIs from scratch (write design doc first), porting
  external ML libs (separate concern), generating training data only
  (→ uft-ml-training-data).
---

# UFT ML Skeleton Implementation

Use this skill when promoting a function from a `UFT_SKELETON_PLANNED`
header to a real implementation. UFT has 41 skeleton functions across
two headers — every callsite either link-fails or silently no-ops until
implemented.

## When to use this skill

- Implementing one or more functions from `include/uft/ml/uft_ml_decoder.h`
  (20 functions, all unimplemented as of v3.7.0+)
- Implementing functions from `include/uft/ml/uft_ml_training_gen.h`
  (21 functions, same status)
- Removing a skeleton function that's not going to be implemented
- Reviewing a PR that touches skeleton headers
- Promoting any future `UFT_SKELETON_PLANNED` API to real status

**Do NOT use this skill for:**
- Modifying `uft_ml_protection.c` — that's complete C code, use the
  `uft-ml-protection-classifier` skill
- Designing brand-new ML APIs from scratch — write a design doc in
  `docs/` first, then this skill
- Porting external ML libraries (TensorFlow, PyTorch, ONNX) — separate
  concern, not skeleton work
- Generating training data only — use `uft-ml-training-data`

## The skeleton convention

Every skeleton header has this banner:

```c
/* * * UFT_SKELETON_PLANNED * * *
 * PLANNED FEATURE — Machine-learning decode
 * This header declares N public functions, of which M have no
 * implementation in the source tree. Callers exist but will link-fail
 * or silently no-op until the feature is implemented.
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 DOCUMENT-Welle).
 * Do NOT add new call sites to functions from this header without first
 * implementing them or removing the prototype.
 * * * * * * * * * * */
```

Two rules from this banner:

1. **No new call sites for unimplemented functions.** Adding `uft_ml_decoder_init()` somewhere increases the scope of the broken promise.
2. **Implement OR remove. Never both.** Half-implemented prototypes are worse than no prototypes — they advertise a feature that doesn't work.

## Workflow

### Step 1: Pick the function

Read the header. Pick ONE function. Do not bulk-implement — each function
needs a working test. Bulk-implementations skip tests and produce
phantom features by another name.

```bash
# List all skeleton functions in a header
grep -E "^[a-zA-Z_].*\(.*\);" include/uft/ml/uft_ml_decoder.h | head -20
```

Choose the function with smallest dependency footprint first. For
`uft_ml_decoder.h`:

| Order | Function class | Why first |
|-------|----------------|-----------|
| 1 | `uft_ml_*_init` / `_destroy` | No deps — just allocation |
| 2 | `uft_ml_*_load_model` / `_save_model` | Just I/O |
| 3 | `uft_ml_*_predict` | Needs model loaded |
| 4 | `uft_ml_*_train` | Needs everything else |

### Step 2: Decide on the ML runtime

`uft_ml_decoder.h` mentions "ONNX/TFLite runtime support". Before writing
code, decide which:

| Runtime | Pro | Con |
|---------|-----|-----|
| **No ML lib** (cosine sim, like `uft_ml_protection.c`) | Pure C, firmware-portable, zero deps | Limited model expressivity |
| **TFLite Micro** | Runs on STM32H723 (UFI-compatible) | Build complexity, ~100KB flash |
| **ONNX Runtime** | Wide model support | Desktop only, ~10MB |
| **GGML (llama.cpp style)** | Single .h+.c file, portable | CNN support is weaker |

Default recommendation: start with **no ML lib** (statistical features +
similarity), like `uft_ml_protection.c` does. Promote to TFLite/ONNX
only if the simple approach measurably underperforms.

### Step 3: Implement with the matching .c file

Create `src/ml/uft_ml_decoder.c` (or `src/analysis/uft_ml_decoder.c`
following `uft_ml_protection.c`'s example). Use
`templates/skeleton_impl.c.tmpl` as starting point.

**Required at the top:**

```c
/* * * UFT_SKELETON_PROMOTED * * *
 * Implementation status (this file):
 *   IMPLEMENTED: list functions implemented here
 *   NOT_YET:     list functions still NOT implemented (=> link error if called)
 *   REMOVED:     list functions whose prototypes should be deleted from .h
 * When this list is complete (no NOT_YET, no REMOVED) replace the
 * UFT_SKELETON_PLANNED banner in the .h with: "STATUS: Implemented".
 * * * * * * * * * * */
```

This banner tracks state. CI can grep for `UFT_SKELETON_PROMOTED` to
verify the list is consistent.

### Step 4: Write the test alongside

Test goes in `tests/test_ml_decoder.c` (one file per skeleton header).
Required test cases per implemented function:

- **Happy path:** typical input → expected output
- **Boundary:** empty / single-element / max-size input
- **Invalid:** NULL pointer, negative size, malformed model
- **Determinism:** same input → same output across runs (seed=0x4E554654)

```c
TEST(uft_ml_decoder_init_returns_valid_handle) {
    uft_ml_decoder_t *d = uft_ml_decoder_init();
    ASSERT(d != NULL);
    uft_ml_decoder_destroy(d);
}
```

### Step 5: Update the banner

When a function moves from skeleton to implemented:

1. Remove the function from "this header declares N public functions, of
   which M have no implementation" — decrement M.
2. If M reaches 0: replace the whole banner with `STATUS: Implemented`.
3. Add the function to `docs/KNOWN_ISSUES.md` resolved list.

If you decide to REMOVE rather than implement:

1. Delete the prototype from the .h
2. Find all callers: `grep -rn "uft_ml_decoder_xyz" src/ include/`
3. Decide each callsite: replace with alternative, or remove the calling code
4. Update docs/MASTER_PLAN.md to mark MF-011 scope reduction

## Verification

```bash
# 1. Header still declares only the count it claims
declared=$(grep -cE "^[a-zA-Z_].*\(.*\);" include/uft/ml/uft_ml_decoder.h)
echo "Declared: $declared"

# 2. Count matches the banner ("declares N public functions")
banner_count=$(grep -oE "declares [0-9]+ public functions" \
    include/uft/ml/uft_ml_decoder.h | grep -oE "[0-9]+")
echo "Banner says: $banner_count"
[[ "$declared" == "$banner_count" ]] || echo "MISMATCH"

# 3. Implementation compiles
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/ml/uft_ml_decoder.c

# 4. Linked binary has no undefined references for implemented functions
qmake && make -j$(nproc) 2>&1 | grep -E "undefined reference.*uft_ml_decoder"
# expect: empty

# 5. Test passes
cd tests && make test_ml_decoder && ./test_ml_decoder
# expect: passes including determinism check

# 6. No new call sites to STILL-skeleton functions
git diff main -- src/ include/ | \
    grep -E "^\+.*uft_ml_(decoder|training_gen)_[a-z_]+\(" | \
    grep -v src/ml/uft_ml_decoder.c
# expect: empty (only the implementation file should add calls)
```

## Common pitfalls

### Implementing 5 functions, testing 1

Each function needs its own test. Bulk-implementation produces "we built
the framework, tests later" — and tests later means tests never. Stop
after one function, write its test, run it, then continue.

### Returning UFT_OK from empty body

```c
/* WRONG — silent no-op disguised as success */
uft_error_t uft_ml_decoder_train(uft_ml_decoder_t *d, ...) {
    (void)d;
    return UFT_OK;
}

/* RIGHT — explicit not-implemented */
uft_error_t uft_ml_decoder_train(uft_ml_decoder_t *d, ...) {
    (void)d;
    return UFT_ERROR_NOT_IMPLEMENTED;
}
```

If callers see `UFT_OK` they assume the model trained. They will then
call `predict()` on a not-trained model and get garbage. Better to fail
loud.

### Adding ONNX dependency without firmware fallback

If you import ONNX Runtime, the function MUST also have a path that
works without it (for UFI firmware target). Either:

- Two implementations: `_onnx.c` and `_pure_c.c`, dispatcher chooses
- One implementation that gracefully degrades

`#ifdef UFT_HAS_ONNX` everywhere is fine, but the no-ONNX build must
still link and the function must do something meaningful (e.g. fall back
to cosine similarity like `uft_ml_protection.c`).

### Forgetting the determinism rule

ML inference must be deterministic for forensic reproducibility. Same
input + same model = same output, every time. Random init in training
must seed from `0x4E554654` (the UFT seed convention). If you use
threading or GPU, expose a `--single-threaded-deterministic` mode.

## Related

- `.claude/skills/uft-ml-protection-classifier/` — the one ML module
  that's already implemented; reference for style
- `.claude/skills/uft-ml-training-data/` — generates inputs for testing
  newly-implemented functions
- `.claude/skills/uft-stm32-portability/` — required check before merge
- `docs/AI_COLLABORATION.md` §1 (Arbeits-Reihenfolge) — applies fully
- `docs/MASTER_PLAN.md` M1/MF-011 — DOCUMENT-Welle scope
- `docs/KNOWN_ISSUES.md` "Planned APIs" — tracking list to update
- `src/analysis/uft_ml_protection.c` — existing implementation reference
