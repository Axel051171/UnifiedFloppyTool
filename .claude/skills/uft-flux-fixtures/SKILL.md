---
name: uft-flux-fixtures
description: |
  Use when generating synthetic flux/bitstream/sector test data (SCP, HFE,
  KryoFlux RAW, ADF, D64). Trigger phrases: "erzeuge testdaten", "synthetic
  flux file", "test fixture für X", "PoC input data", "minimal valid SCP",
  "generate fake disk image", "weak bit fixture", "corrupted CRC fixture".
  Fully functional generators for 5 formats. DO NOT use for: real disk
  captures (copyright risk), performance fuzzing (use real dumps), UI
  testing (Qt test framework instead).
---

# UFT Flux Fixtures

Use this skill when generating synthetic test data for UFT's tests or PoCs.
Provides **fully working** generators for SCP, HFE v1, KryoFlux RAW, ADF,
and D64 — no TODO markers, no incomplete stubs.

## When to use this skill

- Writing a new unit test that needs deterministic input
- Building a PoC that validates a decoder
- Reproducing a bug report (generating the minimal input that triggers it)
- Regression tests where a real disk isn't available or allowed

**Do NOT use this skill for:**
- Real disk captures — copyright risk, use `tests/vectors/real/` with
  proper licensing
- Performance fuzzing — use real dumps from `~/.uft/samples/` instead
- Qt UI testing — use `QTest` and `QSignalSpy`
- Filesystem tests that need realistic content — combine this with
  filesystem generators from `uft-filesystem`

## The 5 generators

| Generator | Output format | Lines | Purpose |
|-----------|---------------|-------|---------|
| `gen_scp_fixture.py` | SCP | ~180 | Flux tests — PLL, multi-rev |
| `gen_hfe_fixture.py` | HFE v1 | ~150 | Bitstream tests |
| `gen_kryoflux_fixture.py` | KF RAW | ~120 | Raw-stream parser tests |
| `gen_adf_fixture.py` | ADF | ~80 | Amiga filesystem tests |
| `gen_d64_fixture.py` | D64 | ~100 | C64 filesystem tests |

All generators share the same idempotency rules: re-running produces
byte-identical output.

## Workflow

### Step 1: Pick the generator

Start from the smallest generator that exercises what you're testing. If
you're testing PLL, use SCP. If you're testing sector decode, use ADF.

### Step 2: Run it

```bash
cd <uft-root>
python3 .claude/skills/uft-flux-fixtures/scripts/generators/gen_scp_fixture.py
```

Output goes to `tests/vectors/<format>/`. The generator is self-contained;
no arguments needed for the standard fixtures.

### Step 3: Commit both

Fixtures AND generators are committed:

```bash
git add tests/vectors/scp/minimal_mfm_80tracks.scp
git add .claude/skills/uft-flux-fixtures/scripts/generators/gen_scp_fixture.py
```

### Step 4: Use in C test

```c
static const char *FIXTURE =
    "tests/vectors/scp/minimal_mfm_80tracks.scp";

TEST(parse_fixture_returns_80_tracks) {
    uft_scp_file_t scp;
    ASSERT(uft_scp_open(FIXTURE, &scp) == UFT_OK);
    ASSERT(scp.track_count == 80);
    uft_scp_close(&scp);
}
```

For CMake to find it, set working directory:

```cmake
set_tests_properties(test_scp_parser PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
```

## Fixture variant conventions

Every generator emits **4 variants** by default:

| Pattern | Purpose |
|---------|---------|
| `minimal_<format>.ext` | Smallest valid file |
| `weak_sector_<format>.ext` | Includes induced jitter for recovery tests |
| `corrupted_crc_<format>.ext` | Intentionally bad CRC for error-path tests |
| `truncated_<format>.ext` | File cut mid-chunk for robustness tests |

Always generate all 4. They're small (<50 KB each) and the test suite
benefits from having them available.

## Verification

```bash
# 1. Run all generators
for gen in .claude/skills/uft-flux-fixtures/scripts/generators/gen_*.py; do
    python3 "$gen"
done

# 2. Verify determinism — re-run, should diff clean
for gen in .claude/skills/uft-flux-fixtures/scripts/generators/gen_*.py; do
    python3 "$gen"
done
git diff --stat tests/vectors/
# expect: no changes

# 3. Verify fixtures are parseable by UFT
for f in tests/vectors/*/minimal_*; do
    ./uft info "$f" >/dev/null 2>&1 && echo "OK: $f" || echo "FAIL: $f"
done

# 4. Fixture sizes reasonable (<1MB each)
find tests/vectors/ -size +1M
# expect: no output
```

## Determinism rules

- `random.seed(0x4E554654)` at top of every generator — the constant is
  `'NUFT'` in ASCII, chosen arbitrarily but fixed
- No `time.time()` / timestamps in output
- `locale.setlocale(locale.LC_ALL, 'C')` if any string formatting
- Byte-exact reproducibility: `diff -q` must be silent on re-run

Test this in CI:

```bash
# .github/workflows/fixtures.yml adds this:
python3 scripts/regenerate_fixtures.py
git diff --exit-code tests/vectors/
```

## Common pitfalls

### Using Python's `random.randint` without seeding

```python
# WRONG — non-deterministic
jitter = random.randint(-50, 50)

# RIGHT — seeded at module load
random.seed(0x4E554654)
jitter = random.randint(-50, 50)
```

### Using real captured data as "synthetic"

Synthetic means **computed**, not "copied from a real disk with headers
stripped". Real disk bytes carry copyright. Generate from algorithmic
specifications.

### Fixture too large

A fixture over 500KB probably tests something that should be tested at a
lower level. Split into smaller fixtures targeting specific code paths.

### Name collision with real disks

Anything in `tests/vectors/` is synthetic. Real-disk captures (if any,
with licenses) go in `tests/vectors/real/` and are `.gitignore`d. Never
mix the two directories.

## Related

- `.claude/skills/uft-format-plugin/` — use these fixtures to test plugins
- `.claude/skills/uft-benchmark/` — benchmarks need deterministic input
- `.claude/skills/uft-filesystem/` — filesystem tests combine these with
  FS generators
- `tests/vectors/` — target directory
- `docs/DESIGN_PRINCIPLES.md` Principle 1 (no silent data loss —
  fixtures must faithfully represent pathologies)
