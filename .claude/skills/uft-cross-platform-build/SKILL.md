---
name: uft-cross-platform-build
description: |
  Use when a UFT build fails on any platform/compiler/Qt-version combination,
  when the CI matrix goes red, or when adding code that touches platform-
  specific APIs. Trigger phrases: "build fehlschlag windows", "msvc c11
  fehler", "ci red", "qmake fehler", "vla msvc", "macos arch fehler",
  "mingw build kaputt", "windows linker error", "qt 6.10 c++20 fehler",
  "ci matrix red", "linux build only", "platform-specific build issue",
  "stale object files", "header conflict windows". Maps every error to
  the FIX-class that resolves it. DO NOT use for: runtime bugs that build
  fine (→ uft-debug-session), packaging/installer issues (separate
  concern), Qt UI errors (→ uft-qt-widget), compiler optimization tuning
  (→ uft-benchmark), STM32-firmware-portability of UFT code
  (→ uft-stm32-portability — different concern, prepares UFT for
  embedded; THIS skill is about desktop/CI matrix).
---

# UFT Cross-Platform Build

Use this skill when a build fails on any of the platforms UFT supports.
The CI matrix is **6 configurations** that must all stay green:

| OS | Qt | C++ | Compiler |
|----|----|----|----------|
| Linux Ubuntu (ubuntu-22.04) | 6.7.3 | C++17 | gcc 11 |
| Linux Ubuntu (ubuntu-22.04) | 6.10.1 | C++20 | gcc 11 |
| Linux Debian (debian:12 container) | 6.4.x (apt) | C++17 | gcc 12 |
| macOS (macos-14, ARM64) | 6.10.1 | C++20 | clang |
| Windows (windows-2022) | 6.10.1 | C++20 | MinGW (gcc 13.1) |
| Windows local (developer) | 6.x | C++17/20 | MSVC |

The Ubuntu entries cover modern Qt + bleeding-edge C++; Debian covers
the slower-moving distro tail (Qt from apt, no jurplel install). macOS
and Windows-MinGW are checked automatically; the MSVC build is **only
checked locally** because Axel sometimes builds with Visual Studio.
Most pain comes from MSVC-specific quirks that the GCC/MinGW pair never
sees, plus Debian-specific hardening flags that Ubuntu masks.

## When to use this skill

- A CI job goes red and you need to map the error to a known fix
- You add code that touches `<unistd.h>`, `<direct.h>`, `<sys/...>`,
  `__attribute__`, `_MSC_VER`, or any platform-specific call
- A build that worked yesterday breaks today after a Qt/toolchain bump
- You need to update CI to a new Qt version
- "Works on my machine" — i.e. your platform passes but another fails
- Reviewing a PR that adds a new `.c` or `.cpp` file (parity check
  with `.pro` AND `CMakeLists.txt`)

**Do NOT use this skill for:**
- Runtime bugs in code that builds fine — use `uft-debug-session`
- Qt-widget compilation errors that are widget-specific — use `uft-qt-widget`
- STM32 firmware-portability of UFT code — use `uft-stm32-portability`
  (different concern: this skill is about desktop CI, that one is
  about preparing the same code for embedded)
- Performance-flag tuning (`-O3`, `-march`) — use `uft-benchmark`
- Packaging installers (DEB/MSI/DMG) — outside scope

## The fix-class table (authoritative)

When you see a build error, find it here first. Each row links to the
canonical fix; don't reinvent. The numbering matches the `FIX-NNN`
markers in `.github/workflows/ci.yml` and `UnifiedFloppyTool.pro`.

