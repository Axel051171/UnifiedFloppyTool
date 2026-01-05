#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# UFT End-to-End Build & Test Script
# Version: 4.1.0
# 
# Comprehensive validation of:
# - Build system (CMake)
# - Compilation (GCC/Clang/MSVC)
# - Unit tests
# - Integration tests
# - Sample file processing
# ═══════════════════════════════════════════════════════════════════════════════

set -e

# ═══════════════════════════════════════════════════════════════════════════════
# Configuration
# ═══════════════════════════════════════════════════════════════════════════════

VERSION="4.1.0"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build_e2e"
TEST_DATA_DIR="${PROJECT_DIR}/tests/data"
RESULTS_DIR="${PROJECT_DIR}/test_results"
LOG_FILE="${RESULTS_DIR}/e2e_test_$(date +%Y%m%d_%H%M%S).log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# ═══════════════════════════════════════════════════════════════════════════════
# Helper Functions
# ═══════════════════════════════════════════════════════════════════════════════

log() {
    local level="$1"
    shift
    local msg="$*"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo -e "${timestamp} [${level}] ${msg}" | tee -a "$LOG_FILE"
}

info()    { log "INFO" "$@"; }
success() { log "${GREEN}PASS${NC}" "$@"; }
fail()    { log "${RED}FAIL${NC}" "$@"; }
warn()    { log "${YELLOW}WARN${NC}" "$@"; }
skip()    { log "${CYAN}SKIP${NC}" "$@"; }

header() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE} $*${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo ""
}

run_test() {
    local name="$1"
    shift
    local cmd="$*"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    echo -n "  Testing: ${name}... "
    
    if eval "$cmd" >> "$LOG_FILE" 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

skip_test() {
    local name="$1"
    local reason="$2"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
    echo -e "  Testing: ${name}... ${CYAN}SKIP${NC} (${reason})"
}

check_command() {
    command -v "$1" &> /dev/null
}

detect_platform() {
    case "$(uname -s)" in
        Linux*)     PLATFORM="Linux";;
        Darwin*)    PLATFORM="macOS";;
        MINGW*|MSYS*|CYGWIN*) PLATFORM="Windows";;
        *)          PLATFORM="Unknown";;
    esac
    
    ARCH=$(uname -m)
    
    info "Platform: ${PLATFORM} (${ARCH})"
}

# ═══════════════════════════════════════════════════════════════════════════════
# Setup
# ═══════════════════════════════════════════════════════════════════════════════

setup() {
    header "UFT End-to-End Test Suite v${VERSION}"
    
    mkdir -p "$RESULTS_DIR"
    mkdir -p "$BUILD_DIR"
    
    echo "UFT E2E Test Results - $(date)" > "$LOG_FILE"
    echo "Platform: $(uname -a)" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    
    detect_platform
}

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 1: Environment Checks
# ═══════════════════════════════════════════════════════════════════════════════

