---
name: uft-protection-scheme
description: |
  Use when adding a copy-protection detector (or completing one that
  exists as stub). Trigger phrases: "neuer protection detector",
  "X copy protection", "erkenne Y protection", "V-MAX detector",
  "Copylock detection", "Rob Northen erkennen", "fuzzy bits", "long
  track detection". 35+ schemes already registered — mirror the
  existing pattern in src/protection/. Related to XCOPY_INTEGRATION_TODO
  T6/T7 (virus + bootblock scan).
---

# UFT Protection-Scheme Detector Template

Use this skill when adding a new copy-protection detector. 35+ schemes
exist, the pattern is stable. Never auto-kill or "fix" protections —
UFT is read-only forensic (see XCOPY_INTEGRATION_TODO anti-features).

## Step 1: Pick a template

| If the protection is… | Use as template |
|---|---|
| Track-structure based (long tracks, half tracks) | `src/protection/uft_longtrack.c` |
| Sector-level (weak bits, duplicate sectors) | `src/protection/uft_fuzzy_bits.c` |
| Sync-pattern based (custom 0x448n markers) | `src/protection/uft_copylock.c` |
| Density mismatch (MFM ↔ FM transitions) | `src/protection/uft_speedlock.c` |
| Amiga-specific (RNC, Copylock Amiga) | `src/protection/uft_amiga_caps.c` |
| C64-specific (V-MAX, RapidLok) | `src/protection/uft_rapidlok.c` |
| Atari ST (Macrodos, dec0de) | `src/protection/uft_atarist_copylock.c` |

## Step 2: Detector file structure

Location: `src/protection/uft_<scheme>.c` (+ `.h` in
`include/uft/protection/`). Required shape:

```c
#include "uft/protection/uft_protection.h"

/* Scheme-specific detection state / signatures */
static const uint8_t FOO_SIGNATURE[] = { /* bytes, masked patterns */ };

/*
 * Detector signature. Called once per track. Returns:
 *   - UFT_PROT_NOT_DETECTED  — this scheme is not present
 *   - UFT_PROT_DETECTED      — strong match, populate result fields
 *   - UFT_PROT_POSSIBLE      — weak match, set confidence < 80
 */
static uft_prot_result_t foo_detect(const uft_track_t *track,
                                      const uft_prot_ctx_t *ctx,
                                      uft_prot_detail_t *out_detail)
{
    if (!track || !track->bits) return UFT_PROT_NOT_DETECTED;

    /* Detection logic: scan bits/sectors for the signature pattern. */
    ...

    if (match_score >= 95) {
        out_detail->scheme = UFT_PROT_FOO;
        out_detail->confidence = (uint8_t)match_score;
        out_detail->track_first_seen = track->track_num;
        out_detail->description = "FOO copy protection (trait X)";
        return UFT_PROT_DETECTED;
    }
    return UFT_PROT_NOT_DETECTED;
}

/* Registry entry */
static const uft_prot_detector_t foo_detector = {
    .scheme_id = UFT_PROT_FOO,
    .scheme_name = "FOO",
    .platforms = UFT_PLATFORM_C64 | UFT_PLATFORM_AMIGA,
    .detect = foo_detect,
};
UFT_REGISTER_PROT_DETECTOR(foo)
```

## Step 3: Add scheme ID

`include/uft/protection/uft_protection_types.h`:
```c
UFT_PROT_FOO,   /* Description: first seen in <game>, <year> */
```

## Step 4: Anti-features (never do these)

From `XCOPY_INTEGRATION_TODO.md` §Anti-Features:
- **No auto-kill**: never "clean" a protection. Detection is report-only.
- **No in-place repair**: never modify the loaded image. Copy-on-write
  only, new file on disk.
- **No synthesis**: if a protection uses randomised weak bits, preserve
  the actual jitter — don't substitute a "cleaner" pattern.

## Step 5: False-positive discipline

Copy-protection detectors are famously fragile. Required:

- Detection threshold ≥ 80 for "DETECTED" status. Below → POSSIBLE.
- Minimum 2 corroborating signals (e.g. sync pattern + track length).
- Test against **clean disks** (standard AmigaDOS, DOS 3.3, CP/M) —
  detector must NOT trigger on unprotected images.
- Test against **at least one real sample** of the protected format
  (public-domain cracks, preservation archives).

## Step 6: Test file

Location: `tests/test_<scheme>_protection.c`. Template:
`tests/test_c64_protection.c`. Structure:

```c
TEST(detects_foo_on_known_sample) {
    uft_track_t track = { /* loaded from tests/vectors/ fixture */ };
    uft_prot_detail_t detail;
    ASSERT(foo_detect(&track, &ctx, &detail) == UFT_PROT_DETECTED);
    ASSERT(detail.confidence >= 80);
}

TEST(no_false_positive_on_clean_adf) {
    uft_track_t clean = load_fixture("clean_amigados.adf", 0);
    uft_prot_detail_t detail;
    ASSERT(foo_detect(&clean, &ctx, &detail) == UFT_PROT_NOT_DETECTED);
}
```

## Step 7: Update the protection matrix

`src/protection/uft_protection_detect.c` — the master dispatch:
```c
/* Order matters: more specific schemes before generic catch-alls */
const uft_prot_detector_t *DETECTORS[] = {
    &foo_detector,  /* NEW — specific pattern */
    /* existing detectors... */
    &generic_longtrack_detector,  /* catch-all last */
    NULL
};
```

## Step 8: Documentation requirements (Principle 7)

Each new detector MUST have:
- First-seen commit message entry in CHANGELOG.md
- Entry in `docs/protection_schemes.md` (create if missing): scheme
  name, year introduced, platforms, games known to use it, detection
  rate %
- Known-false-negatives disclosed (e.g. "does not detect the rare
  v2 variant used in 3 Ocean games")

## Anti-patterns

- **Don't combine multiple schemes in one file** — `uft_combined.c`
  becomes unmaintainable. One scheme, one file.
- **Don't detect by filename/metadata** — it's the disk bits that
  count, not the label.
- **Don't hardcode game titles** — a detector for "Dungeon Master's
  Fuzzy Bits" scheme, not for "Dungeon Master" itself.
- **Don't let detectors mutate `uft_track_t`** — detection is
  read-only (`const`).

## Related

- `src/protection/uft_protection_unified.c` — registry
- `docs/XCOPY_INTEGRATION_TODO.md` (T1, T2, T6, T7)
- `CLAUDE.md` — Protection schemes section (35+ listed)
- `.claude/agents/forensic-integrity.md` — review new detectors for
  silent data loss
