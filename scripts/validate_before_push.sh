#!/bin/bash
#═══════════════════════════════════════════════════════════════════════════════
# UFT Pre-Push Validator v2.0 - Prevents CI failures
#═══════════════════════════════════════════════════════════════════════════════

ERRORS=0
WARNINGS=0

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  UFT Pre-Push Validation v2.0"
echo "══════════════════════════════════════════════════════════════"

# 1. Critical files
echo -e "\n[1/7] Critical files..."
for f in "CMakeLists.txt" ".github/workflows/ci.yml" "src/main.cpp"; do
    [ -f "$f" ] && echo "  ✓ $f" || { echo "  ✗ MISSING: $f"; ((ERRORS++)); }
done

# 2. Qt Module Sync
echo -e "\n[2/7] Qt modules..."
if grep -q "SerialPort" CMakeLists.txt 2>/dev/null; then
    grep -q "serialport" .github/workflows/ci.yml 2>/dev/null && echo "  ✓ SerialPort synced" || { echo "  ✗ SerialPort missing in CI!"; ((ERRORS++)); }
fi
if grep -q "core5compat" UnifiedFloppyTool.pro 2>/dev/null; then
    grep -rq "QTextCodec\|QRegExp" src/ 2>/dev/null && echo "  ✓ core5compat needed" || { echo "  ✗ core5compat unused!"; ((ERRORS++)); }
fi

# 3. Integer overflow
echo -e "\n[3/7] Integer overflow..."
BAD=$(grep -rn "0x[0-9a-fA-F]*<<24" src/ --include="*.c" 2>/dev/null | grep -v "uint32_t\|1U<<\|1u<<" | head -3)
[ -z "$BAD" ] && echo "  ✓ Safe" || { echo "  ✗ Overflow risk:"; echo "$BAD" | sed 's/^/    /'; ((ERRORS++)); }

# 4. GitHub URLs
echo -e "\n[4/7] GitHub URLs..."
grep -rq "axelmuhr" src/ 2>/dev/null && { echo "  ✗ Wrong username!"; ((ERRORS++)); } || echo "  ✓ Correct"

# 5. CI Windows shell
echo -e "\n[5/7] CI Windows config..."
if grep -q "QT_ROOT_DIR" .github/workflows/ci.yml; then
    echo "  ✓ Uses QT_ROOT_DIR"
elif grep -q "Qt6_DIR" .github/workflows/ci.yml; then
    echo "  ⚠ Qt6_DIR may fail on Windows"; ((WARNINGS++))
fi

# 6. ZIP would include .github
echo -e "\n[6/7] .github directory..."
[ -d ".github/workflows" ] && echo "  ✓ Present" || { echo "  ✗ Missing!"; ((ERRORS++)); }

# 7. Local build check
echo -e "\n[7/7] Build artifacts..."
[ -d "build" ] && echo "  ✓ Build exists" || { echo "  ⚠ No build dir (test locally?)"; ((WARNINGS++)); }

# Summary
echo ""
echo "══════════════════════════════════════════════════════════════"
if [ "$ERRORS" -eq 0 ]; then
    [ "$WARNINGS" -gt 0 ] && echo "  ⚠ PASSED ($WARNINGS warnings)" || echo "  ✅ PASSED - Safe to push!"
    exit 0
else
    echo "  ❌ FAILED - $ERRORS errors"
    exit 1
fi
