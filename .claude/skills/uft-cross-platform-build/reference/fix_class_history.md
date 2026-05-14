# Build Fix-Class History

Narrative log of how each `FIX-NNN` was discovered. Read this before
adding a new class — you may rediscover an existing one.

## FIX-001 — macOS arch mismatch

**Symptom:** macOS CI failed during Qt installation:
```
ERROR: Could not determine architecture for macos
```

**Root cause:** `jurplel/install-qt-action` was passed `arch: clang_64`,
which is the x86_64 architecture identifier. The macos-14 runners are
ARM64 by default — Qt 6.10's `clang_64` package no longer matches.

**Fix:** Drop the `arch:` line entirely; the action picks the right
default for macos-14.

**Where the comment lives:** `.github/workflows/ci.yml` near
`build-macos:`.

---

## FIX-002 — MinGW tools not on PATH

**Symptom:** Windows CI build failed at:
```
'mingw32-make' is not recognized as an internal or external command
```

**Root cause:** Older versions of `install-qt-action` required setting
`IQTA_TOOLS` env var manually for the toolchain to land on PATH.

**Fix:** Use `add-tools-to-path: true` in the install-qt-action step.
Modern versions handle PATH automatically.

---

## FIX-003 — MSVC C89 mode

**Symptom:** `.c` files (not `.cpp`) failed to compile under MSVC with
errors like `C2059 syntax error: 'for'` or `C2143 missing ';' before
'type'`.

**Root cause:** MSVC's default for `.c` files is C89/C90, which
disallows declarations after statements, mixed declarations and code,
inline variable definitions in `for(int i=...)`, etc. Every modern UFT
`.c` file uses C99/C11 idioms.

**Fix:** Add to `.pro`:
```pro
win32-msvc* {
    QMAKE_CFLAGS += /std:c11
}
```

(Note: only affects `.c` files. `.cpp` is fine because MSVC defaults to
modern C++.)

---

## FIX-004 — `__attribute__((unused))` on MSVC

**Symptom:** MSVC errors on lookup tables marked
`__attribute__((unused))`:
```
C2061: syntax error: identifier '__attribute__'
```

**Root cause:** GCC extension; not part of the C standard.

**Fix:** Wrap per occurrence:
```c
static const uint8_t TABLE[64]
#ifndef _MSC_VER
    __attribute__((unused))
#endif
= { ... };
```

Reference site: `src/algorithms/advanced/uft_gcr_viterbi_v2.c` (lines
40-46).

---

## FIX-005 — Missing `strcasecmp`

**Symptom:** MSVC link error:
```
unresolved external symbol _strcasecmp
```

**Root cause:** `strcasecmp` is POSIX, not standard C. Windows has
`_stricmp` instead.

**Fix:** In `.pro`'s `msvc {}` block:
```pro
DEFINES += strcasecmp=_stricmp strncasecmp=_strnicmp
```

This is preprocessor-substitution at compile time; cleaner than
adding wrapper functions.

---

## FIX-006 — Basename collision

**Symptom:** NMAKE/MSVC build failed because two source files with the
same basename existed in different dirs (e.g.
`src/core/uft_format_registry.c` and
`src/formats/uft_format_registry.c`). The flat object output dir
overwrote the first object with the second, leaving symbols from the
overwritten file unlinked.

**Root cause:** Default qmake/NMAKE flat object output. GCC/MinGW had
the same issue but less catastrophically.

**Fix:** `CONFIG += object_parallel_to_source` mirrors the source
directory structure into the object output, eliminating collisions.

This is set unconditionally in `.pro` line 54 — DO NOT remove.

---

## FIX-007 — `cmake --target all`

**Symptom:** CMake build step on Windows runner failed:
```
Error: Unknown target 'all'
```

**Root cause:** Older CMake on the runner generated `MSBuild.exe`-style
solution files where `all` is not a valid target name (it's `ALL_BUILD`
in MSBuild).

**Fix:** Drop `--target all`; let CMake build the default target.
```yaml
cmake --build build-tests --parallel $(nproc)
```

---

## FIX-008 — Qt vs C++ standard mismatch

**Symptom:** Build with Qt 6.7.x and `CONFIG+=cxx20` errored on
Qt-internal C++20-only constructs that 6.7 didn't have.

**Root cause:** Qt 6.7.3 ships C++17 internals; passing `cxx20` to a
project linking against it caused header mismatches. Qt 6.10+ requires
C++20 for its own headers.

**Fix:** Match per matrix entry:
```yaml
- qt_version: '6.7.3'
  cxx_std: 'c++17'
  qmake_extra: ''
- qt_version: '6.10.1'
  cxx_std: 'c++20'
  qmake_extra: 'CONFIG+=cxx20'
```

---

## FIX-009 — Windows linker missing libs

**Symptom:** Windows MinGW link errors:
```
undefined reference to `SetupDiGetClassDevsA'
undefined reference to `PathFileExistsA'
```

**Root cause:** SetupAPI, Shell APIs, and Path APIs are in separate
Win32 system libs that aren't pulled in by default.

**Fix:** In `.pro`:
```pro
win32 {
    LIBS += -lshlwapi -lshell32 -ladvapi32 -lws2_32 -lsetupapi
}
```

---

## FIX-010 — `_FORTIFY_SOURCE` clash

**Symptom:** Distro builds (especially Fedora/Ubuntu 24.04+) emitted:
```
warning: "_FORTIFY_SOURCE" redefined
```

