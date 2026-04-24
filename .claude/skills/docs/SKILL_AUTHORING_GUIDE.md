# UFT Skill Authoring Guide

Canonical rules for writing a UFT skill. Read this before creating or editing
any skill. Every skill in `.claude/skills/` must follow this structure.

## What a skill is (and isn't)

A skill is a **wisdom + scaffolding package** that Claude loads automatically
when it matches the user's task. It is not:

- Documentation (that's `docs/`)
- A tutorial (that's `docs/USER_MANUAL.md` sections)
- An agent (that's `.claude/agents/`)
- General code advice (that's `CLAUDE.md` and `docs/AI_COLLABORATION.md`)

A skill pays off when a task **recurs 3+ times** and **has mechanical steps**
that scripts can automate. If both conditions don't hold, it's not a skill —
it's a doc.

## Directory structure

Every skill lives at `.claude/skills/<name>/` with at minimum:

```
<skill-name>/
├── SKILL.md              # Trigger + workflow (required)
├── templates/            # Code/config templates (if applicable)
│   └── *.tmpl
├── scripts/              # Executable helpers (if applicable)
│   └── *.py or *.sh
└── reference/            # Pure-text domain knowledge (optional)
    └── *.md
```

At least one of `templates/` or `scripts/` should exist. Skills with only
`SKILL.md` are rarely worth the loading cost.

## SKILL.md frontmatter

```yaml
---
name: uft-<short-name>
description: |
  Use when <exact situation>. Trigger phrases: "...", "...", "..."
  (German), "..." (English). Links to: <related skill or agent>.
  DO NOT use for: <anti-trigger list>.
---
```

### `name` rules

- Always prefix `uft-`
- Lowercase, hyphen-separated
- 2–4 words maximum
- The name IS the filename of the directory

### `description` rules

**Mandatory three-part structure:**

1. **"Use when"** sentence — the positive trigger (one sentence)
2. **"Trigger phrases"** — 5–10 exact phrases, bilingual (German + English)
3. **"DO NOT use for"** — 3–5 explicit anti-triggers that redirect elsewhere

The anti-trigger part is what prevents skill-overload. Without it, skills
get auto-loaded too often.

**Length limit:** 6–8 lines of YAML. If longer, you're putting content in
the description that belongs in SKILL.md body.

### Bad vs. good description

```yaml
# BAD — no anti-triggers, verbose
description: |
  This skill helps you with adding new format plugins to UFT. Format plugins
  are the way UFT supports disk image formats. When a user wants to add
  support for a new format, use this skill. It enforces the template
  pattern from existing plugins, handles registry wiring, and ensures
  MF-007 test coverage. The skill covers probe functions, open/close/read,
  and the registration macro.
```

```yaml
# GOOD — trigger + anti-trigger, compact
description: |
  Use when adding a new disk-image format plugin to UFT. Trigger phrases:
  "füge plugin für X hinzu", "neuer format plugin", "add X plugin",
  "plugin für Y schreiben". Links to uft-crc-engine, uft-flux-fixtures.
  DO NOT use for: modifying existing plugins (→ structured-reviewer),
  conversion between formats (→ uft-format-converter), filesystem-layer
  work (→ uft-filesystem).
```

## SKILL.md body structure

Every SKILL.md follows the same section order:

```markdown
# <Title>

Short intro — 2–4 sentences. What the skill is for. What it enforces.

## When to use this skill

Bullet list of concrete trigger situations. Then explicit "Do NOT use for:"
list pointing to alternative skills/agents.

## Workflow

Numbered steps. Each step has:
- A clear goal ("Write the plugin file")
- A location reference (file path)
- A code snippet OR template reference OR script invocation
- Exit criteria (how to know the step is done)

## Verification

Executable commands — not a passive checklist. User should be able to
copy-paste each line.

## Common pitfalls

Specific failure modes with before/after fixes. Not generic advice.

## Related

- Cross-references to other skills (at least 2, if applicable)
- Reference docs in `docs/`
- Reference agents in `.claude/agents/`
```

## Verification sections must be executable

Replace passive checklists with copy-pasteable commands:

```markdown
# BAD
- [ ] gcc compiles clean
- [ ] Test passes
- [ ] Plugin registered

# GOOD
```bash
# 1. Compile check
gcc -std=c11 -Wall -Wextra -Werror -c src/formats/<n>/uft_<n>.c \
    -I include/ && echo OK

# 2. Run the plugin test
cd tests && cmake . && make test_<n>_plugin && ./test_<n>_plugin

# 3. Confirm registration
./uft list-formats | grep -i <n> || echo "NOT REGISTERED"
```
```

## Templates should be complete

A template with `TODO` or `...` placeholder is a broken template.
Every template must be:

- Compilable/runnable as-is (after placeholder substitution)
- Minimal but not trivial (no `/* your code here */` at the core)
- Documented at the top (first 5 lines explain the template's purpose)

Placeholder format: `@@VAR@@` (matches scaffold script expectations).

## Scripts should be idempotent

Any script that modifies files must be **idempotent** — re-running with the
same arguments should be a no-op. Check before insert:

```python
if target_line in existing_file:
    print(f"  [skip] already present")
    return
```

Scripts should also support `--dry-run` to preview changes without writing.

## Cross-referencing other skills

Every skill should link to related skills in its `## Related` section:

```markdown
## Related

- `.claude/skills/uft-crc-engine/` — CRC computation families
- `.claude/skills/uft-flux-fixtures/` — synthetic test data
- `.claude/agents/structured-reviewer.md` — review the final plugin
- `docs/DESIGN_PRINCIPLES.md` Principle 7 (honesty matrix)
```

This prevents duplication — "what's the CRC for Amiga MFM?" is answered in
one skill, not copy-pasted across five.

## Anti-patterns to avoid

1. **Generic advice** — Skills should be UFT-specific. If a sentence would
   apply to any C project, cut it.
2. **Duplicate cross-cutting rules** — "don't modify `formats_v2/`" belongs
   in CLAUDE.md. Don't repeat it per skill.
3. **Walls of prose** — Every paragraph should either end in code, a command,
   or a checklist. If it's three paragraphs of explanation, that's a doc.
4. **Claim before demonstrate** — Don't say "use the X pattern" without
   showing the pattern in the same section.
5. **Passive verbs** — "should be done" → "do this". Skills are instructions.
6. **Hidden anti-triggers** — If the skill shouldn't apply in case X, say so
   in the description, not buried in the body.

## When to split a skill

Split a skill if:

- It has two distinct workflows (e.g. Qt Tab vs. Qt Analysis Panel)
- The template section has two unrelated templates
- Two sections of anti-patterns contradict each other

Merge when:

- Two skills share >60% of their templates
- Two skills' trigger phrases overlap such that disambiguation is hard

## Maintenance

When UFT adds a new convention (e.g. a new plugin format ID), update the
affected skills in the same PR. Skills that contradict the codebase are
worse than no skill at all.

To check skill coverage:

```bash
ls .claude/skills/*/SKILL.md | wc -l   # count skills
grep -l "TODO" .claude/skills/*/SKILL.md  # find incomplete ones
```

## Cross-skill consistency table

All skills must agree on these canonical references:

| Topic | Canonical location | Never duplicate |
|-------|-------------------|-----------------|
| Plugin registry macro | `include/uft/uft_format_common.h` | skill-internal |
| CRC family cheat-sheet | `uft-crc-engine/SKILL.md` | skill-internal |
| HAL vtable shape | `uft-hal-backend/SKILL.md` | skill-internal |
| Firmware-safe rules | `uft-stm32-portability/SKILL.md` | skill-internal |
| formats_v2 dead | `CLAUDE.md` | everywhere else |
| Principle 7 honesty | `docs/DESIGN_PRINCIPLES.md` | everywhere else |

If content appears in two places, one is wrong.
