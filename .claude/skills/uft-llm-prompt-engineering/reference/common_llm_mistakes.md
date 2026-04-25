# Common LLM Mistakes on UFT — Reference

A catalog of LLM failure modes I've observed when working on UFT, with
concrete examples and how to prevent them via prompt structure.

## 1. The "stale advice" failure

**Pattern:** LLM reviews something and recommends doing X, when X is
already done.

**Real example (Flatpak manifest review):**
> Reviewer: "For USB access you'll want `--device=all` in the manifest"
>
> Reality: `--device=all` was already in the manifest. Reviewer didn't
> read it.

**Why it happens:**
- LLM ran a generic "Flatpak manifest review checklist" without reading
  the actual file
- Or: LLM read the file but pattern-matched to "common Flatpak issues"
  without checking which apply

**Prevention:**
- Paste the actual current state into the prompt, not just file paths
- Ask: "What concrete things in THIS specific file need changing?"
- Reject responses that read like generic checklists

**Detection in response:**
- Recommendations without file:line references
- Phrasing "you should consider" instead of "line N currently does X,
  change to Y"
- No acknowledgment of what the file currently does

---

## 2. The "vague speedup claim" failure

**Pattern:** "This will be 2-3× faster" with no measurement, no
workload, no variance.

**Real example:**
> LLM: "Replacing the byte-by-byte loop with a 256-entry LUT would be
> 5× faster."
>
> Actual measurement: 1.4× on DD workload, 0.8× (regression!) on HD
> workload due to cache pressure.

**Why it happens:**
- LLM extrapolates from textbook claims about LUT vs computed
- No mention of measurement context

**Prevention:**
- Konfidenz-Regel block in every perf prompt
- Demand: range, workload, build flags, variance
- Reject point estimates

**Phrases that signal failure:**
- "approximately N×"
- "much faster"
- "significant speedup"
- single number without min/max

---

## 3. The "200-line refactor for 3-line bug" failure

**Pattern:** Asked to fix a bug. Returns a refactor that fixes the bug
but also reorganizes 200 unrelated lines.

**Real example:**
> Task: "Fix off-by-one in flux_find_sync at line 142"
>
> LLM response: 200-line patch reorganizing the entire sync detection
> logic, "while we're at it I noticed the function could be cleaner"

**Why it happens:**
- LLM incentivized to be "helpful" → adds value beyond strict scope
- No explicit anti-goal stating "fix only the bug"

**Prevention:**
- Anti-goals block listing "no refactoring", "minimal diff"
- Require: "Diff is minimal (only changes required for the task)"
- Reject responses where >50% of changed lines are unrelated

**Detection:**
- Diff size disproportionate to bug size
- Multiple "while I was here" sentences
- Renamed functions or reorganized structure

---

## 4. The "phantom feature" failure

**Pattern:** LLM claims to have implemented function X, but body is
empty or returns success without doing anything.

**Real example:**
```c
// LLM-generated
uft_error_t uft_ml_decoder_train(uft_ml_decoder_t *m, ...) {
    /* TODO: implement training */
    return UFT_OK;  // claims success!
}
```

Caller code now thinks the model is trained → calls predict() → garbage
results that look authoritative.

**Why it happens:**
- LLM generated a plausible-looking stub to "complete" the request
- No honesty rule cited in prompt

**Prevention:**
- Honesty rule block in skeleton-implementation prompts
- Require explicit `UFT_ERROR_NOT_IMPLEMENTED` for anything not actually
  built
- Test must actually exercise the function — empty bodies fail

**Detection:**
- Empty function body returning UFT_OK
- Capability flag set true with stubbed implementation
- TODOs without GitHub issue links

---

## 5. The "context window collapse" failure

**Pattern:** After 30+ messages, LLM forgets the project rules and
makes basic mistakes (uses double, calls malloc in hot path, etc.).

**Real example:**
> Message 5: "Single-precision float only, no double" (acknowledged)
> Message 35: LLM writes `double x = sin(angle); ...`

**Why it happens:**
- Long conversations push project rules out of attention
- LLM weights recent context heavier than older system messages

**Prevention:**
- After every 10-15 turns, restate constraints
- For critical rules, paste the exact AI_COLLABORATION.md section text
- Use multiple shorter sessions instead of one long one

**Detection:**
- Rule violations that LLM previously acknowledged
- Generic code that ignores project conventions
- Reverting to "common practice" rather than UFT-specific patterns

---

## 6. The "false expertise" failure

**Pattern:** LLM confidently asserts technical claims that are wrong
because they sound right.

**Real example:**
> LLM: "AVX-512 will be 16× faster than scalar for this loop because
> it processes 16 floats per instruction."
>
> Reality: The loop is memory-bound. AVX-512 changes nothing because
> RAM is the bottleneck. Actual measured speedup: 1.05×.

**Why it happens:**
- LLM combines facts ("AVX-512 has 16-wide vectors") into a confident
  claim that ignores other factors (memory bandwidth)
- No "Not checked" requirement → all claims appear equally certain

**Prevention:**
- Mandatory "Not checked" section in output
- Require LLM to list assumptions explicitly
- Push back on superlatives ("massively", "dramatically", "huge")

**Detection:**
- Confident claims about performance without measurement
- Claims about hardware behavior without acknowledging it varies
- No "Not checked" listing what wasn't verified

---

## 7. The "documentation drift" failure

**Pattern:** Code change is correct but documentation isn't updated, or
documentation update doesn't match the code change.

**Real example:**
> PR: changes UFT_FORMAT_PLUGIN signature from 4 args to 5
> Code: updated correctly
> docs/PLUGIN_GUIDE.md: still shows the 4-arg signature
> docs/EXAMPLES.md: shows 5-arg version with a different param name
> Result: confusion for next contributor

**Why it happens:**
- LLM focuses on code change, treats docs as afterthought
- No "documentation update required" in done criteria

**Prevention:**
- Done criteria block must include "docs match code"
- Specifically list which docs files cover the touched code
- Verify by grep for the old signature after the change

---

## How to detect any of these in real-time

After receiving an LLM response, ask yourself:

1. **Stale advice?** — Does the response reference current state, or
   does it read like a generic checklist?
2. **Vague claims?** — Are performance numbers ranged with workload?
3. **Refactor creep?** — Is the diff scope proportional to the task?
4. **Phantom features?** — Does every "implemented" function have a
   non-trivial body and a test?
5. **Rule drift?** — Does this session's response contradict an
   earlier session's acknowledgments?
6. **False expertise?** — Are confident claims supported by reasoning,
   or just assertion?
7. **Doc drift?** — Are documentation changes paired with code changes?

If any answer is "yes/uncertain", push back before accepting.

## What this is NOT

This catalog isn't blame — these are systemic failure modes of LLMs
working on dense codebases. The point is to design prompts that make
each failure mode hard, and to recognize them when reviewing output.

The structured prompts in `templates/` are designed to make at least
4 of these 7 failure modes immediately visible.
