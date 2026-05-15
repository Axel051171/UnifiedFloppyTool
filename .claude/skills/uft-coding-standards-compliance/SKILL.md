---
name: uft-coding-standards-compliance
description: |
  Use when closing gaps from the Master Coding Standards v1.0
  (MysticFoxDE, see memory/coding_standards.md §11). Trigger phrases:
  "H-1 / H-2 / H-9 / F-2 / D-2 fix", "capability manifest", "deprecate
  alias", "GUI HAL wiring test", "SPEC_STATUS marker", "preflight in
  converter X", "compliance gap", "rule X verletzt", "standards-
  konformität". This skill enforces the rule numbers, the 3-part error
  contract (F-4), and the rollout pattern (additive in v4.x, mandatory
  in v5.0). DO NOT use for: brand-new HAL backends (→ uft-hal-backend),
  format plugins (→ uft-format-plugin), bugs unrelated to the standard
  (→ uft-debug-session / quick-fix).
---

# UFT Coding Standards Compliance

Use this skill to close gaps from the Master Coding Standards v1.0.
The standard lives in `memory/coding_standards.md` (durable across
sessions). Section §11 lists the live gap inventory.

## When to use this skill

- Closing a labelled gap (H-1, H-2, H-9, H-3/H-4, F-2, D-2)
- Marking a duplicate field `[[deprecated]]`
- Replacing a silent stub (`return false; Q_UNUSED`) with
  `throw NotSupportedError` or `UFT_ERR_NOT_IMPLEMENTED` + 3-part
  message
- Adding the SPEC_STATUS marker to reverse-engineered code
- Wiring `uft_preflight` into one of the 44 `convert_*` paths
- Building the GUI↔HAL wiring test

**Do NOT use this skill for:**
- Implementing new HAL backends from scratch — use `uft-hal-backend`
- New format plugins — use `uft-format-plugin`
- Performance regressions — use `uft-benchmark`
- Bugs that don't map to a standard rule — use `uft-debug-session`

## The standard at a glance

The standard's identifiers are stable. NEVER reuse a number even if
a rule is removed. When you fix a gap, mark the corresponding rule in
`memory/coding_standards.md` "Was schon erledigt ist" section.

Pflichtgrade:
- **MUST** = CI breaks if violated.
- **SHOULD** = expected; PR-justification needed for exceptions.
- **MAY** = recommendation.

Konflikt-Hierarchie: Forensik-Mission > Coding-Regel.

## Rule playbooks

### H-9 — Deprecate duplicate DTO fields

The DTO has two ways to express the same fact (`success`/`valid`,
`error`/`errorMessage`). Deprecation rollout:

1. **Identify duplicates.** Read the DTO header end-to-end. Confirm both
   fields encode the same information; do not deprecate fields that
   have subtly different semantics — that's a rename.
2. **Pick the canonical name.** Convention: shorter + factual wins.
   `success` over `valid`. `error` over `errorMessage`. Document the
   choice in the commit body.
3. **Mark the alias `[[deprecated]]`.** With message pointing at the
   canonical: `[[deprecated("MF-NNN: use error; will be removed in v5.0")]]`.
4. **Find writers + readers.** `Grep -n "\.errorMessage\b"` and
   `\.valid\b`. Compiler will surface them too once deprecation lands.
5. **Convert call-sites in this commit.** Each writer must set BOTH
   fields during the deprecation window so older readers still work.
   Cleanest: write the canonical, then mirror to alias in one place.
6. **Add a v5.0 removal task.** Either as an entry in
   `KNOWN_ISSUES.md` or as a TODO comment carrying the MF-NNN id.

Verification:
- `g++ -Wdeprecated-declarations -Werror=deprecated-declarations`
  on a TU that uses the field surfaces non-converted callers as errors.
- New tests: none required (deprecation is a no-op runtime change).
  But ensure existing tests compile clean.

### H-1 / H-2 — Capability manifest + ban silent defaults

Multi-step migration. Don't rush — touch everything in one PR makes
the diff unreviewable.

Phase A (v4.2 additive — recommended scope per session):
1. Add `enum class Capability` and `using Capabilities = QFlags<...>;`
   in a new header `include/uft/hal/uft_hal_capabilities.h`.
2. Add `virtual Capabilities supported() const = 0;` to base.
3. Each subclass declares its caps explicitly. Default for legacy
   providers: `Capability::Legacy` (until reviewed).
4. Add `enum class HalStatus { STUB, PARTIAL, PRODUCTION, DEPRECATED }`
   and `static constexpr HalStatus kStatus`. CI rejects STUB in
   release-registry — that gate is a separate commit.
