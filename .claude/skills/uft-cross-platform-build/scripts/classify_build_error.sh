#!/usr/bin/env bash
# classify_build_error.sh — map a UFT build log to one of the FIX-NNN classes.
#
# Usage:
#   bash classify_build_error.sh <build.log>
#   make 2>&1 | bash classify_build_error.sh -
#
# Exit code: 0 if at least one class matched, 1 if none.

set -u

if [ $# -ne 1 ]; then
    echo "usage: $0 <build.log>   (use - for stdin)" >&2
    exit 2
fi

if [ "$1" = "-" ]; then
    LOG=$(mktemp); cat > "$LOG"
    trap 'rm -f "$LOG"' EXIT
else
    LOG="$1"
    [ -r "$LOG" ] || { echo "$LOG: cannot read" >&2; exit 2; }
fi

MATCHED=0

# Each rule:
#   pattern (egrep)                                              | FIX-ID | message | patch hint
match_rule() {
    local pattern="$1" id="$2" msg="$3" patch="$4"
    if grep -qE "$pattern" "$LOG"; then
        local sample
        sample=$(grep -E -m 1 "$pattern" "$LOG" | head -1)
        echo "[$id] $msg"
        echo "      sample: $sample"
        echo "      patch:  $patch"
        echo
        MATCHED=$((MATCHED + 1))
    fi
}

# ─── FIX-001: macOS arch mismatch ──────────────────────────────────────
match_rule \
    "Could not determine arch.*macos|arch.*clang_64.*not.*found|x86_64.*ARM64" \
    "FIX-001" \
    "macOS arch mismatch — macos-14 runners are ARM64." \
    "remove 'arch: clang_64' from install-qt-action step in ci.yml"

# ─── FIX-002: MinGW tools not on PATH ──────────────────────────────────
match_rule \
    "mingw32-make.*not found|'mingw32-make' is not recognized|IQTA_TOOLS" \
    "FIX-002" \
    "MinGW toolchain not on PATH on Windows runner." \
    "set 'add-tools-to-path: true' in install-qt-action step"

# ─── FIX-003: MSVC C89 mode ────────────────────────────────────────────
match_rule \
    "C2059.*syntax error.*'for'|C2143.*missing ';'.*before.*'type'|C2275.*illegal use of this type as an expression" \
    "FIX-003" \
    "MSVC compiling .c file in C89 mode (default) — code uses C99/C11 syntax." \
    "ensure UnifiedFloppyTool.pro has  win32-msvc*: QMAKE_CFLAGS += /std:c11"

# ─── FIX-004: __attribute__ on MSVC ────────────────────────────────────
match_rule \
    "C2061.*syntax error.*__attribute__|warning C5030.*attribute.*not recognized" \
    "FIX-004" \
    "MSVC doesn't support GCC's __attribute__ syntax." \
    "wrap the attribute in #ifndef _MSC_VER ... #endif (see uft_gcr_viterbi_v2.c for pattern)"

# ─── FIX-005: missing strcasecmp etc. ──────────────────────────────────
match_rule \
    "strcasecmp.*undefined|strncasecmp.*undefined|C3861.*'strcasecmp'" \
    "FIX-005" \
    "MSVC lacks POSIX string functions." \
    "ensure msvc { DEFINES += strcasecmp=_stricmp strncasecmp=_strnicmp } block in .pro"

# ─── FIX-006: basename collision ───────────────────────────────────────
match_rule \
    "object file.*overwrite|multiple definition of.*\.obj|cannot open.*\.obj.*Permission" \
    "FIX-006" \
    "Object-file basename collision (two source files with same name in flat output dir)." \
    "verify 'CONFIG += object_parallel_to_source' present in .pro (already canonical)"

# ─── FIX-007: cmake --target all ───────────────────────────────────────
match_rule \
    "Error.*--target all|Unknown target.*'all'.*cmake" \
    "FIX-007" \
    "Older cmake on Windows runner doesn't accept --target all." \
    "drop '--target all' from cmake invocation in ci.yml"

# ─── FIX-008: Qt 6.7 with C++20 ────────────────────────────────────────
match_rule \
    "concept.*requires C\+\+20|coroutine.*requires C\+\+20|Qt.*requires.*C\+\+17" \
    "FIX-008" \
    "Qt version vs C++ standard mismatch." \
    "Qt 6.7.x → cxx_std c++17, Qt 6.10+ → cxx_std c++20 — match per matrix entry"

# ─── FIX-009: Windows linker missing libs ──────────────────────────────
match_rule \
    "undefined reference to.*Setup(Di|Class)|undefined reference to.*PathFile|cannot find -lsetupapi|unresolved external symbol .*Setup" \
    "FIX-009" \
    "Windows-only API used; linker not pulling in Windows libs." \
    "ensure win32 { LIBS += -lshlwapi -lshell32 -ladvapi32 -lws2_32 -lsetupapi } in .pro"

# ─── FIX-010: _FORTIFY_SOURCE clash ────────────────────────────────────
match_rule \
    "_FORTIFY_SOURCE redefined|_FORTIFY_SOURCE.*requires compiling with optimization" \
    "FIX-010" \
    "_FORTIFY_SOURCE clash — distro injection vs project flags." \
    "do NOT set _FORTIFY_SOURCE globally; opt-in only via 'qmake CONFIG+=uft_force_fortify'"

# ─── FIX-011: mkdir signature ──────────────────────────────────────────
match_rule \
    "too few arguments to function.*mkdir|too many arguments to function.*mkdir" \
    "FIX-011" \
    "mkdir signature mismatch (POSIX 2-arg vs Win32 _mkdir 1-arg)." \
    "include globals.h or add: #if defined(_WIN32) #define mkdir(A,B) _mkdir(A) #endif"

# ─── FIX-012: Qt SerialPort missing ────────────────────────────────────
match_rule \
    "Unknown module.*serialport|QtSerialPort.*not found" \
    "FIX-012" \
    "Qt SerialPort module not installed." \
    "use qtHaveModule(serialport) guard + #ifdef UFT_HAS_SERIALPORT in callers"

# ─── FIX-013: qmake duplicate sources ──────────────────────────────────
match_rule \
    "multiple definition of|duplicate symbol|first defined here" \
    "FIX-013" \
    "Possibly: source file listed multiple times in .pro (qmake doesn't auto-dedup)." \
    "verify 'SOURCES = \$\$unique(SOURCES)' is present at end of .pro"

# ─── FIX-014: stale build artifacts ────────────────────────────────────
match_rule \
    "No rule to make target.*deleted|No such file or directory.*\.o.*needed by" \
    "FIX-014" \
    "Stale qmake-generated Makefile references files that no longer exist." \
    "git clean -fdx build/  &&  re-run qmake"

# ─── FIX-015: parity drift ─────────────────────────────────────────────
match_rule \
    "undefined reference to.*uft_.*_(plugin|probe|init|register)" \
    "FIX-015" \
    "Possibly: source added to .pro but not CMakeLists.txt (or vice versa)." \
    "run scripts/check_build_parity.sh from this skill"

# ─── FIX-016: Debian qmake6 vs qmake ───────────────────────────────────
match_rule \
    "qmake: command not found|/usr/bin/qmake: not found" \
    "FIX-016" \
    "Debian ships qmake6 (Qt6) and qmake (Qt5/legacy) as separate binaries." \
    "use qmake6 explicitly, or run: sudo update-alternatives --config qmake"

# ─── FIX-017: Debian Qt-version-too-old ────────────────────────────────
match_rule \
    "asKeyValueRange.*not a member|no member named.*emplace.*QHash|QStringTokenizer.*not declared" \
    "FIX-017" \
    "Debian stable's Qt 6.4.x lacks features available in Qt 6.6+." \
    "guard with #if QT_VERSION >= QT_VERSION_CHECK(6,6,0) or use a fallback path"

# ─── Summary ───────────────────────────────────────────────────────────

if [ "$MATCHED" -eq 0 ]; then
    echo "No known FIX-class matched. This may be a NEW build-failure class."
    echo
    echo "Steps:"
    echo "  1. Read SKILL.md '## Adding a new platform / Qt version' section"
    echo "  2. Diagnose by hand"
    echo "  3. Once fixed, add a new FIX-NNN row to the table AND a"
    echo "     match_rule entry to this script so future occurrences classify."
    exit 1
fi

echo "Matched $MATCHED known fix-class(es). See SKILL.md table for details."
exit 0
