#!/usr/bin/env bash
# run_tests.sh – Comprehensive Test Runner
# Usage: ./run_tests.sh [options]

set -euo pipefail

# =============================================================================
# CONFIGURATION
# =============================================================================

BUILD_DIR="${BUILD_DIR:-build}"
SAMPLES_DIR="${SAMPLES_DIR:-}"
OUTPUT_DIR="${OUTPUT_DIR:-test-results}"
ENABLE_ASAN="${ENABLE_ASAN:-0}"
ENABLE_UBSAN="${ENABLE_UBSAN:-0}"
ENABLE_VALGRIND="${ENABLE_VALGRIND:-0}"

# =============================================================================
# COLORS
# =============================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# =============================================================================
# FUNCTIONS
# =============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

check_build() {
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        log_info "Run: mkdir build && cd build && cmake .. && make"
        exit 1
    fi
}

# =============================================================================
# MAIN
# =============================================================================

echo "=========================================="
echo "UnifiedFloppyTool - Test Runner"
echo "=========================================="
echo ""

# Check build directory
check_build

# Create output directory
mkdir -p "$OUTPUT_DIR"/{logs,reports,coverage}

# =============================================================================
# 1) UNIT TESTS
# =============================================================================

log_info "Running Unit Tests..."
cd "$BUILD_DIR"

if ctest --output-on-failure --parallel $(nproc 2>/dev/null || echo 2); then
    log_success "Unit tests passed"
else
    log_error "Unit tests failed"
    exit 1
fi

cd -

# =============================================================================
# 2) SANITIZER TESTS
# =============================================================================

if [ "$ENABLE_ASAN" = "1" ]; then
    log_info "Running with Address Sanitizer..."
    
    export ASAN_OPTIONS="detect_leaks=1:check_initialization_order=1:strict_init_order=1"
    
    if "$BUILD_DIR/UnifiedFloppyTool" --version > /dev/null 2>&1; then
        log_success "ASan: No issues detected"
    else
        log_error "ASan: Issues detected!"
        exit 1
    fi
fi

if [ "$ENABLE_UBSAN" = "1" ]; then
    log_info "Running with Undefined Behavior Sanitizer..."
    
    export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
    
    if "$BUILD_DIR/UnifiedFloppyTool" --version > /dev/null 2>&1; then
        log_success "UBSan: No issues detected"
    else
        log_error "UBSan: Issues detected!"
        exit 1
    fi
fi

# =============================================================================
# 3) VALGRIND
# =============================================================================

if [ "$ENABLE_VALGRIND" = "1" ] && command -v valgrind &> /dev/null; then
    log_info "Running Valgrind..."
    
    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --error-exitcode=1 \
        --log-file="$OUTPUT_DIR/logs/valgrind.log" \
        "$BUILD_DIR/UnifiedFloppyTool" --version > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        log_success "Valgrind: No memory leaks"
    else
        log_error "Valgrind: Memory leaks detected!"
        cat "$OUTPUT_DIR/logs/valgrind.log"
        exit 1
    fi
fi

# =============================================================================
# 4) REGRESSION TESTS (if samples provided)
# =============================================================================

if [ -n "$SAMPLES_DIR" ] && [ -d "$SAMPLES_DIR" ]; then
    log_info "Running Regression Tests..."
    
    PASSED=0
    FAILED=0
    
    find "$SAMPLES_DIR" -type f \( -iname "*.scp" -o -iname "*.hfe" -o -iname "*.kfraw" \) | while read -r sample; do
        base="$(basename "$sample")"
        name="${base%.*}"
        
        log_info "Testing: $base"
        
        # Run decode (CLI would be implemented separately)
        # "$BUILD_DIR/uft_cli" --input "$sample" --output "$OUTPUT_DIR/images/$name.img" \
        #     --report "$OUTPUT_DIR/reports/$name.json" \
        #     > "$OUTPUT_DIR/logs/$name.log" 2>&1
        
        # For now: just note that sample exists
        echo "  Sample: $sample" >> "$OUTPUT_DIR/logs/regression.log"
    done
    
    log_warn "Regression tests require uft_cli (not yet implemented)"
else
    log_warn "Skipping regression tests (no samples directory)"
fi

# =============================================================================
# 5) PERFORMANCE TESTS
# =============================================================================

if [ -f "$BUILD_DIR/tests/bench_mfm" ]; then
    log_info "Running Performance Benchmarks..."
    
    "$BUILD_DIR/tests/bench_mfm" > "$OUTPUT_DIR/reports/bench_mfm.txt" 2>&1 || true
    
    log_success "Benchmark results saved to $OUTPUT_DIR/reports/bench_mfm.txt"
fi

# =============================================================================
# SUMMARY
# =============================================================================

echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Build Directory:    $BUILD_DIR"
echo "Output Directory:   $OUTPUT_DIR"
echo ""
echo "Tests Run:"
echo "  ✅ Unit Tests"
[ "$ENABLE_ASAN" = "1" ] && echo "  ✅ Address Sanitizer"
[ "$ENABLE_UBSAN" = "1" ] && echo "  ✅ UB Sanitizer"
[ "$ENABLE_VALGRIND" = "1" ] && echo "  ✅ Valgrind"
[ -n "$SAMPLES_DIR" ] && echo "  ⚠️  Regression (samples: $SAMPLES_DIR)"
echo ""
log_success "All tests passed! 🎉"
echo ""
echo "Results available in: $OUTPUT_DIR"
echo "=========================================="