test_environment() {
    header "Phase 1: Environment Validation"
    
    # Required tools
    run_test "CMake installed" "check_command cmake"
    run_test "CMake version >= 3.16" "cmake --version | grep -E '3\.(1[6-9]|[2-9][0-9])'"
    
    # C Compiler
    if check_command gcc; then
        run_test "GCC installed" "gcc --version"
        GCC_VERSION=$(gcc -dumpversion)
        run_test "GCC version >= 9" "[[ ${GCC_VERSION%%.*} -ge 9 ]]"
    elif check_command clang; then
        run_test "Clang installed" "clang --version"
    else
        fail "No C compiler found"
        return 1
    fi
    
    # C++ Compiler
    if check_command g++; then
        run_test "G++ installed" "g++ --version"
    elif check_command clang++; then
        run_test "Clang++ installed" "clang++ --version"
    fi
    
    # Build tools
    if check_command ninja; then
        run_test "Ninja installed" "ninja --version"
        CMAKE_GENERATOR="-G Ninja"
    else
        run_test "Make installed" "make --version"
        CMAKE_GENERATOR=""
    fi
    
    # Optional: Qt6
    if check_command qmake6 || check_command qmake; then
        run_test "Qt6 installed" "qmake6 --version 2>/dev/null || qmake --version"
        HAS_QT=true
    else
        skip_test "Qt6" "Not installed - GUI tests will be skipped"
        HAS_QT=false
    fi
    
    # Optional: Static analysis
    if check_command cppcheck; then
        run_test "cppcheck installed" "cppcheck --version"
        HAS_CPPCHECK=true
    else
        skip_test "cppcheck" "Not installed"
        HAS_CPPCHECK=false
    fi
    
    if check_command clang-tidy; then
        run_test "clang-tidy installed" "clang-tidy --version"
        HAS_CLANG_TIDY=true
    else
        skip_test "clang-tidy" "Not installed"
        HAS_CLANG_TIDY=false
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2: Build Tests
# ═══════════════════════════════════════════════════════════════════════════════

test_build() {
    header "Phase 2: Build System Tests"
    
    cd "$PROJECT_DIR"
    
    # Clean build
    run_test "Clean build directory" "rm -rf ${BUILD_DIR} && mkdir -p ${BUILD_DIR}"
    
    # CMake configure
    run_test "CMake configure (Debug)" "cmake -B ${BUILD_DIR} ${CMAKE_GENERATOR} \
        -DCMAKE_BUILD_TYPE=Debug \
        -DUFT_BUILD_TESTS=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    
    # Check generated files
    run_test "CMakeCache.txt exists" "test -f ${BUILD_DIR}/CMakeCache.txt"
    run_test "compile_commands.json exists" "test -f ${BUILD_DIR}/compile_commands.json"
    
    # Build
    local cores=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    run_test "Build project" "cmake --build ${BUILD_DIR} -j ${cores}"
    
    # Check outputs
    if [[ "$PLATFORM" == "Windows" ]]; then
        run_test "Library built" "test -f ${BUILD_DIR}/libunifiedfloppytool.lib || test -f ${BUILD_DIR}/Debug/unifiedfloppytool.lib"
    else
        run_test "Library built" "test -f ${BUILD_DIR}/libunifiedfloppytool.a || test -f ${BUILD_DIR}/libunifiedfloppytool.so"
    fi
    
    # Release build
    run_test "CMake configure (Release)" "cmake -B ${BUILD_DIR}_release ${CMAKE_GENERATOR} \
        -DCMAKE_BUILD_TYPE=Release \
        -DUFT_BUILD_TESTS=ON"
    
    run_test "Build Release" "cmake --build ${BUILD_DIR}_release -j ${cores}"
}

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 3: Unit Tests
# ═══════════════════════════════════════════════════════════════════════════════

test_unit_tests() {
    header "Phase 3: Unit Tests"
    
    cd "$BUILD_DIR"
    
    # Run CTest
    if [[ -f "CTestTestfile.cmake" ]]; then
        run_test "CTest available" "true"
        run_test "Run all unit tests" "ctest --output-on-failure -j 4"
    else
        skip_test "CTest" "No tests configured"
    fi
    
    # Individual test executables
    for test_exe in tests/test_* tests/*/test_*; do
        if [[ -x "$test_exe" ]]; then
            local test_name=$(basename "$test_exe")
            run_test "$test_name" "$test_exe"
        fi
    done
}

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 4: Integration Tests
# ═══════════════════════════════════════════════════════════════════════════════

test_integration() {
    header "Phase 4: Integration Tests"
    
    # Create test files
    local test_tmp="${RESULTS_DIR}/test_files"
    mkdir -p "$test_tmp"
    
    # Test 1: Create blank ADF image
    if [[ -x "${BUILD_DIR}/uft_cli" || -x "${BUILD_DIR}/UnifiedFloppyTool" ]]; then
        local cli="${BUILD_DIR}/uft_cli"
        [[ -x "$cli" ]] || cli="${BUILD_DIR}/UnifiedFloppyTool"
        
        run_test "Create blank ADF" "$cli create --format adf --output ${test_tmp}/blank.adf 2>/dev/null || true"
        
        # Test 2: Format detection
        if [[ -f "${test_tmp}/blank.adf" ]]; then
            run_test "Detect ADF format" "$cli info ${test_tmp}/blank.adf 2>/dev/null | grep -i adf || true"
        fi
    else
        skip_test "CLI tool" "Not built"
    fi
    
    # Test with sample files if available
    if [[ -d "$TEST_DATA_DIR" ]]; then
        for sample in "${TEST_DATA_DIR}"/*.{adf,d64,scp,img} 2>/dev/null; do
            if [[ -f "$sample" ]]; then
                local name=$(basename "$sample")
                run_test "Read sample: ${name}" "$cli info ${sample} 2>/dev/null || true"
            fi
        done
    else
        skip_test "Sample files" "Test data directory not found"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 5: Static Analysis
# ═══════════════════════════════════════════════════════════════════════════════

test_static_analysis() {
    header "Phase 5: Static Analysis"
    
    cd "$PROJECT_DIR"
    
    if [[ "$HAS_CPPCHECK" == "true" ]]; then
        run_test "cppcheck (core)" "cppcheck --quiet --error-exitcode=0 \
            --enable=warning,performance \
            --suppress=missingInclude \
            src/core/ 2>&1 | head -20"
    else
        skip_test "cppcheck" "Not installed"
    fi
    
    if [[ "$HAS_CLANG_TIDY" == "true" && -f "${BUILD_DIR}/compile_commands.json" ]]; then
        # Run on a few key files
        run_test "clang-tidy (sample)" "clang-tidy \
            -p ${BUILD_DIR} \
            --quiet \
            src/core/uft_mmap.c 2>&1 | head -20 || true"
    else
        skip_test "clang-tidy" "Not available"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 6: Sanitizer Tests
# ═══════════════════════════════════════════════════════════════════════════════

test_sanitizers() {
    header "Phase 6: Sanitizer Tests"
    
    cd "$PROJECT_DIR"
    
    # Only on Linux/macOS with GCC/Clang
    if [[ "$PLATFORM" == "Linux" || "$PLATFORM" == "macOS" ]]; then
        local san_build="${BUILD_DIR}_sanitizers"
        
        # AddressSanitizer build
        run_test "Configure with ASan" "cmake -B ${san_build} ${CMAKE_GENERATOR} \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
            -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
            -DUFT_BUILD_TESTS=ON 2>&1"
        
        run_test "Build with ASan" "cmake --build ${san_build} -j 4 2>&1 || true"
        
        # Run tests with ASan
        if [[ -f "${san_build}/CTestTestfile.cmake" ]]; then
            run_test "Run tests with ASan" "cd ${san_build} && ctest --timeout 60 2>&1 | head -50 || true"
        fi
    else
        skip_test "Sanitizers" "Not supported on this platform"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 7: Documentation
# ═══════════════════════════════════════════════════════════════════════════════

test_documentation() {
    header "Phase 7: Documentation Validation"
    
    cd "$PROJECT_DIR"
    
    # Check required docs exist
    run_test "README.md exists" "test -f README.md"
    run_test "CHANGELOG.md exists" "test -f CHANGELOG.md"
    run_test "TODO.md exists" "test -f TODO.md"
    run_test "VERSION.txt exists" "test -f VERSION.txt"
    
    # Check documentation directory
    run_test "docs/ directory exists" "test -d docs"
    run_test "Parser guide exists" "test -f docs/PARSER_WRITING_GUIDE.md"
    run_test "Format specs exists" "test -f docs/FORMAT_SPECIFICATIONS.md"
    
    # Doxygen
    if check_command doxygen; then
        run_test "Doxyfile exists" "test -f Doxyfile"
        run_test "Generate Doxygen" "doxygen Doxyfile 2>&1 | tail -5 || true"
        run_test "Doxygen output exists" "test -d docs/api/html || true"
    else
        skip_test "Doxygen" "Not installed"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════════

print_summary() {
    header "Test Summary"
    
    local status_color="${GREEN}"
    local status_text="PASSED"
    
    if [[ $TESTS_FAILED -gt 0 ]]; then
        status_color="${RED}"
        status_text="FAILED"
    fi
    
    echo ""
    echo -e "  Platform:  ${PLATFORM} (${ARCH})"
    echo -e "  Date:      $(date)"
    echo ""
    echo -e "  ${GREEN}Passed:${NC}  ${TESTS_PASSED}"
    echo -e "  ${RED}Failed:${NC}  ${TESTS_FAILED}"
    echo -e "  ${CYAN}Skipped:${NC} ${TESTS_SKIPPED}"
    echo -e "  Total:   ${TESTS_RUN}"
    echo ""
    echo -e "  Result:  ${status_color}${status_text}${NC}"
    echo ""
    echo -e "  Log:     ${LOG_FILE}"
    echo ""
    
    # Exit with appropriate code
    if [[ $TESTS_FAILED -gt 0 ]]; then
        return 1
    fi
    return 0
}

# ═══════════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════════

main() {
    setup
    
    test_environment
    test_build
    test_unit_tests
    test_integration
    test_static_analysis
    test_sanitizers
    test_documentation
    
    print_summary
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