**Root cause:** Modern distros inject `-Wp,-D_FORTIFY_SOURCE=N` via
their build-flags packaging. UFT's `.pro` was setting `=2` globally,
which downgraded distros that ship `=3`.

**Fix:** Don't set `_FORTIFY_SOURCE` globally. Provide an opt-in flag
for bare-metal developer builds:
```pro
uft_force_fortify {
    QMAKE_CFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
}
```

The full rationale lives in `.pro` lines ~93-125. Read it before
changing.

---

## FIX-011 — `mkdir` signature

**Symptom:** Windows compile errors:
```
error: too few arguments to function 'mkdir'
```

**Root cause:** POSIX `mkdir` takes 2 args (path + mode), Windows
`_mkdir` takes 1.

**Fix:** In `globals.h`:
```c
#if defined(_WIN32) || defined(__WIN32__)
#include <direct.h>
#define mkdir(A, B) _mkdir(A)
#endif
```

---

## FIX-012 — Optional Qt SerialPort

**Symptom:** Builds on minimal Qt installs (some distros, some Docker
images) errored:
```
Project ERROR: Unknown module(s) in QT: serialport
```

**Root cause:** `QtSerialPort` is a separate module not always
installed.

**Fix:**
```pro
qtHaveModule(serialport) {
    QT += serialport
    DEFINES += UFT_HAS_SERIALPORT
}
```
Then guard usage in source:
```c
#ifdef UFT_HAS_SERIALPORT
    /* serial-port code */
#endif
```

---

## FIX-013 — Duplicate SOURCES

**Symptom:** Either slow builds (same source compiled N times) or
linker errors about multiple definitions.

**Root cause:** Multiple `SOURCES +=` blocks listed the same file. The
OTDR bridge files were once 99x duplicated due to a copy-paste error
across block boundaries.

**Fix:** At end of `.pro`:
```pro
SOURCES = $$unique(SOURCES)
HEADERS = $$unique(HEADERS)
```

---

## FIX-014 — Stale build artifacts

**Symptom:** After renaming/moving a source file, the build fails with
`No rule to make target` referencing the old name.

**Root cause:** qmake-generated Makefile caches dependency edges. The
old object file is still referenced even after the source moved.

**Fix:** Clean aggressively:
```bash
git clean -fdx build/
```

Note: pass `build/` explicitly. `git clean -fdx` from repo root nukes
user-local files too.

---

## FIX-015 — Source/Build-system parity drift

**Symptom:** A new `.c` file links cleanly via CMake (which auto-globs)
but `.pro`-built apps fail at link time with:
```
undefined reference to `uft_<n>_plugin_register'
```

**Root cause:** Adding a file to the tree without updating `.pro`'s
`SOURCES +=`. CMake's `file(GLOB_RECURSE)` picks it up; qmake doesn't.

**Fix:** Run `scripts/check_build_parity.sh` from this skill before
pushing.

---

## FIX-016 — Debian: qmake6 vs qmake

**Symptom:** Build inside a Debian container fails:
```
/usr/bin/qmake: not found
```

even though `apt install qt6-base-dev` succeeded.

**Root cause:** Debian (and derivatives like Mint, MX) install Qt6's
qmake as `qmake6` and reserve the bare `qmake` name for legacy Qt5.
Ubuntu has historically symlinked `qmake → qmake6` more aggressively.
Build scripts that assume `qmake` will fail on Debian.

**Fix:** Two options, in order of preference:

```bash
# Option A: invoke qmake6 explicitly (preferred for CI / scripts)
qmake6 ../UnifiedFloppyTool.pro CONFIG+=release

# Option B: register alternatives (developer machines only)
sudo update-alternatives --install /usr/bin/qmake qmake /usr/bin/qmake6 60
```

Don't rely on Option B in CI — different runners get different
alternatives priority.

---

## FIX-017 — Debian: Qt version too old

**Symptom:** Compile errors like:
```
error: 'asKeyValueRange' is not a member of 'QHash'
error: 'QStringTokenizer' was not declared in this scope
```

on Debian builds, while Ubuntu and Windows pass cleanly.

**Root cause:** Debian stable (bookworm / 12) ships Qt 6.4.x at the
time of writing. Several APIs UFT uses were added in 6.5 or 6.6.
Ubuntu 22.04 with the jurplel install-action gets Qt 6.7+; Debian's
apt repo stays on what bookworm shipped.

**Fix:** Guard the call site with a Qt-version check and provide a
fallback:

```c
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    for (auto [key, value] : map.asKeyValueRange()) { /* fast path */ }
#else
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        const auto& key   = it.key();
        const auto& value = it.value();
        /* fallback path */
    }
#endif
```

Don't bump the Debian build's required Qt version to "fix" this — the
whole point of the Debian config is to track what apt actually ships.
Adding a backports requirement defeats it.

---

## How to add a new FIX-NNN

When you encounter a build failure that doesn't match any existing FIX:

1. **Bump the number.** Look at the highest existing `FIX-NNN`, add one.
2. **Document here.** Add a section using the template above (Symptom,
   Root cause, Fix, where the patch lives).
3. **Update SKILL.md.** Add a row to the fix-class table.
4. **Update classifier.** Add a `match_rule` block to
   `scripts/classify_build_error.sh` so future occurrences self-classify.
5. **Annotate the patch.** Add a `# FIX-NNN: <symptom>` comment at the
   patch site so `grep -rE 'FIX-[0-9]{3}'` finds it.

If you skip step 5, the patch sits in the codebase but is invisible to
anyone trying to understand why the workaround exists.
