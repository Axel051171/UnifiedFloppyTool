---
name: uft-recovery-integrity
description: |
  Use when modifying or reviewing code under src/recovery/ or any function
  that fabricates, repairs, or reconstructs sector/bitstream/flux data.
  Trigger phrases: "recovery code", "sector recovery", "bitstream
  recovery", "multi-read voting", "CRC correction", "fehlende bytes
  rekonstruieren", "recovery pass", "confidence_map", "weak bits handhaben",
  "majority voting für sektoren". Enforces the data-integrity invariants
  that distinguish a forensic preservation tool from a "best-effort"
  recovery tool. DO NOT use for: DeepRead pipeline modules
  (→ uft-deepread-module), top-level format plugins (→ uft-format-plugin),
  CRC polynomial implementation (→ uft-crc-engine), filesystem-level
  salvage (→ uft-filesystem).
---

# UFT Recovery Integrity

Use this skill when touching any code that produces "recovered" bytes the
user will later trust. Forensic preservation has one rule that overrides
everything else: **a byte either came from the disk, or it didn't, and
the output must be honest about which.** Most ways recovery code fails
are violations of that rule disguised as cleverness.

This skill exists because the audit at commit 70e60fc surfaced 6
data-integrity bugs in `src/recovery/`, all of the same family. They
shipped because no skill enforced these invariants. This is that skill.

## When to use this skill

- Modifying any file under `src/recovery/`
- Modifying `src/sector/`, `src/flux/uft_flux_decoder.c`, or any decoder
  that emits bytes the user will trust
- Adding a new "fill-the-gap", "repair", "correct", or "reconstruct"
  function anywhere in the tree
- Reviewing a PR that claims to "improve recovery rate" — almost always
  something here is being traded for integrity
- Implementing a function that consumes `confidence_map`, `source_pass`,
  or `flags` from `uft_forensic_types.h`

**Do NOT use this skill for:**
- DeepRead pipeline modules — those have their own architectural skill
  (`uft-deepread-module`); use it. This skill is for the standalone
  recovery utilities, not pipeline passes.
- Format-plugin reads on healthy data — `uft-format-plugin` covers that.
- New CRC polynomial implementations — `uft-crc-engine`.
- Filesystem-level salvage (FAT recovery, AmigaDOS undelete) —
  `uft-filesystem`.
- Performance optimization of a recovery function whose correctness is
  established — `uft-benchmark` after this skill, not instead of it.

## The five invariants (non-negotiable)

These are derived from `docs/DESIGN_PRINCIPLES.md` Principle 7 and the
audit findings. Every recovery function must satisfy all five, or it is
broken regardless of how many tests pass.

| # | Invariant | Common violation |
|---|-----------|------------------|
| **I1** | Bytes that didn't come from media must be marked unknown | Linear interpolation of binary data |
| **I2** | Probabilistic corrections must verify uniqueness | Single-bit CRC flip accepts first match |
| **I3** | Verifiability information must propagate | `vote_byte` ignores `crc_ok` |
| **I4** | Counter widths must match input scale | `uint8_t counts[256]` for unbounded read counts |
| **I5** | Confidence is reported, never inferred by the consumer | "200/256 reads agreed" reported as `0xFF` confidence |

Each invariant has a section below.

## The shared types you must use

Don't invent your own validity tracking. The project already has it:

```c
/* include/uft/recovery/uft_forensic_types.h */
typedef struct {
    uint8_t *data;              /* the bytes (output) */
    size_t   data_size;
    float    quality;           /* 0.0..1.0 */
    uint8_t *confidence_map;    /* per-byte 0..255, may be NULL */
    uint8_t *source_pass;       /* per-byte: which read produced it */
    uint32_t flags;             /* UFT_FS_FLAG_WEAK | _DELETED | _RECONSTRUCTED */
    /* ... */
} uft_forensic_sector_t;
```

Three rules:

- If you write `data[i]` from anywhere other than a verified-clean
  read, you must also set `confidence_map[i]` to reflect that, AND set
  the `UFT_FS_FLAG_RECONSTRUCTED` bit if `confidence_map[i] < 255`.
