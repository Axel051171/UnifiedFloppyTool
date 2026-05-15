# UFT Hardware-Provider Audit

Forensic validation of the V2 hardware-provider layer
(`src/hardware_providers/*_provider_v2.{h,cpp}`) against the official
reference sources of each backend.

## Scope correction (read first)

The original audit brief was written against the **V1 architecture**
(`HardwareManager`, `makeProvider()` substring dispatch, `HardwareProvider`
virtual base, a Catweasel provider, an 11-provider count). That
architecture was **deleted** by refactor tasks P1.17–P1.19 on branch
`refactor/type-driven-hal`. This audit is therefore re-scoped to the
**real V2 surface**:

| Brief said | Reality on this branch |
|---|---|
| 11 providers incl. Catweasel + Mock | **9 V2 providers** built by `.pro` |
| Catweasel provider | does not exist — "catweasel" is only a *format* (`uft_cw_raw.c`) |
| Mock as shipping provider | `MockProviderV2` lives in `tests/`, a test double |
| D3: `makeProvider()` substring dispatcher | deleted (P1.17/MF-169); GUI wiring is now codegen `wire_action<cap::X>` |
| D3: virtual `readTrack`/`writeTrack` of a base class | V2 = `final` classes, mixin composition + C++20 concepts |

The 9 audited providers: **greaseweazle, scp, kryoflux, fluxengine,
fc5025, xum1541, applesauce, adfcopy, usbfloppy**.

D3 is re-defined for V2: instead of "does the substring dispatcher hit",
it asks **"does `HardwareTab` route to this provider at all, and does the
codegen `wire_action` gate its capability buttons correctly?"**

## The five dimensions

| Dim | Question | Evidence form |
|-----|----------|---------------|
| **D1** | Wire-protocol / CLI contract byte-/arg-exact vs. the official reference | constants diff (JSON), `test_*_vectors.c` static asserts |
| **D2** | Can UFT actually process what the backend delivers? | code-path trace from `do_*()` to the image-format sink, every conversion named |
| **D3** | Is the provider↔GUI coupling real? (V2: codegen routing + capability gating) | routing table, capability-match table |
| **D4** | Is the real device found per OS (Win/Linux/macOS)? | OS-enumeration code snippet + cross-check vs. official device identification |
| **D5** | Forensic integrity — fake values, silent fallbacks, swallowed errors, stub façades | code citation + severity |

Each dimension is graded **PASS / PARTIAL / FAIL / UNVERIFIED**. Every
UNVERIFIED states *why* (which test is missing, what would be needed).

## Layout

```
audit/
  README.md            ← this file
  MASTER_REPORT.md      ← cross-provider synthesis + prioritised fix list
  <provider>/
    REPORT.md           ← the per-provider audit
    extract_uft.py      ← pulls UFT-side constants from headers/source
    extract_ref.py      ← the official reference values + provenance
    diff.py             ← compares the two, writes evidence.json
    test_<backend>_vectors.c   (native USB/serial backends)
      or mock_<backend>.py     (CLI-wrapper backends)
    evidence.json       ← snapshot of every compared value
```

## Reference provenance honesty

The "reference" column is only as strong as its source. Three grades:

- **vendored** — a copy of the official source is in the repo. (none yet)
- **recalled** — the reference value is well-known protocol knowledge but
  not vendored. Marked as such; the value is correct but the *audit* of
  it is one step weaker than a vendored diff.
- **needs-source** — the reference could not be established →
  the dimension is UNVERIFIED with a note on what to vendor.

A `recalled` D1 PASS means "UFT matches the protocol as documented in the
official project, cross-checked from memory" — not "byte-diffed against a
checked-in copy". Upgrading `recalled` → `vendored` is a tracked follow-up.

## Reproduce

Per provider:

```bash
cd audit/<provider>
python extract_uft.py    > /tmp/uft.json     # UFT-side constants
python extract_ref.py    > /tmp/ref.json     # official reference
python diff.py                                # writes evidence.json, prints verdict
# native backends additionally:
gcc -std=c11 -I../../include -fsyntax-only test_<backend>_vectors.c
```

`diff.py` exits non-zero on any FAIL — it is CI-able as a conformance gate
once the `recalled` references are upgraded to `vendored`.
