---
name: uft-llm-prompt-engineering
description: |
  Use when crafting prompts for an LLM to work effectively on UFT — for
  Claude Code sessions, Claude Chat reviews, GPT-4 PR reviews, or any
  LLM you brief on a UFT task. Trigger phrases: "prompt für ML decoder
  review", "wie briefe ich Claude für hotpath optimization", "system
  prompt UFT", "agent prompt", "prompt engineering für UFT review",
  "reviewer prompt format". Templates that channel AI_COLLABORATION.md
  rules into ready-to-paste prompts. DO NOT use for: writing code itself
  (different skills), prompt engineering for unrelated projects (this
  is UFT-specific), conversational chat without a task (just talk),
  hyperparameter tuning prompts for ML training (separate ops domain).
---

# UFT LLM Prompt Engineering

Use this skill when briefing an LLM to work on UFT — whether Claude
Code, Claude Chat, GPT, Gemini, or local. Translates `AI_COLLABORATION.md`
rules into prompt templates that prevent common failure modes.

## When to use this skill

- Starting a new Claude Code session on a non-trivial UFT task
- Briefing a chat-LLM to review code or PRs
- Writing a system prompt for a specialized agent
- Constructing a prompt for batched review (multiple files)
- Crafting a prompt for hard cases (cross-target, dual-platform)

**Do NOT use this skill for:**
- Writing the code itself — use the other skills (format-plugin,
  ml-implement-skeleton, etc.)
- Prompt engineering for non-UFT projects — this is project-specific
- Casual conversation with an LLM — just talk
- Tuning ML training hyperparameters — different domain

## The four prompt failure modes

These are the failure modes I've seen on UFT specifically. Every
prompt template addresses at least one:

| Failure | Symptom | Cause |
|---------|---------|-------|
| **Stale advice** | "Empfehlung X" — but X already done | LLM didn't read recent state |
| **Vague claims** | "Wahrscheinlich 2-3× schneller" with no measurement | No confidence rule enforced |
| **Architecture creep** | 3-line bug → 200-line refactor | No scope discipline in prompt |
| **Phantom features** | "I added function Y" — Y returns UFT_OK from empty body | No honesty rule cited |

The templates below are designed to make each of these fail loud.

## Workflow

### Step 1: Pick the right template

| Task type | Template |
|-----------|----------|
| Hotpath performance review | `templates/perf_review.md` |
| Code review of a specific file | `templates/code_review.md` |
| Implement a skeleton function | `templates/skeleton_impl.md` |
| Add a new format plugin | `templates/new_plugin.md` |
| Cross-cutting refactor | `templates/refactor.md` |
| Bug repro + fix | `templates/bug_repro.md` |

Each template has the same structure:

1. **State the codebase context** (point to relevant files + paths)
2. **State the constraint set** (firmware-portable? dual-target? hot path?)
3. **Cite the rules that apply** (AI_COLLABORATION.md sections)
4. **Define done** (concrete verification criteria)
5. **Anti-goals** (what should NOT happen)

### Step 2: Customize, don't rewrite

The templates are starting points. Add task-specific details under each
heading; don't restructure. The structure is the part that prevents
failure modes.

### Step 3: For long tasks, restate after every break

LLM context degrades. After a 30-message session, the LLM may have
forgotten Regel 2. If you're past 20 turns, paste the relevant prompt
section again before the next non-trivial request.

### Step 4: Inspect the response against the prompt

If the response doesn't follow the template structure, the LLM didn't
internalize it. Push back: "Use the format from the original prompt,
specifically the Konfidenz-Regel section."

## Prompt-section catalog

The templates compose from these reusable blocks:

### Block A — Codebase context

```
You are working on UFT (UnifiedFloppyTool), a Qt6/C/C++ floppy-disk
forensics application. Repository: github.com/Axel051171/UnifiedFloppyTool.

Relevant files for THIS task:
  <list 2-5 specific paths with line ranges if applicable>

Recent changes you should be aware of:
  <commit hashes or PR numbers, optional>

The task is contained to these files. Don't propose changes elsewhere
unless ABSOLUTELY required to fix a bug introduced by your change.
```

### Block B — Constraint declaration

```
This code is dual-target:
  Desktop:  x86-64, GCC ≥11, Qt6, full POSIX
  Firmware: STM32H723ZGT6, Cortex-M7 @ 550MHz, 564 KB SRAM, no Qt

Constraints from this dual-target:
  - Single-precision float only (no double in hot paths)
  - No malloc in hot paths (static buffers)
  - No x86 intrinsics without #ifdef UFT_HOST_X86 fallback
  - LUTs ≤ 16 KB (I-cache size)
  - No size_t in struct layouts (use uint32_t)

Run:
  bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh
before claiming the change is firmware-safe.
```

