---
name: uft-protection-scheme
description: |
  Use when adding a copy-protection DETECTOR to UFT's read-only forensic
  pipeline. Trigger phrases: "neuer protection detector", "X copy protection
  erkennen", "V-MAX detector", "Copylock detection", "Rob Northen erkennen",
  "fuzzy bits detector", "long track detection". 35+ schemes registered;
  pattern is stable. DO NOT use for: removing/cleaning protections (UFT is
  read-only by design), format plugins (→ uft-format-plugin), filesystem
  semantics (→ uft-filesystem), protection-scheme reverse-engineering (write
  a spec doc first, then this skill).
---

# UFT Protection-Scheme Detector

Use this skill when adding a new copy-protection **detector**. UFT is
read-only forensic software — detectors observe and report, never modify.

## When to use this skill

- Adding a detector for a protection scheme UFT doesn't recognize yet
- Completing a stubbed detector file
- Adding platform variants (e.g. CopyLock Amiga vs. CopyLock Atari ST)

**Do NOT use this skill for:**
- "Cleaning" or "killing" a protection — UFT never modifies images
- Repairing a protection that didn't match — detection is observation-only
- Reverse-engineering a scheme from scratch — write a spec doc in
  `docs/protection_schemes/` first, then use this skill to wire it in
- Format-level work (SCP/HFE decoding) — that's `src/formats/` and `src/flux/`

## The 4 touchpoints

| # | File | Purpose |
|---|------|---------|
| 1 | `src/protection/uft_<scheme>.c` | Detector implementation |
| 2 | `include/uft/protection/uft_protection_types.h` | Scheme ID enum |
| 3 | `src/protection/uft_protection_detect.c` | Dispatch table entry |
| 4 | `docs/protection_schemes.md` | Documentation (first-seen game, year) |

## Workflow

### Step 1: Pick a template by scheme class

| Your protection is… | Reference detector |
|---------------------|--------------------|
| Long tracks, half tracks | `src/protection/uft_longtrack.c` |
| Weak bits, duplicate sectors | `src/protection/uft_fuzzy_bits.c` |
| Custom sync markers (non-4489) | `src/protection/uft_copylock.c` |
| Density mismatch (MFM↔FM mix) | `src/protection/uft_speedlock.c` |
| Amiga-specific (RNC, CopyLock) | `src/protection/uft_amiga_caps.c` |
| C64-specific (V-MAX, RapidLok) | `src/protection/uft_rapidlok.c` |

Read the closest existing detector end-to-end before writing.

### Step 2: Write the detector

Location: `src/protection/uft_<scheme>.c` using
`templates/detector.c.tmpl`. Every detector is a single C file with:

- A signature pattern (hardcoded bytes or structural test)
- A `detect()` function returning `UFT_PROT_NOT_DETECTED /
  UFT_PROT_POSSIBLE / UFT_PROT_DETECTED`
- Platform flags (`UFT_PLATFORM_C64 | UFT_PLATFORM_AMIGA | ...`)
- A registration block at bottom

### Step 3: Register the scheme ID

`include/uft/protection/uft_protection_types.h`:

```c
UFT_PROT_<SCHEME>,   /* Description: first seen in <game>, <year> */
```

The comment is not optional — it's documentation that prevents future
confusion about what each enum value means.

### Step 4: Dispatch order matters

`src/protection/uft_protection_detect.c`:

```c
const uft_prot_detector_t *DETECTORS[] = {
    &specific_pattern_detector,        /* specific first */
    /* ... existing ... */
    &generic_longtrack_detector,       /* catch-alls last */
    NULL
};
```

Specific-pattern detectors go BEFORE catch-alls. Otherwise the catch-all
"claims" protections that a specific detector would have identified better.

### Step 5: Document in `docs/protection_schemes.md`

```markdown
## SCHEME_NAME

- **First seen:** <game>, <year>
- **Platforms:** C64, Amiga
- **Variant:** v1 / v2 (if applicable)
- **Detection confidence:** 85% against known sample set
- **Known false negatives:** <list>
- **Reference:** <url or book citation>
```

## Verification

```bash
# 1. Compile the detector
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/protection/uft_<scheme>.c

# 2. Full build
qmake && make -j$(nproc)

# 3. Run the detector's own test
cd tests && make test_<scheme>_protection && ./test_<scheme>_protection

# 4. Confirm no false positive on clean disks
for clean in tests/vectors/*/minimal_*; do
    ./uft detect-protection "$clean" | grep -q "<SCHEME>" && \
        echo "FALSE POSITIVE: $clean"
done

# 5. Confirm detection on known protected sample (if you have one)
./uft detect-protection tests/vectors/protected/<sample> | grep "<SCHEME>"

# 6. Regression check
ctest --output-on-failure -R protection
```

## Anti-features (enforced by forensic-integrity agent)

These are hard rules — violations block merge:

### Never modify the image

```c
/* WRONG — mutating the track destroys forensic integrity */
void fix_protection(uft_track_t *track) { ... }

/* RIGHT — observation only, track is const */
uft_prot_result_t detect(const uft_track_t *track, ...) { ... }
```

### Never synthesize "clean" versions

If a protection uses genuinely random weak bits (like fuzzy bits), preserve
the actual jitter. Don't substitute a "cleaner" pattern because it decodes
more reliably. The jitter IS the data.

### Never auto-unlock

UFT doesn't crack. A detector that identifies Rob Northen CopyLock **does
not** generate a key patch. That's a separate tool in a separate repo
with different legal context.

## False-positive discipline

Copy-protection detectors are notoriously fragile. Required for merge:

- Detection threshold ≥ 80 for `DETECTED` status; below → `POSSIBLE`
- Minimum 2 corroborating signals (e.g. sync pattern + track length)
- Tested against clean disks (standard AmigaDOS, DOS 3.3, CP/M) — must NOT trigger
- Tested against at least one real protected sample

## Common pitfalls

### Detecting by filename or metadata

Detection MUST come from disk bits. A file named `dungeon_master.adf`
can still be an unprotected copy.

### Hardcoding game titles in the detector

The detector identifies the **scheme** (fuzzy bits pattern), not the game.
"Dungeon Master" is an example game that used the scheme, not the target
of detection.

### Confusing scheme with vendor

"Rainbow Arts protection" is not a scheme — it's a publisher. Schemes are
specific bit patterns. A publisher may have used multiple schemes.

### Mutating `uft_track_t` inside detect

Detectors take `const uft_track_t *`. If you find yourself needing to
modify it, you're doing something wrong — probably mixing detection with
"repair" logic. Split them into two phases.

## Related

- `.claude/skills/uft-format-plugin/` — plugins provide the tracks detectors analyze
- `.claude/skills/uft-flux-fixtures/` — generate test samples with/without protection
- `.claude/agents/forensic-integrity.md` — reviews detectors for hard-rule violations
- `docs/protection_schemes.md` — scheme catalog
- `docs/XCOPY_INTEGRATION_TODO.md` §Anti-Features — the "never modify" rule
- `src/protection/uft_fuzzy_bits.c` — canonical reference (weak bits)
- `src/protection/uft_copylock.c` — canonical reference (sync patterns)
