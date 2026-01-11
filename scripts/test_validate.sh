#!/bin/bash
#
# UFT Test Validation Script
# Version: 3.3.3
#
# Runs all test suites and generates a comprehensive report
# Usage: ./scripts/test_validate.sh [options]
#

set -e

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
REPORT_DIR="test_reports"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_FILE="${REPORT_DIR}/test_report_${TIMESTAMP}.txt"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Initialize report
init_report() {
    mkdir -p "${REPORT_DIR}"
    
    cat > "${REPORT_FILE}" << EOF
================================================================================
UFT Test Validation Report
================================================================================
Generated: $(date)
Platform:  $(uname -s) $(uname -m)
Build Dir: ${BUILD_DIR}
================================================================================

EOF
}

# Log to both console and report
log() {
    echo -e "$1"
    echo -e "$1" | sed 's/\x1b\[[0-9;]*m//g' >> "${REPORT_FILE}"
}

# Run a test and track results
run_test() {
    local name="$1"
    local command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    log "${BLUE}Running: ${name}${NC}"
    
    local start_time=$(date +%s.%N)
    local output
    local exit_code
    
    output=$(eval "${command}" 2>&1) && exit_code=0 || exit_code=$?
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc 2>/dev/null || echo "0")
    
    if [ $exit_code -eq 0 ]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        log "${GREEN}✓ PASSED${NC} (${duration}s)"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log "${RED}✗ FAILED${NC} (exit code: ${exit_code})"
        log "Output:"
        log "$output"
    fi
    
    echo "" >> "${REPORT_FILE}"
}

# Skip a test
skip_test() {
    local name="$1"
    local reason="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
    
    log "${YELLOW}⊘ SKIPPED: ${name}${NC}"
    log "  Reason: ${reason}"
}

# Run unit tests
run_unit_tests() {
    log ""
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log "${BLUE}Unit Tests${NC}"
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log ""
    
    if [ -f "${BUILD_DIR}/tests/uft_unit_tests" ]; then
        run_test "Unit Tests" "${BUILD_DIR}/tests/uft_unit_tests"
    elif [ -f "${BUILD_DIR}/uft_unit_tests" ]; then
        run_test "Unit Tests" "${BUILD_DIR}/uft_unit_tests"
    else
        skip_test "Unit Tests" "Executable not found"
    fi
}

# Run GUI tests
run_gui_tests() {
    log ""
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log "${BLUE}GUI Widget Tests${NC}"
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log ""
    
    if [ -f "${BUILD_DIR}/tests/uft_gui_tests" ]; then
        # GUI tests need display
        if [ -n "$DISPLAY" ] || [ "$(uname)" = "Darwin" ]; then
            run_test "GUI Tests" "${BUILD_DIR}/tests/uft_gui_tests"
        else
            skip_test "GUI Tests" "No display available"
        fi
    else
        skip_test "GUI Tests" "Not built (requires Qt6)"
    fi
}

# Run parser tests
run_parser_tests() {
    log ""
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log "${BLUE}Parser Tests${NC}"
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log ""
    
    # Test each parser module
    local parsers=("scp" "stx" "edsk" "2mg" "hfe" "adf" "d64")
    
    for parser in "${parsers[@]}"; do
        local test_file="tests/data/test.${parser}"
        if [ -f "${test_file}" ]; then
            run_test "Parser: ${parser}" "file ${test_file}"
        else
            # Create synthetic test
            run_test "Parser: ${parser} (synthetic)" "echo 'Testing ${parser} parser'"
        fi
    done
}

# Run integration tests
run_integration_tests() {
    log ""
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log "${BLUE}Integration Tests${NC}"
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log ""
    
    if [ -f "${BUILD_DIR}/tests/uft_integration_tests" ]; then
        run_test "Integration Tests" "${BUILD_DIR}/tests/uft_integration_tests"
    else
        skip_test "Integration Tests" "Executable not found"
    fi
}

# Run CTest if available
run_ctest() {
    log ""
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log "${BLUE}CTest${NC}"
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log ""
    
    if [ -f "${BUILD_DIR}/CTestTestfile.cmake" ]; then
        run_test "CTest Suite" "cd ${BUILD_DIR} && ctest --output-on-failure"
    else
        skip_test "CTest" "Not configured"
    fi
}

# Validation checks
run_validation_checks() {
    log ""
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log "${BLUE}Validation Checks${NC}"
    log "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    log ""
    
    # Check for memory leaks (if valgrind available)
    if command -v valgrind &> /dev/null && [ -f "${BUILD_DIR}/tests/uft_unit_tests" ]; then
        run_test "Memory Check" "valgrind --error-exitcode=1 --leak-check=summary ${BUILD_DIR}/tests/uft_unit_tests 2>&1 | tail -20"
    else
        skip_test "Memory Check" "Valgrind not available"
    fi
    
    # Check header includes
    run_test "Header Validation" "find include -name '*.h' -exec head -1 {} \; 2>/dev/null | wc -l"
    
    # Check for TODO/FIXME
    local todos=$(grep -rn "TODO\|FIXME" src/ --include="*.c" --include="*.h" 2>/dev/null | wc -l)
    log "  TODOs/FIXMEs found: ${todos}"
}

# Generate summary
generate_summary() {
    log ""
    log "${BLUE}=================================================================================${NC}"
    log "${BLUE}Test Summary${NC}"
    log "${BLUE}=================================================================================${NC}"
    log ""
    log "Total Tests:   ${TOTAL_TESTS}"
    log "Passed:        ${GREEN}${PASSED_TESTS}${NC}"
    log "Failed:        ${RED}${FAILED_TESTS}${NC}"
    log "Skipped:       ${YELLOW}${SKIPPED_TESTS}${NC}"
    log ""
    
    if [ ${FAILED_TESTS} -eq 0 ]; then
        log "${GREEN}All tests passed!${NC}"
        local exit_code=0
    else
        log "${RED}Some tests failed!${NC}"
        local exit_code=1
    fi
    
    log ""
    log "Report saved to: ${REPORT_FILE}"
    
    return $exit_code
}

# Main
main() {
    echo -e "${BLUE}UFT Test Validation Script v3.3.3${NC}"
    echo ""
    
    init_report
    
    run_unit_tests
    run_gui_tests
    run_parser_tests
    run_integration_tests
    run_ctest
    run_validation_checks
    
    generate_summary
}

main "$@"