### Block C — Konfidenz-Regel (from AI_COLLABORATION.md)

```
Confidence rule (mandatory):

When reporting a speedup, NEVER use single numbers.
- BAD:  "2× faster"
- GOOD: "1.7-2.4× faster on a 500k-transition DD track, GCC 13.2 -O3"

Required for any performance claim:
  - Range (min-max), not point estimate
  - Workload description ("DD track", "weak-bit fixture", etc.)
  - Variance (min/median/max over ≥11 runs)
  - Build flags + compiler version
  - CPU frequency governor

Without all five, the number isn't publishable. Don't put a number in
your response without these.
```

### Block D — Honesty rule (no phantom features)

```
Honesty rule (from docs/DESIGN_PRINCIPLES.md Principle 7):

If a function isn't fully implemented, return UFT_ERROR_NOT_IMPLEMENTED
explicitly. Never:
  - Return UFT_OK from an empty body
  - Output zero-padded data when reads fail (return error instead)
  - Set capability flag = true when capability is stubbed
  - Round resolution silently when hardware can't deliver requested precision

The forensic-integrity agent reviews for these. Better to fail loud
than succeed quietly with wrong data.
```

### Block E — Done criteria

```
This task is DONE when:
  - [ ] Compiles with -Wall -Wextra -Werror
  - [ ] Unit test added/updated and passes
  - [ ] No new linker warnings
  - [ ] Firmware portability check passes
  - [ ] Existing tests still pass (regression)
  - [ ] Diff is minimal (only changes required for the task)
  - [ ] No TODO/XXX/FIXME added without GitHub issue link

If you can't tick all boxes, say so explicitly. Don't claim done
prematurely.
```

### Block F — Anti-goals

```
Anti-goals — these are NOT in scope:
  - <list 2-5 concrete things>

If the right fix requires touching anti-goals, STOP and ask before
proceeding. Don't refactor your way out of the task scope.
```

### Block G — Output format (Findings)

```
Output format (mandatory):

## TL;DR
2 sentences max. No preamble. Direct conclusion.

## Findings (priority order)

### 1. [Title] — [LOC] — [Impact range]
**Location:** src/path/file.c:123-145
**Current code:**
```c
// existing
```
**Problem:** specific behavior described
**Suggested change:**
```c
// proposed
```
**Confidence:** ranged ("3-5× faster on DD workload, X-O3")

### 2. ...

## Not checked
- <list of things you didn't verify>

## Recommended order
1. <highest impact / lowest risk>
2. ...
```

## Verification

After receiving an LLM response, check:

```
1. Does TL;DR exist and is it ≤2 sentences?
2. Are confidence claims ranged with workload descriptors?
3. Is "Not checked" present and non-empty?
4. Are file:line references concrete (not "somewhere in flux/")?
5. Is anti-goal list respected (no creep into excluded scope)?
6. If task involved firmware: was check_firmware_portability.sh cited
   as having been (mentally) run?
```

If any answer is "no", paste it back and ask for a corrected version
referencing the original prompt.

## Common pitfalls

### Treating the LLM as authoritative

LLM output is a draft. Always verify file:line references actually
exist, that suggested code compiles, that performance claims are
reproducible. The structured prompts make verification *possible* —
they don't make it *automatic*.

### Pasting the entire AI_COLLABORATION.md

700 lines of context on every prompt = LLM picks the wrong rules. Use
the section blocks (A-G) above. Each one is ≤20 lines and covers one
concern.

### Asking the LLM to "improve" something

"Improve uft_flux_decoder.c" is the worst possible prompt. The LLM has
no constraint, no done criteria, no anti-goals → produces a 200-line
refactor of working code. Always state: WHAT to improve, WHY, BY HOW
MUCH, WITHIN WHAT LIMITS.

### Ignoring "Not checked"

If the LLM lists 5 things under "Not checked", those things are
genuinely not checked. Don't merge based on the green parts. Either
verify the unchecked items yourself or ask the LLM (or another LLM) to
check them.

### Re-using prompts without updating

Templates have placeholders for a reason. A prompt that says "review
src/flux/uft_flux_decoder.c" but you actually wanted ml_protection.c
reviewed → the LLM reads what's specified, not what you intended.

## Related

- `docs/AI_COLLABORATION.md` — the source document these templates encode
- `docs/DESIGN_PRINCIPLES.md` — Principle 7 referenced in honesty block
- `.claude/skills/uft-stm32-portability/` — script cited in firmware block
- `.claude/skills/uft-debug-session/` — bug repro template extends this
- `.claude/agents/structured-reviewer.md` — agent that uses these prompts
- `CLAUDE.md` — auto-loaded context for Claude Code, complements these
