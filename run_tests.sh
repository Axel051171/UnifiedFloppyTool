#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# UFT Unified Test Runner
# 
# Compiles and runs all tests with summary report.
# Part of INDUSTRIAL_UPGRADE_PLAN.
#
# Usage: ./run_tests.sh [--quick|--full|--coverage]
# ═══════════════════════════════════════════════════════════════════════════════

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
BUILD_DIR="build_tests"
TEST_DIR="tests"
COVERAGE_DIR="coverage"

# Test list
TESTS=(
    "test_smoke"
    "test_crc"
    "test_pll"
    "test_format_verify"
    "test_format_autodetect"
    "test_roundtrip"
    "test_integration"
    "test_golden"
    "test_benchmark"
)

# Counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

print_header() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  UFT Test Runner v3.8.0${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════════${NC}"
    echo ""
}

print_section() {
    echo ""
    echo -e "${YELLOW}>>> $1${NC}"
    echo ""
}

compile_test() {
    local test_name=$1
    local test_file="${TEST_DIR}/${test_name}.c"
    local test_bin="${BUILD_DIR}/${test_name}"
    
    if [ ! -f "$test_file" ]; then
        return 1
    fi
    
    # Compile with common flags
    gcc -std=c11 -Wall -Wextra -O2 \
        -I include \
        -I src \
        -D_POSIX_C_SOURCE=200809L \
        -o "$test_bin" \
        "$test_file" \
        -lm 2>/dev/null
    
    return $?
}

run_test() {
    local test_name=$1
    local test_bin="${BUILD_DIR}/${test_name}"
    
    ((TOTAL++))
    
    printf "  [%02d] %-35s " "$TOTAL" "$test_name"
    
    if [ ! -f "$test_bin" ]; then
        echo -e "${YELLOW}[SKIP]${NC} (not compiled)"
        ((SKIPPED++))
        return
    fi
    
    # Run test with timeout
    if timeout 30s "$test_bin" > "${BUILD_DIR}/${test_name}.log" 2>&1; then
        echo -e "${GREEN}[PASS]${NC}"
        ((PASSED++))
    else
        echo -e "${RED}[FAIL]${NC}"
        ((FAILED++))
        # Show last 5 lines of log
        echo "      Last output:"
        tail -5 "${BUILD_DIR}/${test_name}.log" | sed 's/^/      /'
    fi
}

print_summary() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  Summary${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo "  Total:   $TOTAL"
    echo -e "  Passed:  ${GREEN}$PASSED${NC}"
    echo -e "  Failed:  ${RED}$FAILED${NC}"
    echo -e "  Skipped: ${YELLOW}$SKIPPED${NC}"
    echo ""
    
    if [ $FAILED -eq 0 ] && [ $PASSED -gt 0 ]; then
        echo -e "  ${GREEN}✓ All tests passed!${NC}"
        return 0
    else
        echo -e "  ${RED}✗ Some tests failed${NC}"
        return 1
    fi
}

# Main
print_header

# Parse arguments
MODE="quick"
if [ "$1" == "--full" ]; then
    MODE="full"
elif [ "$1" == "--coverage" ]; then
    MODE="coverage"
fi

echo "Mode: $MODE"

# Create build directory
mkdir -p "$BUILD_DIR"

print_section "Compiling Tests"

for test in "${TESTS[@]}"; do
    printf "  Compiling %-35s " "$test"
    if compile_test "$test"; then
        echo -e "${GREEN}[OK]${NC}"
    else
        echo -e "${YELLOW}[SKIP]${NC}"
    fi
done

print_section "Running Tests"

for test in "${TESTS[@]}"; do
    run_test "$test"
done

print_summary
exit $?
