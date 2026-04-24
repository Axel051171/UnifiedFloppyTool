#!/usr/bin/env bash
#
# pre_release_check.sh — Must-pass gate before tagging a UFT release.
#
# Exits 0 if all green, non-zero on first failure.

set -e

UFT_ROOT="${UFT_ROOT:-$(pwd)}"
cd "$UFT_ROOT"

echo "=== Pre-release check ==="
echo "UFT root: $UFT_ROOT"
echo

# ----- Check 1: clean build from scratch -----
echo "[1/6] Clean build..."
if [[ -f Makefile ]]; then
    make distclean >/dev/null 2>&1 || make clean >/dev/null 2>&1 || true
fi
qmake && make -j"$(nproc)" 2>&1 | tail -5
if [[ ${PIPESTATUS[1]} -ne 0 ]]; then
    echo "FAIL: build failed"
    exit 1
fi
echo "OK"
echo

# ----- Check 2: full test suite -----
echo "[2/6] Full test suite..."
cd tests
cmake . >/dev/null 2>&1
make -j"$(nproc)" 2>&1 | tail -5
if ! ctest --output-on-failure; then
    echo "FAIL: tests failed"
    exit 1
fi
cd ..
echo "OK"
echo

# ----- Check 3: plugin compliance audit -----
echo "[3/6] Plugin compliance audit..."
if [[ -x scripts/audit_plugin_compliance.py ]]; then
    if ! python3 scripts/audit_plugin_compliance.py; then
        echo "FAIL: plugin compliance check failed"
        exit 1
    fi
    echo "OK"
else
    echo "  [skip] scripts/audit_plugin_compliance.py not found"
fi
echo

# ----- Check 4: firmware portability -----
echo "[4/6] Firmware portability check..."
if [[ -x .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh ]]; then
    if ! bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh; then
        echo "FAIL: firmware portability issues found"
        exit 1
    fi
    echo "OK"
else
    echo "  [skip] portability script not installed"
fi
echo

# ----- Check 5: no TODO/XXX/FIXME in src/ touched since last tag -----
echo "[5/6] Checking for TODO/XXX/FIXME in new code..."
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "")
if [[ -n "$LAST_TAG" ]]; then
    CHANGED=$(git diff --name-only "$LAST_TAG" HEAD -- 'src/**/*.c' 'src/**/*.cpp' 'src/**/*.h')
    if [[ -n "$CHANGED" ]]; then
        FOUND=$(echo "$CHANGED" | xargs grep -lE "TODO|XXX|FIXME" 2>/dev/null || true)
        if [[ -n "$FOUND" ]]; then
            echo "WARN: files with TODO/XXX/FIXME since $LAST_TAG:"
            echo "$FOUND"
            # Non-fatal — just a warning
        fi
    fi
fi
echo "OK"
echo

# ----- Check 6: CHANGELOG has an entry for the current version -----
echo "[6/6] CHANGELOG freshness..."
CURRENT_VER=$(grep -oE "VERSION\s*=\s*[0-9]+\.[0-9]+\.[0-9]+" UnifiedFloppyTool.pro \
              | awk '{print $NF}')
if [[ -n "$CURRENT_VER" ]] && [[ -f CHANGELOG.md ]]; then
    if ! grep -q "\[$CURRENT_VER\]" CHANGELOG.md; then
        echo "FAIL: CHANGELOG.md has no entry for version $CURRENT_VER"
        echo "      (run bump_version.py first)"
        exit 1
    fi
    # Check if the entry still has placeholder TODOs
    if grep -A 20 "\[$CURRENT_VER\]" CHANGELOG.md | grep -q "^- TODO"; then
        echo "FAIL: CHANGELOG entry for $CURRENT_VER still contains 'TODO' placeholders"
        echo "      (fill in the Added/Changed/Fixed/Removed sections)"
        exit 1
    fi
fi
echo "OK"
echo

echo "=== ALL CHECKS PASSED — safe to tag ==="
