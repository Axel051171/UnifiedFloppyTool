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
