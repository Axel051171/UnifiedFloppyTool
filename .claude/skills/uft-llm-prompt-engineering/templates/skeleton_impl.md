# UFT Skeleton-Function Implementation Prompt

Use when asking an LLM to implement one (or a few) function(s) from a
`UFT_SKELETON_PLANNED` header.

---

## Context

You are implementing a function from a UFT skeleton header. UFT is a
Qt6/C/C++ floppy-disk forensics tool.

**Skeleton header:** `include/uft/[path]/[header].h`
**Function to implement:** `[exact function signature]`
**Implementation file:** `src/[path]/[file].c` (create if doesn't exist,
or extend existing)

**Existing context:**
- Other functions in the same header that ARE implemented:
  `[list them]`
- Functions in the header still skeleton:
  `[list them — important so you don't add callsites to those]`

## The skeleton convention (read carefully)

The header has this banner:

```
* * UFT_SKELETON_PLANNED * *
This header declares N public functions, of which M have no
implementation. Callers exist but will link-fail or silently no-op
until implemented.
Do NOT add new call sites to functions from this header without first
implementing them or removing the prototype.
* * * * * * * * * * *
```

**Two non-negotiable rules:**

1. After your implementation, the banner must be UPDATED:
   - Decrement M by the number of functions you implemented
   - If M reaches 0, replace banner with `STATUS: Implemented`

2. Inside the .c file, add a `UFT_SKELETON_PROMOTED` header listing:
   - IMPLEMENTED (functions you implemented now)
   - NOT_YET (still returning UFT_ERROR_NOT_IMPLEMENTED)
   - REMOVED (prototypes you decided to delete from .h)

## Constraints

**Honesty rule (mandatory):**

If you can't fully implement a function, return
`UFT_ERROR_NOT_IMPLEMENTED` explicitly. NEVER:
- Return `UFT_OK` from an empty body
- Output zero-padded data when computation fails
- Set output flags to true when feature isn't there

**Determinism rule (mandatory for ML):**

ML inference must be deterministic. Same input + same model = same
output. Random init in training must seed from `0x4E554654`. If
threading is used, expose a deterministic single-threaded mode.

**Firmware-portable (if file is in src/ml/, src/algorithms/, src/flux/):**

- Single-precision float only
- No malloc in hot paths (static buffers OK)
- No x86 intrinsics without portable fallback
- No external ML library dependencies without UFT_HAS_X guard

## Required deliverables

```
1. The implementation in src/[path]/[file].c
2. A unit test in tests/test_[module].c that:
   - Tests happy path (typical input → expected output)
   - Tests boundary (empty / single-element / max-size)
   - Tests invalid (NULL, negative, malformed)
   - Tests determinism (same input → same output)
3. Updated banner in the .h:
   - Decrement unimplemented count
   - Or replace banner with STATUS: Implemented if all done
4. Updated docs/KNOWN_ISSUES.md "Planned APIs" — strike out implemented
```

## Output format (mandatory)

```
## TL;DR
Implemented [function_name] — [one sentence about approach].

## Implementation
**File:** `src/[path]/[file].c`
```c
[complete function body, ready to paste]
```

## Test
**File:** `tests/test_[module].c`
```c
[complete test function(s), ready to paste]
```

## Header changes
**File:** `include/uft/[path]/[header].h`
**Banner update (before/after):**
```diff
- declares N public functions, of which M have no implementation
+ declares N public functions, of which M-1 have no implementation
```

## docs/KNOWN_ISSUES.md changes
[diff or list of strike-outs]

## Verification commands
```bash
# Compile
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -I include/ \
    src/[path]/[file].c

# Test
cd tests && make test_[module] && ./test_[module]

# Audit
python3 .claude/skills/uft-ml-implement-skeleton/scripts/audit_skeleton.py
```

## Not implemented in this change
[remaining skeleton functions — explicit list, not "the rest"]
```

## Anti-goals

- **No bulk implementation.** Implement ONE function per LLM session,
  with a working test. "Implement all 20 decoder functions" produces
  20 stubs and 0 tests. Stop after one.
- **No new external dependencies.** No ONNX, no TFLite, no PyTorch
  bindings without explicit pre-approval (and a `#ifdef` fallback).
- **No silent no-ops.** Either implement or explicit
  `UFT_ERROR_NOT_IMPLEMENTED`.
- **No callsite leakage.** Don't add calls to OTHER skeleton functions
  in your implementation.

## Reference

- The skeleton header itself
- `docs/AI_COLLABORATION.md` §1 (Arbeits-Reihenfolge)
- `docs/MASTER_PLAN.md` M1/MF-011 (DOCUMENT-Welle scope)
- `src/analysis/uft_ml_protection.c` — example of complete pure-C ML impl
- `.claude/skills/uft-ml-implement-skeleton/SKILL.md` — full workflow
