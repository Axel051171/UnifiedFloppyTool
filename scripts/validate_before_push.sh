#!/bin/bash
#═══════════════════════════════════════════════════════════════════════════════
# UFT Pre-Push Validator - Run BEFORE every git push
#═══════════════════════════════════════════════════════════════════════════════

set -e
ERRORS=0

echo ""
echo "══════════════════════════════════════════════════════════════"
echo "  UFT Pre-Push Validation"
echo "══════════════════════════════════════════════════════════════"
echo ""

#───────────────────────────────────────────────────────────────────
# 1. Check critical files exist
#───────────────────────────────────────────────────────────────────
echo "[1/7] Checking critical files..."

for file in "UnifiedFloppyTool.pro" "CMakeLists.txt" ".github/workflows/ci.yml" "src/main.cpp"; do
    if [ -f "$file" ]; then
        echo "  ✓ $file"
    else
        echo "  ✗ MISSING: $file"
        ((ERRORS++)) || true
    fi
done

#───────────────────────────────────────────────────────────────────
# 2. Check for problematic Qt modules
#───────────────────────────────────────────────────────────────────
echo ""
echo "[2/7] Checking Qt module dependencies..."

if grep -q "core5compat" UnifiedFloppyTool.pro 2>/dev/null; then
    # Check if actually needed
    if ! grep -rq "QTextCodec\|QRegExp\|QLinkedList" src/ --include="*.cpp" --include="*.h" 2>/dev/null; then
        echo "  ✗ core5compat in .pro but NOT used - remove it!"
        ((ERRORS++)) || true
    else
        echo "  ✓ core5compat needed and present"
    fi
else
    echo "  ✓ No unnecessary Qt5 compat modules"
fi

#───────────────────────────────────────────────────────────────────
# 3. Check GitHub URLs
#───────────────────────────────────────────────────────────────────
echo ""
echo "[3/7] Checking GitHub URLs..."

if grep -rq "axelmuhr" src/ --include="*.cpp" --include="*.h" 2>/dev/null; then
    echo "  ✗ WRONG GitHub username 'axelmuhr' found"
    grep -rn "axelmuhr" src/ --include="*.cpp" --include="*.h"
    ((ERRORS++)) || true
else
    echo "  ✓ GitHub URLs correct"
fi

#───────────────────────────────────────────────────────────────────
# 4. Check for integer overflow patterns
#───────────────────────────────────────────────────────────────────
echo ""
echo "[4/7] Checking for integer overflow patterns..."

# Find <<24 shifts without uint32_t cast
OVERFLOW_ISSUES=$(grep -rn "<<24\|<<16" src/ --include="*.c" --include="*.h" 2>/dev/null | grep -v "uint32_t" | grep -v "//" || true)
if [ -n "$OVERFLOW_ISSUES" ]; then
    echo "  ⚠ Potential integer overflow (missing uint32_t cast):"
    echo "$OVERFLOW_ISSUES" | head -5
else
    echo "  ✓ Bit shifts appear safe"
fi

#───────────────────────────────────────────────────────────────────
# 5. Check array bounds (MAX_* vs actual array size)
#───────────────────────────────────────────────────────────────────
echo ""
echo "[5/7] Checking array bounds definitions..."

# Look for suspicious MAX_FORMATS type definitions
if grep -rq "MAX_FORMATS 64" src/ --include="*.c" --include="*.h" 2>/dev/null; then
    echo "  ⚠ MAX_FORMATS=64 found - verify array size matches"
else
    echo "  ✓ No suspicious array bounds"
fi

#───────────────────────────────────────────────────────────────────
# 6. Run cppcheck if available
#───────────────────────────────────────────────────────────────────
echo ""
echo "[6/7] Static analysis..."

if command -v cppcheck &> /dev/null; then
    CPPCHECK_ERRORS=$(cppcheck --enable=warning --error-exitcode=0 --quiet \
        --suppress=missingIncludeSystem \
        -I include -I src \
        src/ 2>&1 | grep -c "error:" || true)
    
    if [ "$CPPCHECK_ERRORS" -gt 0 ]; then
        echo "  ✗ cppcheck found $CPPCHECK_ERRORS errors"
        ((ERRORS++)) || true
    else
        echo "  ✓ cppcheck passed"
    fi
else
    echo "  ⊘ cppcheck not installed (skipped)"
fi

#───────────────────────────────────────────────────────────────────
# 7. Validate YAML syntax
#───────────────────────────────────────────────────────────────────
echo ""
echo "[7/7] Validating CI workflow..."

if command -v python3 &> /dev/null; then
    if python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci.yml'))" 2>/dev/null; then
        echo "  ✓ CI YAML syntax valid"
    else
        echo "  ✗ CI YAML syntax error"
        ((ERRORS++)) || true
    fi
else
    # Basic check
    if grep -q "runs-on:" .github/workflows/ci.yml; then
        echo "  ✓ CI workflow appears valid"
    else
        echo "  ✗ CI workflow invalid"
        ((ERRORS++)) || true
    fi
fi

#───────────────────────────────────────────────────────────────────
# Summary
#───────────────────────────────────────────────────────────────────
echo ""
echo "══════════════════════════════════════════════════════════════"

if [ "$ERRORS" -eq 0 ]; then
    echo "  ✅ PASSED - Safe to push!"
    echo "══════════════════════════════════════════════════════════════"
    echo ""
    echo "Run: git add -A && git commit -m \"message\" && git push"
    exit 0
else
    echo "  ❌ FAILED - $ERRORS error(s) found. Fix before pushing!"
    echo "══════════════════════════════════════════════════════════════"
    exit 1
fi
