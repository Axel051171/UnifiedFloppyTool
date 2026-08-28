# Contributing to UnifiedFloppyTool

Thank you for your interest in contributing to UnifiedFloppyTool!

## Build Prerequisites

| Dependency | Minimum Version | Notes |
|------------|----------------|-------|
| Qt         | 6.5+           | Qt 6.7.3 used in CI; 6.10+ works locally |
| Compiler   | GCC 11+ / Clang 14+ / MSVC 2022 | MinGW 13.x on Windows also works |
| Build tool | qmake (from Qt) | CMake is only used for the test suite |
| Platform   | Linux, macOS, Windows | All three are CI-tested |

## Building

```bash
# Clone
git clone https://github.com/YOUR_USERNAME/UnifiedFloppyTool.git
cd UnifiedFloppyTool

# Build (Linux/macOS)
qmake UnifiedFloppyTool.pro
make -j$(nproc)

# Build (Windows with MinGW)
qmake UnifiedFloppyTool.pro
mingw32-make -j%NUMBER_OF_PROCESSORS%

# Build (Windows with MSVC via Qt Creator or Developer Command Prompt)
qmake UnifiedFloppyTool.pro
nmake
```

## Running Tests

```bash
mkdir build-tests && cd build-tests
cmake .. -DUFT_BUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

## Critical Warnings

**Read these before modifying the build or adding files:**

1. **`CONFIG += object_parallel_to_source` is MANDATORY** in the .pro file.
   Without it, MSVC/NMAKE inference rules silently compile the wrong source
   file when two `.c` files share the same basename in different directories.
   There are 35+ potential collisions. Never remove this setting.

2. **`src/formats_v2/` is DEAD CODE.** These are unused legacy copies.
   They are NOT in SOURCES. Do not modify, reference, or add them to the build.

3. **C headers with `protected` field** (e.g., `uft_unified_types.h`) cannot
   be included directly from C++ — `protected` is a reserved keyword. Wrap
   with `extern "C"` and only include headers that avoid C++ keywords.

4. **Qt6 QByteArray::append()** no longer accepts bare `char`. Use
   `static_cast<char>(value)` for integer-to-char appends.

## Getting Started

1. Fork the repository
2. Clone your fork
3. Create a feature branch: `git checkout -b feature/my-feature`
4. Make your changes
5. Test on at least one platform (Linux, Windows, or macOS)
6. Commit with clear messages: `git commit -m "Add: new feature description"`
7. Push and create a Pull Request

## Licensing — what header your file carries

**Code written for UFT carries `SPDX-License-Identifier: GPL-2.0-or-later`.**

That is the whole rule. Reasons, so it is not folklore:

- It matches the project's `LICENSE` (GPL v2) and is what MF-580 already
  chose for the one ported file in the tree.
- The `-or-later` half keeps the door open to GPL-3 sources (for example
  `flux-analyze`, `DiskImageTool`) should the project ever move up. A
  `GPL-2.0-only` header would close it permanently.

**Ported or adapted code keeps its origin's licence** and names it. Use
the samdisk pattern: SPDX identifier, upstream project, upstream commit,
and one sentence on what was changed. `src/flux/fdc_bitstream/README.md`
is the worked example.

**A licence that is only a sentence in a comment is not a licence.** Two
cases in this tree show why: `SPDX: MIT` sat on a GPLv2+ port for months
(P0-5, fixed in MF-580), and `uft_dms.c` claimed "Public Domain" with no
source until MF-614 pinned it to Debian's `xdms` copyright file.

**Not permitted without an owner decision:** GPL-3.0, AGPL, Apache-2.0
and BSD-4-Clause in this tree — none of them combine with GPL-2.0.
`tools/uft-scout/playbook/lizenzmatrix.md` is the binding table.

This is checked: `scripts/audit_spdx_policy.py` fails on any SPDX
identifier outside the allowed set. It exists because the one violation
in the tree was found by accident, in a scout side-run, and not by a
gate (MF-620/621).

**What that gate does not do, measured (MF-622):** it catches a *wrong*
identifier, never a *missing* one. Of the 1182 source files under `src/`
and `include/` (vendored trees excluded), **50 carry an SPDX header and
1132 do not — 4.2 %**. The rule above is therefore binding for new and
touched files, and aspirational for the rest.

Adding 1132 headers is a single mass change with real consequences —
it is a decision for the owner, not something to do in passing. Until
then, do not read this section as a description of the tree. Two known
cases sit inside those 1132: `src/formats/d77/uft_d77.c` and
`src/formats/d88/uft_d88.c` carry no header, no `based on`, and no spec
reference at all.

## Code Style

- C++17 features where appropriate
- Qt naming conventions: camelCase for methods, `m_` prefix for members
- Keep functions focused and under 50 lines where possible
- Add comments for complex algorithms

## Commit Message Format

```
Type: Short description

Longer description if needed.

Fixes #123
```

Types: `Add`, `Fix`, `Update`, `Remove`, `Refactor`, `Docs`

## Adding a New Disk Format

1. Create your parser in `src/formats/<platform>/my_format.c`
2. Implement the standard format interface (read, write, detect)
3. Register the format in `src/core/uft_format_registry.c`
4. Add the new `.c` file to `SOURCES` in `UnifiedFloppyTool.pro`
5. Add a detection entry in `src/formats/uft_format_detect_complete.c`
6. Add tests in `tests/`
7. Update the format status matrix if applicable

See `CLAUDE.md` for the full technical reference on architecture, basename
collision details, POSIX compat shims, and CI configuration.

## Reporting Issues

- Use the issue templates
- Include version, OS, and hardware info
- Attach log files if available
- Provide steps to reproduce

## Questions?

Open a Discussion or Issue — we're happy to help!
