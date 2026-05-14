# Prompt template: New format plugin

Use this when delegating "add a UFT format plugin for X" to an LLM. Pair
with the `uft-format-plugin` skill — this template constrains the LLM
to follow that skill's 6-touchpoint pattern.

---

## Block A — Codebase context

You are working on UFT (UnifiedFloppyTool), a Qt6/C/C++ floppy-disk
forensics application. Repository: github.com/Axel051171/UnifiedFloppyTool.

The project has 80+ format plugins. The pattern is stable. Read these
before proposing anything:

- `.claude/skills/uft-format-plugin/SKILL.md` — the canonical workflow,
  including the 6 touchpoints and scaffold script
- `src/formats/d64/uft_d64_plugin.c` — reference for fixed-size sector
  formats with no header
- `src/formats/atr/uft_atr_plugin.c` — reference for magic-byte formats
- `include/uft/uft_format_plugin.h` — registry enum

Do NOT modify any of: existing plugins, the registry macro itself,
`src/flux/`, `src/recovery/`. The task is contained to the new plugin
and its registration.

## Block B — The task

Add a plugin for: `<FORMAT_NAME>` (extension `.<EXT>`).

Format facts (fill in before sending):

```
Description:    <one sentence>
Min size:       <bytes>
Max size:       <bytes>
Sector size:    <usually 256 / 512 / 1024>
Magic bytes:    <hex sequence at offset N, or "none">
Writable:       <yes | no>
Spec status:    OFFICIAL_FULL | OFFICIAL_PARTIAL | REVERSE_ENGINEERED
                | DERIVED | UNKNOWN
Closest existing plugin (for reference): <path>
```

If `Spec status = UNKNOWN` you have not done enough research yet —
either find a reference (a8rawconv, X-Copy, MAME, an emulator's source)
or stop and report back.

## Block C — Constraint declaration

This plugin code must be:

- **Firmware-portable.** No malloc on hot paths, no x86 intrinsics
  without `#ifdef UFT_HOST_X86`, single-precision float only. Run
  `bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh`
  before claiming firmware-safe.
- **Forensic-honest.** If a function isn't fully implemented, return
  `UFT_ERROR_NOT_IMPLEMENTED` — never `UFT_OK` from an empty body. Set
  capability flags only when the capability is actually wired.

## Block D — Honesty rule

If the format spec is incomplete or you're unsure about a structural
element, set the corresponding handler to return
`UFT_ERROR_NOT_IMPLEMENTED` and document the gap in a comment. Do not
guess at the layout and ship a plugin that decodes garbage as success.

## Block E — Done criteria

This task is DONE when:

- [ ] The 6 touchpoints from `uft-format-plugin` SKILL.md exist and compile
- [ ] `gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only` is clean on
      the new plugin source
- [ ] The new test (`tests/test_<n>_plugin.c`) has at least:
      one positive test against a fixture, one negative against a
      neighbour format
- [ ] `qmake && make -j$(nproc)` produces no NEW warnings
- [ ] `./uft list-formats | grep -i <name>` shows the plugin
- [ ] `python3 scripts/audit_plugin_compliance.py` has no ERROR for the
      new plugin
- [ ] The diff touches only the 6 touchpoints — no drive-by edits

If you can't tick all boxes, say so explicitly.

## Block F — Anti-goals

- Do NOT modify `src/flux/`, `src/recovery/`, or any existing plugin
- Do NOT add a new dependency (no new third-party libs)
- Do NOT introduce a new CRC variant in this PR — if the format needs
  one not in `uft-crc-engine`, that's a separate PR
- Do NOT speculate about format details — cite a source or stop

If the right fix requires touching anti-goals, STOP and ask before
proceeding.

## Block G — Output format

```
## TL;DR
2 sentences. Plugin name + sectors decoded successfully on N-byte fixture.

## Diff summary
- src/formats/<n>/uft_<n>_plugin.c  (+NEW, X lines)
- include/uft/uft_format_plugin.h  (+1 enum entry)
- src/formats/uft_format_registry.c  (+1 entry)
- UnifiedFloppyTool.pro  (+1 source line)
- tests/test_<n>_plugin.c  (+NEW, X lines)
- tests/CMakeLists.txt  (+1 add_executable, +1 add_test)

## Probe logic
3-5 line description of how probe distinguishes this format from
neighbours. Cite the magic bytes / size / structural cue used.

## Test fixtures used
List of files from tests/vectors/ that the test suite depends on.
If a fixture had to be generated, cite the uft-flux-fixtures generator.

## Not checked
- <e.g. "behavior on >2GB files (no fixture)">
- <e.g. "extended-track variants">
```
