# UFT Skills Package

13 skills + authoring guide for Claude Code working on UnifiedFloppyTool.

## What's in here

```
skills/
├── docs/
│   └── SKILL_AUTHORING_GUIDE.md     # Conventions for writing skills
│
├── uft-format-plugin/               # Add a new disk-image format
├── uft-format-converter/            # Add a conversion path between formats (NEW)
├── uft-hal-backend/                 # Add a hardware-controller backend
├── uft-flux-fixtures/               # Generate synthetic test data
├── uft-benchmark/                   # Measure decoder/PLL/CRC hotpaths
├── uft-crc-engine/                  # CRC16/FM/MFM/Amiga/Apple variants
├── uft-protection-scheme/           # Add copy-protection detector
├── uft-qt-widget/                   # Add Qt6 widget/tab/dialog
├── uft-stm32-portability/           # Firmware-safe code check
├── uft-filesystem/                  # Filesystem layer (NEW)
├── uft-deepread-module/             # DeepRead/OTDR pipeline module (NEW)
├── uft-release/                     # Version bump + release workflow (NEW)
└── uft-debug-session/               # Bug repro → isolate → fix → regression (NEW)
```

## Installation

From your UFT repository root:

```bash
# Extract the zip here and move the skills directory into .claude/
mkdir -p .claude
cp -r skills .claude/
# Result: .claude/skills/uft-*
```

Verify:

```bash
ls .claude/skills/
# expect 13 directories plus docs/
```

Claude Code will auto-load the appropriate skill when a task matches its
triggers.

## Which skill for which task?

| When you want to... | Use this skill |
|---------------------|----------------|
| Add `.roland`, `.jv1`, or other new format | `uft-format-plugin` |
| Build D64→G64, SCP→ADF converter | `uft-format-converter` |
| Implement SCP-Direct or XUM1541 HAL | `uft-hal-backend` |
| Generate test fixtures for unit tests | `uft-flux-fixtures` |
| Measure "is this 2× faster?" | `uft-benchmark` |
| Add CRC for a new format | `uft-crc-engine` |
| Add Rob Northen or Speedlock detector | `uft-protection-scheme` |
| Add analysis panel to GUI | `uft-qt-widget` |
| Check firmware-safety of flux/decoder code | `uft-stm32-portability` |
| Add AmigaDOS or FAT12 file listing | `uft-filesystem` |
| Add weighted voting to DeepRead | `uft-deepread-module` |
| Prepare v4.2.0 release | `uft-release` |
| Reproduce + fix a bug report | `uft-debug-session` |

## Quick test after install

Check that the scaffold scripts are reachable:

```bash
python3 .claude/skills/uft-format-plugin/scripts/scaffold_plugin.py --help
python3 .claude/skills/uft-format-converter/scripts/scaffold_converter.py --help
python3 .claude/skills/uft-crc-engine/scripts/reference/crc_reference.py \
    --family ibm-3740 --input 313233343536373839
# expect: 0x29B1
```

Run the firmware portability check on your codebase:

```bash
bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh
```

Generate all test fixtures:

```bash
for gen in .claude/skills/uft-flux-fixtures/scripts/generators/gen_*.py; do
    python3 "$gen"
done
# output: tests/vectors/{scp,hfe,kryoflux,adf,d64}/*
```

## Authoring new skills

Read `docs/SKILL_AUTHORING_GUIDE.md` before writing a new skill.
Conventions:

- YAML frontmatter needs three-part description: Use when / Trigger
  phrases / DO NOT use for
- Scripts must be idempotent + support `--dry-run`
- Templates must be complete (no `TODO` at the core)
- Verification sections = executable commands, not passive checklists
- Cross-reference related skills in `## Related`

## Differences from the first draft

What changed vs. the initial 8-skill set:

1. **All skills got anti-triggers** — the "DO NOT use for" section that
   redirects mis-triggered invocations.
2. **Scaffold scripts for every skill** that has mechanical steps.
   Previously only SKILL.md existed.
3. **Verification sections are copy-pasteable commands** instead of
   passive checklists.
4. **Cross-references** — skills link to each other via `## Related`,
   so "plugin + CRC + fixtures" surfaces as a coherent workflow.
5. **`uft-qt-widget` stayed unified** (not split into tab/panel) because
   the templates overlap >60% and splitting would duplicate them.
6. **Flux fixtures are fully working**, not TODO-stubs. SCP generator
   produces deterministic, parseable output. Same for HFE, KryoFlux,
   ADF, D64.
7. **5 new skills added** for gaps in the original set:
   - `uft-format-converter` (hoch-frequent workflow)
   - `uft-filesystem` (separate layer from plugins)
   - `uft-deepread-module` (pipeline architecture)
   - `uft-release` (version bumping across 6 files)
   - `uft-debug-session` (repro → isolate → fix → regression)
