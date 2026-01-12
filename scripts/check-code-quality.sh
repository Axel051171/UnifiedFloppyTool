#!/bin/bash
#
# UFT Code Quality Checker
# Run clang-format and clang-tidy locally
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║           UFT Code Quality Checker                        ║"
echo "╚═══════════════════════════════════════════════════════════╝"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

#============================================================================
# Check tools
#============================================================================
check_tool() {
    if command -v "$1" &> /dev/null; then
        echo -e "${GREEN}✓${NC} $1 found"
        return 0
    else
        echo -e "${YELLOW}⚠${NC} $1 not found (skipping)"
        return 1
    fi
}

echo ""
echo "=== Checking tools ==="
HAS_FORMAT=$(check_tool clang-format && echo 1 || echo 0)
HAS_TIDY=$(check_tool clang-tidy && echo 1 || echo 0)
HAS_CPPCHECK=$(check_tool cppcheck && echo 1 || echo 0)

#============================================================================
# clang-format
#============================================================================
if [ "$HAS_FORMAT" = "1" ]; then
    echo ""
    echo "=== Running clang-format ==="
    
    if [ "$1" = "--fix" ]; then
        echo "Fixing style issues..."
        find src include -name '*.c' -o -name '*.h' | head -50 | while read f; do
            clang-format -i "$f"
        done
        echo -e "${GREEN}✓${NC} Style fixed"
    else
        ISSUES=0
        find src include -name '*.c' -o -name '*.h' | head -50 | while read f; do
            if ! clang-format --dry-run --Werror "$f" 2>/dev/null; then
                echo -e "${YELLOW}⚠${NC} $f"
                ISSUES=$((ISSUES + 1))
            fi
        done
        
        if [ $ISSUES -gt 0 ]; then
            echo -e "${YELLOW}$ISSUES files have style issues${NC}"
            echo "Run with --fix to auto-fix"
        else
            echo -e "${GREEN}✓${NC} All files pass style check"
        fi
    fi
fi

#============================================================================
# cppcheck
#============================================================================
if [ "$HAS_CPPCHECK" = "1" ]; then
    echo ""
    echo "=== Running cppcheck ==="
    
    cppcheck \
        --enable=warning,performance,portability \
        --inline-suppr \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        -I include \
        -q \
        src 2>&1 | head -30
    
    echo -e "${GREEN}✓${NC} cppcheck complete"
fi

#============================================================================
# clang-tidy (requires compile_commands.json)
#============================================================================
if [ "$HAS_TIDY" = "1" ]; then
    echo ""
    echo "=== Running clang-tidy ==="
    
    if [ -f "build/compile_commands.json" ]; then
        # Run on a few core files
        for f in src/core/*.c src/detect/*.c; do
            if [ -f "$f" ]; then
                echo "Checking $f..."
                clang-tidy -p build "$f" 2>&1 | grep -E "(warning:|error:)" | head -5 || true
            fi
        done
        echo -e "${GREEN}✓${NC} clang-tidy complete"
    else
        echo -e "${YELLOW}⚠${NC} No compile_commands.json - run cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    fi
fi

echo ""
echo "═══════════════════════════════════════════════════════════"
echo "Code quality check complete"
echo "═══════════════════════════════════════════════════════════"