- If you have no `confidence_map` available, allocate one — don't
  silently emit data with no confidence info. A NULL map is a contract
  with the caller saying "trust every byte at face value", and that's
  almost never what a recovery function should claim.
- The `source_pass` field is for multi-read code; it tells callers which
  pass each byte came from. Update it whenever your output changes.

## Workflow

### Step 1: Classify the function

Before writing or modifying code, classify what kind of recovery
operation it is. Different classes have different rules.

| Class | Example | Key rule |
|-------|---------|----------|
| **Voting / averaging** | `vote_byte`, `average_sector_reads` | I3, I4 |
| **CRC-driven correction** | `try_single_bit_correction` | I2 |
| **Gap interpolation** | `reconstruct_sector` | I1 (almost always wrong) |
| **Cross-track reconstruction** | `uft_cross_track.c` | I1, I5 |
| **Multi-revolution fusion** | (multiread / OTDR-adjacent) | I3, I5 |

Write the class as a comment at the top of your function. It anchors
review.

### Step 2: Write the integrity preconditions

Before the actual algorithm, document what the function promises.

```c
/* CLASS: voting
 *
 * INTEGRITY CONTRACT:
 *   - Every byte in `output` is either from a verified-OK pass (high conf)
 *     or marked weak in `weak_mask` (low conf).
 *   - `confidence_map[i]` reflects vote concentration, never less than
 *     0 or greater than 255.
 *   - If no pass had crc_ok and no pass was provided at all, output[i]
 *     is set to 0xFF and confidence_map[i] is 0 — never silent zeros.
 *   - read_count up to UINT32_MAX is supported (counters use uint32_t).
 */
```

If you can't write the contract, the function isn't ready to be coded.

### Step 3: Apply the relevant invariants

#### I1 — Bytes that didn't come from media must be marked unknown

The single most common violation. **Never** apply continuous-data
operations (interpolation, smoothing, averaging across positions) to
binary disk data. Disk bytes are not a signal, they are an opaque
sequence — `(0xF3, ?, 0x12)` cannot be filled in.

```c
/* WRONG — linear interpolation makes no semantic sense for binary data */
output[gap_start + j] = start_val +
    (end_val - start_val) * (j + 1) / (gap_len + 1);

/* RIGHT — emit a sentinel and mark unknown */
output[gap_start + j] = 0xFF;             /* sentinel */
out_confidence_map[gap_start + j] = 0;    /* explicit zero confidence */
out_flags |= UFT_FS_FLAG_RECONSTRUCTED;   /* caller can refuse to use */
```

The audit found this exact bug at `src/recovery/uft_sector_recovery.c:154`.
See `reference/audit_2026_04_25.md` for the full case study.

The exception: **filesystem-aware** recovery (e.g., FAT directory
sentinels, AmigaDOS BAM reconstruction) where domain rules say "this
byte must be 0x00 because the FS spec says so". That's not
interpolation, that's spec-driven reconstruction. Mark it with
`UFT_FS_FLAG_RECONSTRUCTED` anyway.

#### I2 — Probabilistic corrections must verify uniqueness

CRC-based correction (single-bit, double-bit, byte-wise) only works if
the correction is **unique**. CRC-16 over 512 bytes has 4096 possible
single-bit flips and 65 536 possible CRC values, so collisions happen
often enough to matter (≈6% per sector under realistic damage models).

```c
/* WRONG — first hit wins */
for (size_t i = 0; i < len; i++) {
    for (int b = 0; b < 8; b++) {
        data[i] ^= (1 << b);
        if (crc_matches(...)) return 1;   /* could be a coincidence */
        data[i] ^= (1 << b);
    }
}

/* RIGHT — find ALL hits, accept only if exactly one */
int hits = 0;
size_t hit_byte = 0; uint8_t hit_bit = 0;
for (...) {
    data[i] ^= (1 << b);
    if (crc_matches(...)) {
        if (++hits > 1) {
            data[i] ^= (1 << b);            /* undo current */
            data[hit_byte] ^= (1 << hit_bit); /* undo first */
            return -1;                       /* AMBIGUOUS, fail */
        }
        hit_byte = i; hit_bit = b;
        /* leave flipped, keep searching */
    } else {
        data[i] ^= (1 << b);
    }
}
return hits == 1 ? 1 : 0;
```

