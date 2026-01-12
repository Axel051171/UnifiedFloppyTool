#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# UFT CI Verification Script (P1-3)
# 
# Runs the same checks as GitHub Actions CI, locally.
# Use this before pushing to ensure CI will pass.
# 
# Usage: ./scripts/ci_verify.sh [--quick|--full]
# ═══════════════════════════════════════════════════════════════════════════════

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
PASSED=0
FAILED=0

# Functions
log_section() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
}

log_pass() {
    echo -e "  ${GREEN}✓ $1${NC}"
    ((PASSED++))
}

log_fail() {
    echo -e "  ${RED}✗ $1${NC}"
    ((FAILED++))
}

log_warn() {
    echo -e "  ${YELLOW}⚠ $1${NC}"
}

log_info() {
    echo -e "  ${NC}$1${NC}"
}

# Parse arguments
MODE="quick"
if [ "$1" == "--full" ]; then
    MODE="full"
fi

# Change to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

echo ""
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║       UFT CI Verification Script (P1-3)                        ║"
echo "║       Mode: $MODE                                                    ║"
echo "╚════════════════════════════════════════════════════════════════╝"

# ═══════════════════════════════════════════════════════════════════════════════
# Check 1: Prerequisites
# ═══════════════════════════════════════════════════════════════════════════════
log_section "Check 1: Prerequisites"

if command -v cmake &> /dev/null; then
    CMAKE_VER=$(cmake --version | head -1)
    log_pass "CMake: $CMAKE_VER"
else
    log_fail "CMake not found"
    exit 1
fi

if command -v gcc &> /dev/null; then
    GCC_VER=$(gcc --version | head -1)
    log_pass "GCC: $GCC_VER"
elif command -v clang &> /dev/null; then
    CLANG_VER=$(clang --version | head -1)
    log_pass "Clang: $CLANG_VER"
else
    log_fail "No C compiler found"
    exit 1
fi

# ═══════════════════════════════════════════════════════════════════════════════
# Check 2: Configure
# ═══════════════════════════════════════════════════════════════════════════════
log_section "Check 2: CMake Configure"

rm -rf build_ci
mkdir -p build_ci

if cmake -B build_ci -DCMAKE_BUILD_TYPE=Release -DUFT_BUILD_GUI=OFF -DBUILD_TESTING=ON > /dev/null 2>&1; then
    log_pass "CMake configure successful"
else
    log_fail "CMake configure failed"
    cmake -B build_ci -DCMAKE_BUILD_TYPE=Release -DUFT_BUILD_GUI=OFF -DBUILD_TESTING=ON
    exit 1
fi

# ═══════════════════════════════════════════════════════════════════════════════
# Check 3: Build
# ═══════════════════════════════════════════════════════════════════════════════
log_section "Check 3: Build"

BUILD_OUTPUT=$(cmake --build build_ci -j$(nproc) 2>&1)
BUILD_RC=$?

if [ $BUILD_RC -eq 0 ]; then
    log_pass "Build successful"
else
    log_fail "Build failed (exit code: $BUILD_RC)"
    echo "$BUILD_OUTPUT" | tail -30
    exit 1
fi

# Check for errors in build output
ERROR_COUNT=$(echo "$BUILD_OUTPUT" | grep -c "error:" || true)
if [ "$ERROR_COUNT" -gt 0 ]; then
    log_fail "Build has $ERROR_COUNT errors"
else
    log_pass "No build errors"
fi

# Check warnings (informational)
WARN_COUNT=$(echo "$BUILD_OUTPUT" | grep -c "warning:" || true)
if [ "$WARN_COUNT" -gt 0 ]; then
    log_warn "Build has $WARN_COUNT warnings"
else
    log_pass "No build warnings"
fi

# ═══════════════════════════════════════════════════════════════════════════════
# Check 4: Tests
# ═══════════════════════════════════════════════════════════════════════════════
log_section "Check 4: Tests"

cd build_ci
TEST_OUTPUT=$(ctest --output-on-failure 2>&1)
TEST_RC=$?
cd ..

if [ $TEST_RC -eq 0 ]; then
    log_pass "All tests passed"
else
    log_fail "Some tests failed"
    echo "$TEST_OUTPUT"
fi

# Count tests
TEST_TOTAL=$(echo "$TEST_OUTPUT" | grep -oP '\d+(?= tests)' | head -1 || echo "?")
TEST_PASSED=$(echo "$TEST_OUTPUT" | grep -oP '\d+(?=% tests passed)' || echo "?")
log_info "Tests: $TEST_TOTAL total"

# ═══════════════════════════════════════════════════════════════════════════════
# Check 5: Static Libraries
# ═══════════════════════════════════════════════════════════════════════════════
log_section "Check 5: Static Libraries"

LIB_COUNT=$(find build_ci -name "*.a" | wc -l)
if [ "$LIB_COUNT" -ge 20 ]; then
    log_pass "Built $LIB_COUNT static libraries"
else
    log_warn "Only $LIB_COUNT libraries (expected 20+)"
fi

# Check key libraries
KEY_LIBS=("libuft_core.a" "libuft_track_analysis.a" "libuft_profiles.a" "libuft_sector.a")
for lib in "${KEY_LIBS[@]}"; do
    if find build_ci -name "$lib" | grep -q .; then
        log_pass "Found $lib"
    else
        log_fail "Missing $lib"
    fi
done

# ═══════════════════════════════════════════════════════════════════════════════
# Check 6: Quick Functionality Test
# ═══════════════════════════════════════════════════════════════════════════════
log_section "Check 6: Functionality Tests"

# Run smoke workflow test if exists
if [ -f "build_ci/tests/smoke/test_smoke_workflow" ]; then
    if ./build_ci/tests/smoke/test_smoke_workflow > /dev/null 2>&1; then
        log_pass "Smoke workflow test passed"
    else
        log_fail "Smoke workflow test failed"
    fi
fi

# Run sector parser test if exists  
if [ -f "build_ci/src/sector/test_sector_parser" ]; then
    if ./build_ci/src/sector/test_sector_parser > /dev/null 2>&1; then
        log_pass "Sector parser test passed"
    else
        log_fail "Sector parser test failed"
    fi
fi

# ═══════════════════════════════════════════════════════════════════════════════
# Full mode: Additional checks
# ═══════════════════════════════════════════════════════════════════════════════
if [ "$MODE" == "full" ]; then
    log_section "Full Mode: Additional Checks"
    
    # Check for TODO/FIXME in critical files
    TODO_COUNT=$(grep -r "TODO\|FIXME" src/ --include="*.c" --include="*.h" 2>/dev/null | wc -l)
    log_info "TODO/FIXME comments: $TODO_COUNT"
    
    # Check header guards
    GUARD_ISSUES=$(find include/ -name "*.h" -exec sh -c '! grep -q "#ifndef.*_H" "$1"' _ {} \; -print 2>/dev/null | wc -l)
    if [ "$GUARD_ISSUES" -eq 0 ]; then
        log_pass "All headers have guards"
    else
        log_warn "$GUARD_ISSUES headers may be missing guards"
    fi
fi

# ═══════════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════════
log_section "Summary"

echo ""
echo "  Passed: $PASSED"
echo "  Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  ✓ CI VERIFICATION PASSED                                      ║${NC}"
    echo -e "${GREEN}║    Ready to push!                                              ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════════╝${NC}"
    rm -rf build_ci
    exit 0
else
    echo -e "${RED}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  ✗ CI VERIFICATION FAILED                                      ║${NC}"
    echo -e "${RED}║    Fix issues before pushing!                                  ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════════════╝${NC}"
    exit 1
fi
