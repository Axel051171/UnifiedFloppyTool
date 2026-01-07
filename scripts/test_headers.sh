#!/bin/bash
# =============================================================================
# UFT Header Standalone Compile Test
# =============================================================================
# 
# This script tests that all public headers can be compiled standalone.
# A header that requires other headers to be included first is a design bug.
#
# Usage:
#   ./scripts/test_headers.sh [--strict] [--verbose]
#
# Options:
#   --strict   Fail on first error (for CI)
#   --verbose  Show compilation output
#
# Exit codes:
#   0 - All headers pass
#   1 - One or more headers fail
#
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INCLUDE_DIR="$PROJECT_ROOT/include"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Options
STRICT=0
VERBOSE=0
for arg in "$@"; do
    case $arg in
        --strict) STRICT=1 ;;
        --verbose) VERBOSE=1 ;;
    esac
done

# Compiler settings
CC="${CC:-gcc}"
CFLAGS="-std=c11 -Wall -Wextra -Werror -Wmissing-prototypes -Wstrict-prototypes -Wshadow -Wundef"
INCLUDES="-I$INCLUDE_DIR -I$INCLUDE_DIR/uft -I$INCLUDE_DIR/uft/core"

# Test directory
TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT

# Counters
PASSED=0
FAILED=0
SKIPPED=0

# =============================================================================
# Header Categories
# =============================================================================

# Core headers - MUST pass
CORE_HEADERS=(
    "include/uft/core/uft_constants.h"
    "include/uft/core/uft_error_codes.h"
    "include/uft/core/uft_encoding.h"
    "include/uft/core/uft_disk.h"
    "include/uft/core/uft_track_base.h"
    "include/uft/core/uft_sector.h"
    "include/uft/core/uft_crc_v2.h"
    "include/uft/core/uft_histogram.h"
    "include/uft/core/uft_safe_string.h"
    "include/uft/core/uft_safe_io.h"
    "include/uft/core/uft_const_correct.h"
    "include/uft/core/uft_core.h"
)

# Private headers - skip
PRIVATE_PATTERNS=(
    "*/compat/*"
    "*/flux/fdc_bitstream/*"
    "*/flux/vfo/*"
    "*_internal.h"
    "*_private.h"
)

# =============================================================================
# Test Function
# =============================================================================

test_header() {
    local header="$1"
    local name=$(basename "$header" .h)
    
    # Skip private headers
    for pattern in "${PRIVATE_PATTERNS[@]}"; do
        if [[ "$header" == $pattern ]]; then
            ((SKIPPED++))
            [ $VERBOSE -eq 1 ] && echo -e "${YELLOW}SKIP${NC} $name (private)"
            return 0
        fi
    done
    
    # Create test file
    cat > "$TEST_DIR/test_$name.c" << EOF
#include "$PROJECT_ROOT/$header"
int main(void) { return 0; }
EOF
    
    # Compile
    if $CC $CFLAGS $INCLUDES -c "$TEST_DIR/test_$name.c" -o "$TEST_DIR/test_$name.o" 2>"$TEST_DIR/err_$name.txt"; then
        ((PASSED++))
        echo -e "${GREEN}PASS${NC} $name"
        return 0
    else
        ((FAILED++))
        echo -e "${RED}FAIL${NC} $name"
        [ $VERBOSE -eq 1 ] && cat "$TEST_DIR/err_$name.txt"
        
        if [ $STRICT -eq 1 ]; then
            echo ""
            echo "Error in $header:"
            head -5 "$TEST_DIR/err_$name.txt"
            exit 1
        fi
        return 1
    fi
}

# =============================================================================
# Main
# =============================================================================

echo "=============================================="
echo "UFT Header Standalone Compile Test"
echo "=============================================="
echo "Compiler: $CC"
echo "Flags: $CFLAGS"
echo ""

# Test core headers (mandatory)
echo "--- Core Headers (Mandatory) ---"
for header in "${CORE_HEADERS[@]}"; do
    if [ -f "$PROJECT_ROOT/$header" ]; then
        test_header "$header"
    else
        echo -e "${YELLOW}SKIP${NC} $(basename "$header") (not found)"
        ((SKIPPED++))
    fi
done

# Test all other public headers
echo ""
echo "--- Other Public Headers ---"
find "$INCLUDE_DIR/uft" -name "*.h" -type f | sort | while read header; do
    # Convert to relative path
    rel_header="${header#$PROJECT_ROOT/}"
    
    # Skip if already tested as core
    is_core=0
    for core in "${CORE_HEADERS[@]}"; do
        if [ "$rel_header" == "$core" ]; then
            is_core=1
            break
        fi
    done
    [ $is_core -eq 1 ] && continue
    
    test_header "$rel_header"
done

# Summary
echo ""
echo "=============================================="
echo "Results"
echo "=============================================="
echo -e "${GREEN}Passed:${NC}  $PASSED"
echo -e "${RED}Failed:${NC}  $FAILED"
echo -e "${YELLOW}Skipped:${NC} $SKIPPED"

if [ $FAILED -gt 0 ]; then
    echo ""
    echo -e "${RED}HEADER TESTS FAILED${NC}"
    exit 1
else
    echo ""
    echo -e "${GREEN}ALL HEADER TESTS PASSED${NC}"
    exit 0
fi