| ID | Symptom | Where it bites | Fix pattern |
|----|---------|----------------|-------------|
| **FIX-001** | macOS build fails: arch mismatch | `jurplel/install-qt-action` | Remove `arch: clang_64` — macos-14 is ARM64 by default |
| **FIX-002** | Windows: `mingw32-make: not found` | install-qt-action on Windows | `add-tools-to-path: true` (no manual `IQTA_TOOLS`) |
| **FIX-003** | MSVC: `.c` files fail with C99 syntax | `win32-msvc*` block | `QMAKE_CFLAGS += /std:c11` (MSVC defaults to C89) |
| **FIX-004** | MSVC: `__attribute__((unused))` errors | every static-table file | Wrap in `#ifndef _MSC_VER ... #endif` |
| **FIX-005** | MSVC: `strcasecmp` undefined | string ops | `DEFINES += strcasecmp=_stricmp strncasecmp=_strnicmp` |
| **FIX-006** | MSVC: basename collision (e.g. two `uft_format_registry.c`) | nmake flat object output | `CONFIG += object_parallel_to_source` (already set) |
| **FIX-007** | CMake: `--target all` fails on Windows | CI step | Drop `--target all`, use `cmake --build build-tests --parallel` |
| **FIX-008** | Qt 6.7 build with `CONFIG+=cxx20` errors | std mismatch | Qt 6.7.3 needs C++17, only 6.10+ takes C++20 — match per matrix entry |
| **FIX-009** | Windows: linker missing `setupapi`, `shlwapi` | win32-only APIs | `LIBS += -lshlwapi -lshell32 -ladvapi32 -lws2_32 -lsetupapi` |
| **FIX-010** | Linux distros: stricter `_FORTIFY_SOURCE` clash | distro CFLAGS injection | Don't set `_FORTIFY_SOURCE` globally — opt-in via `CONFIG+=uft_force_fortify` |
| **FIX-011** | `mkdir` signature mismatch on Windows | POSIX vs Win32 | Use `#if defined(_WIN32) #define mkdir(A,B) _mkdir(A)` shim from `globals.h` |
| **FIX-012** | Qt SerialPort missing on minimal install | optional feature | Use `qtHaveModule(serialport)` guard + `#ifdef UFT_HAS_SERIALPORT` |
| **FIX-013** | qmake duplicate SOURCES (build slow / link errors) | grouped section adds | `SOURCES = $$unique(SOURCES)` at end of .pro (fix already in place) |
| **FIX-014** | "Works after `make clean` but stale object earlier" | qmake's incremental build | `git clean -fdx build/` (qmake-generated Makefiles ignore some renames) |
| **FIX-015** | `.c` file added to .pro but not CMakeLists.txt | parity drift | Run `scripts/check_build_parity.sh` (this skill provides one) |
| **FIX-016** | Debian: `qmake6` not found, only `qmake` (Qt5) on PATH | apt naming | Use `qmake6` explicitly OR `update-alternatives --config qmake` |
| **FIX-017** | Debian: Qt 6.4.x lacks `QHash::asKeyValueRange()` | apt Qt is older | Guard with `#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)` or backport via `qAsKeyValueRange()` shim |

If your error doesn't match any row above, you've found a new class —
add it with the next FIX-NNN number, then this skill gets updated.

## Workflow

### Step 1: Capture the failing log

```bash
# Locally:
qmake CONFIG+=release CONFIG+=cxx20 ../UnifiedFloppyTool.pro 2>&1 | tee build.log
make -j$(nproc) 2>&1 | tee -a build.log

# CI: download artifact "linux-build-logs-..." or "windows-build-logs-..."
gh run view --log-failed <run-id> > ci.log
```

Don't try to fix from memory — work from the actual error.

### Step 2: Classify with the parser script

```bash
bash .claude/skills/uft-cross-platform-build/scripts/classify_build_error.sh \
    build.log
```

The script greps for known signatures and emits one of:

```
FIX-003: MSVC C89 mode triggered (saw "C2059 syntax error: 'for'")
        -> see SKILL.md fix-class table, row FIX-003
        -> patch:  win32-msvc*: QMAKE_CFLAGS += /std:c11
```

If it can't classify, the error is novel — proceed to Step 3.

### Step 3: Apply the fix from the table

Each fix in the table has a one-line patch. Apply it where indicated:
- FIX-001/002/007: `.github/workflows/ci.yml`
- FIX-003/004/005/006/008/009/010/012/013: `UnifiedFloppyTool.pro`
- FIX-011: source-side, in `globals.h` or per-file
- FIX-014/015: developer workflow, no code change

**Don't refactor while fixing.** A build fix is its own commit:
"fix(build): FIX-NNN <short description>". If you find adjacent
problems while fixing, file them separately.

### Step 4: Verify on the affected matrix entry

```bash
# Reproduce the failing config locally
bash .claude/skills/uft-cross-platform-build/scripts/run_matrix_local.sh \
    --config <linux-c++17 | linux-c++20 | macos | windows-mingw | windows-msvc>
```

The script orchestrates qmake invocation with the right flags for
each matrix entry. It shells out to MSVC only on Windows; on Linux it
will skip windows/macos entries with a clear "n/a on this host" note.

### Step 5: Update the table if you found a new class

If your fix added a new FIX-NNN row:

1. Bump the highest FIX-NNN number
2. Add a row to the table in this SKILL.md
3. Add a `# FIX-NNN: <symptom>` comment at the patch site so
   `classify_build_error.sh` can match it
