---
name: uft-deepread-module
description: |
  Use when adding a new module to UFT's DeepRead pipeline or OTDR-booster
  (8+12 modules exist). Trigger phrases: "neues DeepRead modul", "OTDR
  modul für X", "adaptive decode pass für Y", "recovery module", "weighted
  voting pass". Pipeline pattern: each module takes flux+annotations,
  produces refined flux+annotations. DO NOT use for: flat decoder changes
  (→ src/flux/), protection detection (→ uft-protection-scheme), format
  plugins (→ uft-format-plugin), top-level PLL (→ algorithms/).
---

# UFT DeepRead Module

Use this skill when extending UFT's DeepRead pipeline (adaptive recovery)
or the OTDR booster chain (signal-quality improvement). Each module is
an independent pass that refines the input before the next module.

## When to use this skill

- Adding a new DeepRead recovery pass (re-decode, weighted voting,
  multi-rev fusion)
- Adding a new OTDR analysis module (noise floor, peak detection,
  splice detection)
- Porting an idea from a8rawconv or X-Copy into the pipeline
- Replacing an existing module with a better algorithm

**Do NOT use this skill for:**
- Non-pipeline decoder work — that's `src/flux/`
- Protection-scheme detectors — use `uft-protection-scheme`
- Format-level plugins — use `uft-format-plugin`
- Top-level PLL tuning — that's `src/algorithms/uft_kalman_pll.c`

## Pipeline architecture

```
   input (flux)
       │
       ▼
   ┌─────────────┐
   │ OTDR stage1 │ ← modules: dc_blocker, noise_gate, peak_align, ...
   │ OTDR stage2 │
   │     ...     │
   │ OTDR stageN │
   └─────────────┘
       │  (refined flux + annotations)
       ▼
   ┌─────────────┐
   │ DeepRead 1  │ ← modules: adaptive_pll, weak_bit_detect, ...
   │ DeepRead 2  │
   │     ...     │
   │ DeepRead M  │
   └─────────────┘
       │  (decoded bitstream + confidence)
       ▼
   decoder → format plugin
```

Every module has the signature:

```c
uft_error_t module_process(uft_flux_t *in_out,
                             uft_annotations_t *annotations,
                             const uft_module_params_t *params);
```

Modules **mutate in place** via `in_out`. Annotations accumulate
(never discard prior notes).

## The 4 touchpoints

| # | File | Purpose |
|---|------|---------|
| 1 | `src/deepread/uft_dr_<module>.c` or `src/analysis/otdr/uft_otdr_<module>.c` | Implementation |
| 2 | `include/uft/deepread/uft_deepread.h` or `uft_otdr.h` | Public header |
| 3 | Respective pipeline registry | Module order enforcement |
| 4 | `tests/test_<module>.c` | Synthetic flux in, verify output |

## Workflow

### Step 1: Decide which pipeline

OTDR = signal quality (early, before decode). Examples: noise reduction,
peak detection, splice marking.

DeepRead = recovery (late, after initial decode failure). Examples:
adaptive re-decode, weighted voting across revolutions, weak-bit
classification.

If your module tightens signal → OTDR. If it makes a better decision
from existing data → DeepRead.

### Step 2: Implement the module

Use `templates/module.c.tmpl`. Rules:

- **Pure function** in the math sense: same input → same output.
  No globals, no thread-local state, no time-dependence.
- **Mutations in place** on the `uft_flux_t` passed in.
- **Annotations additive** — append to `annotations`, never overwrite.
- **Fast-skip** when the module can't help: if input already meets
  criterion, return early without modification.
- **Confidence update** — if the module refines decode confidence, report
  delta in annotations.

### Step 3: Pipeline position matters

Order of modules in the registry determines processing order. Wrong
order = wrong results:

- OTDR DC-blocker BEFORE peak-alignment (else misaligned peaks)
- DeepRead weak-bit-detect BEFORE voting (else vote without weights)
- DeepRead PLL-retune BEFORE final decode (else decode with stale PLL)

Document your position requirement in the module's header comment.

### Step 4: Confidence range discipline

Modules emit confidence in `[0.0, 1.0]`. Never:

- Output > 1.0 (what would that mean?)
- Hard-code confidence 1.0 without proof
- Silently drop confidence when failing — return 0.0 explicitly

## Verification

```bash
# 1. Compile
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/deepread/uft_dr_<module>.c

# 2. Full build
qmake && make -j$(nproc)

# 3. Synthetic input → verify output
./tests/test_<module>
# expect: passes all vectors

# 4. End-to-end: use synthetic SCP fixture, run full pipeline
./uft analyze tests/vectors/scp/weak_sector_at_track_18.scp --deepread
# expect: recovered sector count increases vs --no-deepread

# 5. Regression check
ctest --output-on-failure -R "deepread|otdr"
```

## Common pitfalls

### Module mutates annotations it doesn't own

Annotations are shared. Don't overwrite entries from earlier modules —
append your own. Earlier findings are part of the audit trail.

### Time-dependent behavior

If the module's output depends on `time()` or random state, it's not
reproducible. Always seed if random is needed; prefer deterministic
algorithms.

### Firmware portability

DeepRead runs on desktop AND potentially on UFI (for live preview). Run
`check_firmware_portability.sh` on new module before merge. See
`uft-stm32-portability` skill.

### Pipeline order dependency

If your module requires another module to run first, document it AND
enforce it in the registry. Don't rely on implicit ordering.

## Related

- `.claude/skills/uft-stm32-portability/` — DeepRead is dual-target
- `.claude/skills/uft-benchmark/` — measure pipeline impact
- `.claude/skills/uft-flux-fixtures/` — test inputs
- `docs/DEEPREAD_ARCHITECTURE.md` — pipeline design
- `docs/OTDR_PIPELINE.md` — OTDR module list
- `src/deepread/uft_dr_weighted_voting.c` — reference DeepRead module
- `src/analysis/otdr/uft_otdr_peak_align.c` — reference OTDR module
