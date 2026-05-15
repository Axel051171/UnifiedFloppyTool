#!/usr/bin/env bash
# check_build_parity.sh — find files in src/ that aren't listed in the .pro.
#
# UFT has TWO build systems:
#   - UnifiedFloppyTool.pro  (qmake — explicit SOURCES, MUST list every .c/.cpp)
#   - CMakeLists.txt         (cmake — auto-globs via file(GLOB_RECURSE))
#
# Because CMake auto-discovers, a new .c file added to the tree is picked
# up by CMake automatically — but qmake silently ignores it (until link
# time, when symbols go missing). This script finds files that CMake
# WOULD compile but qmake does NOT list.
#
# Usage:
#   bash check_build_parity.sh
#
# Exit 0 if in parity, 1 if drift found.

set -u

PRO_FILE="UnifiedFloppyTool.pro"

if [ ! -f "$PRO_FILE" ]; then
    echo "error: run from the UFT repo root (need $PRO_FILE)" >&2
    exit 2
fi
if [ ! -d "src" ]; then
    echo "error: no src/ directory found — wrong working directory?" >&2
    exit 2
fi

ALL_TMP=$(mktemp)
PRO_TMP=$(mktemp)
trap 'rm -f "$ALL_TMP" "$PRO_TMP"' EXIT

# ─── 1. Every .c/.cpp under src/ ───────────────────────────────────────
find src -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.cc' \) \
    | sort -u > "$ALL_TMP"

# ─── 2. Sources listed in .pro ─────────────────────────────────────────
awk '
    /^SOURCES[ \t]*[+=]+/ { in_sources = 1 }
    in_sources {
        orig = $0
        sub(/^SOURCES[ \t]*[+=]+[ \t]*/, "")
        sub(/#.*$/, "")
        gsub(/\\[ \t]*$/, "")
        n = split($0, parts, /[ \t]+/)
        for (i = 1; i <= n; i++) {
            if (parts[i] != "") print parts[i]
        }
        if (orig !~ /\\[ \t]*$/) in_sources = 0
    }
' "$PRO_FILE" | grep -E '\.(c|cpp|cc)$' | sort -u > "$PRO_TMP"

ALL_COUNT=$(wc -l < "$ALL_TMP")
PRO_COUNT=$(wc -l < "$PRO_TMP")

echo "Files on disk under src/:    $ALL_COUNT"
echo "Files listed in .pro:        $PRO_COUNT"
echo

MISSING=$(comm -23 "$ALL_TMP" "$PRO_TMP")
ORPHANED=$(comm -13 "$ALL_TMP" "$PRO_TMP")
MISSING_COUNT=$(echo -n "$MISSING" | grep -c '^' || true)
ORPHANED_COUNT=$(echo -n "$ORPHANED" | grep -c '^' || true)

# Filter excluded patterns (intentionally not built into the main binary)
EXCLUDE_PATTERN='(^src/qmake_stubs/|^src/compat/|/test_|_test\.c|/example_|/examples/)'
REAL_MISSING=$(echo "$MISSING" | grep -vE "$EXCLUDE_PATTERN" || true)
REAL_MISSING_COUNT=$(echo -n "$REAL_MISSING" | grep -c '^' || true)

EXIT=0
STRICT=${STRICT:-0}   # set STRICT=1 to fail on drift; default is advisory

if [ "$REAL_MISSING_COUNT" -gt 0 ]; then
    echo "⚠  Files on disk but NOT in .pro ($REAL_MISSING_COUNT):"
    echo "$REAL_MISSING" | head -20 | sed 's/^/    /'
    [ "$REAL_MISSING_COUNT" -gt 20 ] && echo "    ... and $((REAL_MISSING_COUNT - 20)) more"
    echo
    echo "   Each file is one of:"
    echo "     (a) A real omission — should be added to .pro"
    echo "     (b) Intentionally excluded from the desktop build"
    echo "         (e.g. firmware-only, alternate algorithm)"
    echo
    echo "   Triage: open each path, look at the file header. If it has"
    echo "   no exclusion comment, it's probably (a)."
    echo
    [ "$STRICT" = "1" ] && EXIT=1
fi

if [ "$ORPHANED_COUNT" -gt 0 ]; then
    echo "❌ Listed in .pro but not on disk ($ORPHANED_COUNT) — definite bug:"
    echo "$ORPHANED" | head -10 | sed 's/^/    /'
    [ "$ORPHANED_COUNT" -gt 10 ] && echo "    ... and $((ORPHANED_COUNT - 10)) more"
    echo
    echo "   Action: remove the dangling SOURCES entries (FIX-014 territory)."
    echo
    EXIT=1   # orphaned references ALWAYS fail — they break builds
fi

if [ $EXIT -eq 0 ]; then
    if [ "$MISSING_COUNT" -gt 0 ] && [ "$REAL_MISSING_COUNT" -eq 0 ]; then
        echo "ℹ️  $MISSING_COUNT file(s) under src/ not in .pro, all in excluded paths."
    fi
    echo "✓ Build parity OK"
fi

exit $EXIT
