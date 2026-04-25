# UFT Code Review Prompt

Use this when asking an LLM to review a specific file or PR for
correctness, style, and architectural fit. Customize bracketed sections.

---

## Context

You are reviewing UFT (UnifiedFloppyTool) code. This is a Qt6/C/C++
floppy-disk forensics tool — github.com/Axel051171/UnifiedFloppyTool.

**Files under review:**
- `[src/path/file.c]` (lines [N-M])
- `[src/path/header.h]`

**Recent state to consider:**
- Last release: `[v4.x.y]`
- Recent commits affecting this area: `[hashes or "see git log"]`
- Open issues touching this code: `[issue #N if any]`

**Author of changes (for review tone):**
- This is `[my own work / a contributor PR / external]`
- Reviewer should be `[strict / collaborative / mentoring]`

## Project rules to enforce

### Honesty (Principle 7, hard rule)

- No silent UFT_OK returns from empty bodies
- No phantom features (capability flag without backing implementation)
- No silent data loss (return error, don't zero-pad on read failure)
- No silent precision degradation (don't round-up requested resolution)

### Dual-target constraints

If the file is in any of these directories, firmware constraints apply:
- `src/flux/`
- `src/algorithms/`
- `src/crc/`
- `src/analysis/otdr/`

Firmware constraints:
- No `double`, only `float`
- No x86 intrinsics without `#ifdef UFT_HOST_X86` fallback
- No malloc/free in hot paths
- No `size_t` / `int` in binary struct layouts

### Architecture rules

- Plugin files use `UFT_REGISTER_FORMAT_PLUGIN` macro
- HAL backends are pure C (no Qt) in `src/hal/`
- Qt providers wrap HAL in `src/hardware_providers/`
- Format plugins don't touch each other directly — go through registry

## Output format (mandatory)

```
## TL;DR
[1-2 sentences: overall verdict — APPROVE / REQUEST_CHANGES / BLOCK]

## Critical issues (block merge)
[empty if none — list with file:line + concrete fix suggestion]

## Important issues (should fix)
[important but not blocking — same format]

## Suggestions (nice to have)
[style, naming, minor improvements]

## What's good
[2-4 specific positive observations to balance the review]

## Not reviewed
[things outside scope — e.g. "interaction with worker threads",
 "performance characteristics", "thread safety"]
```

## Anti-goals

- No nitpicks on personal style (4 vs 8 space indent, brace placement)
  unless project convention is violated
- No "consider rewriting" without specific concrete alternative
- No suggestions that pull in new dependencies without strong
  justification
- No demands for documentation that doesn't exist for similar files
  in the same directory

## What I want from this review

- [ ] Find correctness bugs (memory, off-by-one, integer overflow,
       error path coverage)
- [ ] Find architectural mismatch (does this fit how UFT does similar
       things elsewhere?)
- [ ] Verify tests cover the change
- [ ] Confirm honesty rules respected
- [ ] If hot path: flag obvious perf issues (but don't optimize —
       that's a separate review)

## Reference

- `docs/AI_COLLABORATION.md` — collaboration rules
- `docs/DESIGN_PRINCIPLES.md` — Principle 7 honesty
- `docs/COMPLIANCE_RULES.md` — MF-001 through MF-013
- Similar implementations to compare against:
  - `[reference_file_1]` — canonical pattern for this kind of code
  - `[reference_file_2]` — newer alternative pattern