4. Add the pattern to `scripts/classify_build_error.sh`'s grep table

This keeps the skill self-extending — every new build pain becomes
documented, classifiable, and one-line-fixable for next time.

## Verification

```bash
# 1. The .pro and CMakeLists.txt agree on sources
bash .claude/skills/uft-cross-platform-build/scripts/check_build_parity.sh
# expect: "0 sources in .pro but not CMakeLists.txt, 0 vice versa"

# 2. No FIX-XXX referenced that isn't in the table
grep -rE 'FIX-[0-9]{3}' .github/workflows/ UnifiedFloppyTool.pro \
    | awk -F'FIX-' '{print "FIX-" substr($2, 1, 3)}' | sort -u
# Compare against table — every emitted ID must appear here.

# 3. Local matrix run (skips entries the host can't run)
bash .claude/skills/uft-cross-platform-build/scripts/run_matrix_local.sh --all
# expect: every runnable entry returns 0

# 4. CI status check
gh run list --workflow=ci.yml --limit 1
# expect: latest run status conclusion=success
```

## Common pitfalls

### Fixing the symptom in source instead of the .pro

Resist the urge to `#ifdef _MSC_VER` an entire function when the real
fix is one line in `UnifiedFloppyTool.pro` (e.g. enabling C11 mode).
Source-level workarounds metastasize — once the .pro is right, dozens
of files don't need guards.

### Dropping `-Wno-stringop-truncation` "to clean up"

The `win32-g++` block deliberately suppresses warnings that GCC's
mingw build emits but standard GCC doesn't. They're suppressed because
the legacy code triggers them harmlessly and there's no clean fix.
Don't remove unless you've audited every site.

### Trusting "works on my machine" after a CI fix

The whole point of the matrix is that platforms diverge. After
applying a fix, push and check ALL 4 matrix entries. A fix that
unbreaks Linux can break MinGW.

### Updating Qt version without updating C++ standard matrix

Qt 6.10+ requires C++20. Qt 6.7.x is C++17. If you bump the version
in CI, the matrix entry's `cxx_std` and `qmake_extra` must move with
it — otherwise the build silently uses the wrong standard.

### Setting `_FORTIFY_SOURCE` globally

Distros inject it via `-Wp,-D_FORTIFY_SOURCE=N` which can't be
reliably overridden via `-U/-D` on the compiler line. The `.pro`
documents this in detail at lines ~93-125; read before changing.
Setting `=2` globally would *downgrade* Fedora/Ubuntu 24.04+ which
ship with `=3`.

### Adding a `.c` file to .pro but forgetting CMakeLists.txt

The CI build-tests step uses CMake. A file added only to qmake builds
on Linux/macOS/Windows-MinGW (the CI uses qmake) but breaks the
CMake-driven test suite. Run `check_build_parity.sh` before pushing.

### Using `git clean -fdx` from the repo root

That nukes more than the build tree. Always pass the build directory
explicitly: `git clean -fdx build/` or `cd build && git clean -fdx .`
The repo root has user-local files (.claude/, ~/-style stuff sometimes)
that shouldn't be touched.

## Adding a new platform / Qt version

When you bump Qt or add a new OS:

1. Add the matrix entry to `.github/workflows/ci.yml`
2. Run all four existing matrix entries first to establish baseline
3. Add the new entry; expect 1-3 FIX-NNN classes to fire
4. Document each new FIX in this SKILL.md table
5. Update the CI banner comment at the top of `ci.yml` referencing the
   new matrix entry

Never add a new entry with `continue-on-error: true` and call it done.
The CMake test step uses that today as a known-issue marker — but for
build matrix entries it's a code smell.

## Related

- `.claude/skills/uft-stm32-portability/` — different concern: prepares
  UFT code for embedded; this skill is about desktop matrix
- `.claude/skills/uft-release/` — release builds use this skill's
  configurations as the gating check
- `.claude/skills/uft-qt-widget/` — Qt-specific compilation issues live
  there
- `.github/workflows/ci.yml` — canonical CI config; FIX-NNN comments
  live in there
- `UnifiedFloppyTool.pro` — primary build system (qmake)
- `CMakeLists.txt` — secondary, for tests + packaging only
- `win64-toolchain.cmake` — MinGW cross-compile from Linux, when needed
- `reference/fix_class_history.md` — narrative log of how each FIX-NNN
  was discovered (read this before adding a new one — you may
  rediscover an older fix)