Same logic applies to double-bit and byte-flip variants — uniqueness
or fail.

The audit found this at `src/recovery/uft_bitstream_recovery.c:101`.

#### I3 — Verifiability information must propagate

If a data structure carries `crc_ok`, `quality`, `verified`, or any
other trust indicator, **every consumer must read it**. Throwing it
away in the next layer is silent corruption.

```c
/* WRONG — democratic vote ignores trust */
for (uint8_t p = 0; p < pass_count; p++) {
    counts[passes[p].data[offset]]++;   /* crc_ok ignored */
}

/* RIGHT — verified-OK passes dominate, fallback to weighted otherwise */
bool any_ok = false;
for (uint8_t p = 0; p < pass_count; p++)
    if (passes[p].crc_ok) { any_ok = true; break; }

uint32_t weighted[256] = {0};
for (uint8_t p = 0; p < pass_count; p++) {
    if (any_ok && !passes[p].crc_ok) continue;       /* drop unverified */
    uint32_t w = passes[p].crc_ok
                   ? 100 + passes[p].quality
                   : passes[p].quality;
    weighted[passes[p].data[offset]] += w;
}
```

The audit found this at `src/recovery/uft_multiread_pipeline.c:200`.

**Linter for this class:** if a struct field is set somewhere but never
read in the consuming layer, that's a smell — either delete the field
(it's dead state) or wire it up (it's a missed signal).

#### I4 — Counter widths must match input scale

A counter in a recovery loop must be wider than the maximum count it
could ever hold. Don't trust the current `MAX_PASSES` constant —
recovery loops get reused with new bounds.

```c
/* FRAGILE — fine today, broken when MAX_PASSES grows past 255 */
uint8_t counts[256] = {0};
for (size_t r = 0; r < read_count; r++) counts[reads[r][i]]++;

/* ROBUST */
uint32_t counts[256] = {0};
for (size_t r = 0; r < read_count; r++) counts[reads[r][i]]++;
```

Memory cost: 768 extra bytes per sector. Recovery is not the place to
shave kilobytes.

#### I5 — Confidence is reported, never inferred

If a recovery function knows that 7 of 11 reads agreed on a byte, it
must say so explicitly via `confidence_map`, not by emitting "0xFF" and
hoping the caller infers. The caller might be a UI, a CSV export, or
another recovery layer; each needs the explicit number.

```c
/* WRONG — caller has to guess */
output[i] = best_value;
/* (no update to confidence_map) */

/* RIGHT */
output[i] = best_value;
if (confidence_map) {
    uint32_t conf = (best_count * 255u) / valid;
    if (ties > 1) conf /= ties;       /* ties → low confidence */
    confidence_map[i] = (uint8_t)conf;
}
```

### Step 4: Validity-mask the gaps explicitly

Whenever your function leaves a byte unwritten or writes a sentinel,
update **both** `confidence_map[i] = 0` AND set
`UFT_FS_FLAG_RECONSTRUCTED` in `flags` if any byte was reconstructed.
The downstream tools rely on this.

```c
if (!have_data) {
    output[i] = 0xFF;
    if (confidence_map) confidence_map[i] = 0;
    sector_out->flags |= UFT_FS_FLAG_RECONSTRUCTED;
}
```

### Step 5: Run the integrity-lints script

```bash
bash .claude/skills/uft-recovery-integrity/scripts/lint_recovery.sh \
    src/recovery/<your_file>.c
```