5. Default-bodies of virtual methods: change every
   `return false; Q_UNUSED(...)` to `throw NotSupportedError(...)`.
   Add the exception class in the same commit.

Phase B (v5.0 enforcement, separate session):
6. Make `supported()` the single source of truth — remove redundant
   per-method capability bools.
7. Remove the `Legacy` default; every provider must opt in.

### H-3 / H-4 — GUI ↔ HAL wiring test

`tests/test_gui_hal_wiring.cpp.template` is the starting point. Test
walks the .ui catalog, instantiates each provider, fires every
QAction, asserts:
- The slot is non-empty (no comment-only stubs).
- The slot only calls capabilities the provider declares.

Workflow:
1. Read the template; copy to `tests/test_gui_hal_wiring.cpp`.
2. Use `QUiLoader` to load each `forms/*.ui`.
3. For each `QPushButton`/`QAction`, walk `metaObject()->method()` and
   confirm every connected slot ends up calling something on the
   provider.
4. Test must pass for `MockHardwareProvider` (which supports
   everything) and fail predictably for `STUB`-status providers.
5. Add to `tests/CMakeLists.txt` (auto-discovered via GLOB).

### F-2 — Wire uft_preflight into convert_* paths

Each of the 44 conversion paths in `src/formats/uft_format_convert*.c`
must call `uft_preflight(src_format, dst_format, &report)` BEFORE the
conversion and propagate the report on data-loss.

Workflow per path:
1. Find the conversion entry point (one per path).
2. Add at top: `uft_loss_report_t loss = {0};`
3. Call `uft_preflight(...)`. If it returns `UFT_ERR_LOSS_REJECTED`,
   abort with 3-part message:
   - **What:** "Conversion X→Y will drop {feature_a, feature_b}"
   - **Why:** "{feature_a} is not representable in target format"
   - **Fix:** "Pass `--accept-data-loss` to proceed; see docs/LOSS.md"
4. Otherwise, run the conversion and attach `loss` to the result.
5. Track each migrated path in `KNOWN_ISSUES.md §1.2` checklist.

Don't try all 44 in one commit. One PR per path or per logical group
(e.g., all flux→sector paths together).

### D-2 — SPEC_STATUS markers

Reverse-engineered or vendor-undocumented code carries a header
comment within the first 30 lines:

    SPEC_STATUS: REVERSE-ENGINEERED — based on <source>
    or
    SPEC_STATUS: VENDOR-DOCUMENTED — based on <SDK reference>
    or
    SPEC_STATUS: PUBLISHED-STANDARD — <RFC/ECMA/ISO ref>

Audit script `scripts/audit_skeleton_headers.py` should grep for
this marker in:
- `src/hal/uft_*.c` (hardware protocols)
- `src/formats/*/uft_*.c` (file-format parsers)
- `src/protection/*/uft_*.c` (copy-protection detectors)

Files lacking the marker → CI warning. Files with REVERSE-ENGINEERED
+ no `<source>` → CI error.

## Commit + tracking convention

- **Conventional Commits** (rule G-1): `fix(scope): subject (MF-NNN)`.
  Scope examples: `hal`, `hardware`, `format`, `protection`,
  `gui`, `convert`.
- **MF-NNN counter** is the single linear tracker; pick the next
  available number from the latest commit's body. NEVER reuse.
- **Body must include** which standards rule(s) the commit satisfies.
- Mark the rule done in `memory/coding_standards.md` "Was schon
  erledigt ist" list.

## Pre-commit gates (always run)

```bash
python scripts/check_consistency.py           # 4 categories, must be 0
python scripts/verify_build_sources.py        # 0 new regressions
```

If `verify_build_sources.py` reports `Baseline entries resolved`,
optionally run `--rebuild-baseline` in a separate cleanup commit.

## Where to look

| Topic | Source of truth |
|-------|------------------|
| Live rule list | `memory/coding_standards.md` |
| Open gaps | §11 of the standard |
| HAL canonical reference | `src/hal/uft_greaseweazle_full.c` |
| DTO with current H-9 violations | `src/hardware_providers/hardwareprovider.h` |
| Wiring test template | `tests/test_gui_hal_wiring.cpp.template` (TBD) |
| Preflight helper | `src/formats/uft_preflight.c` (per KNOWN_ISSUES §1.2) |

## Related skills

- `uft-hal-backend` — for adding NEW backends (this skill is for
  fixing existing ones to match the standard)
- `uft-format-plugin` — for new file-format support
- `uft-debug-session` — for bugs that aren't standards-related
- `uft-release` — when bumping minor/major after a wave of compliance
  fixes