The script checks for:
- Linear-interpolation patterns (`* (j+1) / (gap_len`)
- First-hit accept patterns (`if (crc_match) return 1`)
- Counter-overflow patterns (`uint8_t counts[256]`)
- Read of `crc_ok` (warns if struct has it but the function doesn't reference it)
- Unconditional writes to `output[i]` without a corresponding
  `confidence_map[i]` write within ±10 lines

It emits warnings, not errors — sometimes the pattern is intentional.
Read each warning and either fix or annotate `/* INTEGRITY-OK: <reason> */`
on the same line.

## Verification

```bash
# 1. Compile clean with extra paranoia
gcc -std=c11 -Wall -Wextra -Werror \
    -fsanitize=undefined,address \
    -I include/ -c src/recovery/<file>.c

# 2. Run the integrity lints
bash .claude/skills/uft-recovery-integrity/scripts/lint_recovery.sh \
    src/recovery/<file>.c
# expect: 0 warnings, OR every warning has /* INTEGRITY-OK: ... */

# 3. Property-based test for correction functions
# (For CRC correction specifically — generates random sectors,
# flips known bits, checks that single-flip is rejected when ambiguous)
cd tests && make test_recovery_properties && \
    ./test_recovery_properties --iterations 10000

# 4. Confidence-map invariant check on the multi-read pipeline
./uft recover tests/vectors/scp/weak_sector_at_track_18.scp --emit-confidence
python3 .claude/skills/uft-recovery-integrity/scripts/check_confmap.py \
    /tmp/uft_recovery_dump.json
# expect: no byte has confidence > 0 unless it was actually verified

# 5. Fuzz the gap-interpolation entry points
afl-fuzz -i tests/fuzz_corpus -o /tmp/fuzz_recovery \
    -- ./fuzz_reconstruct @@
```

## Common pitfalls

### "It's just a small gap, interpolation is fine"

It is never fine for binary data. There is no information-theoretic
justification for interpolating between two bytes whose semantic
relationship is unknown. The only correct answer is "I don't know",
which means: 0xFF + confidence 0 + RECONSTRUCTED flag.

### "CRC matched, so the correction must be right"

CRC-16 is a 16-bit hash. With 4096 single-bit candidates, collisions
happen a few percent of the time. The CRC matched only proves the
candidate is *consistent*, not that it's *unique*. Always check
uniqueness.

### "Voting always picks the right answer"

It picks the *most common* answer, which equals "the right answer" only
when the majority of reads are correct. If 4 of 5 reads failed CRC,
they outvote the one CRC-OK read — and the result is whatever
corruption happened to be common. Use I3.

### "I'll add the confidence map later"

Adding `confidence_map` later means: every test for the function so far
has run with NULL, every code path has been allowed to skip the writes,
and now you have to retrofit. Add it from the start, then it never
gets skipped.

### "The function's output looks good, ship it"

"Looks good" against synthetic data with known answers is meaningless.
The forensic-recovery test bar is:

- Synthetic perfect input → 100% match, confidence 255
- Synthetic with 5% damage → matched bytes confidence 255,
  damaged bytes confidence < 100, none silently corrupted
- Synthetic with 50% damage → most bytes flagged RECONSTRUCTED,
  no false confidence

If your function passes only the first case, it isn't done.

### Testing with a single fixture

A single weak-bit fixture proves nothing about the corner cases. Use
`uft-flux-fixtures` to generate the four standard variants (minimal,
weak_sector, corrupted_crc, truncated) and run all four.

## Related

- `.claude/skills/uft-deepread-module/` — pipeline modules; this skill is for stand-alone recovery utilities
- `.claude/skills/uft-crc-engine/` — CRC implementations; the correction *strategies* live here
- `.claude/skills/uft-flux-fixtures/` — synthetic damaged inputs
- `.claude/skills/uft-debug-session/` — when a recovery bug has a repro
- `.claude/agents/forensic-integrity.md` — agent that audits these invariants on PRs
- `docs/DESIGN_PRINCIPLES.md` Principle 7 (honesty matrix)
- `include/uft/recovery/uft_forensic_types.h` — canonical types
- `src/recovery/uft_recovery_meta.c` — reference for confidence-map handling
- `reference/audit_2026_04_25.md` — the six bugs that motivated this skill
