#!/bin/bash
# Build fuzz targets

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/../../.."

echo "Building hardened fuzz targets..."

# Check for AFL or libFuzzer
if command -v afl-clang-fast &> /dev/null; then
    echo "Using AFL++"
    CC=afl-clang-fast
    FUZZ_FLAGS=""
elif command -v clang &> /dev/null; then
    echo "Using libFuzzer"
    CC=clang
    FUZZ_FLAGS="-fsanitize=fuzzer,address"
else
    echo "Error: Need AFL++ or clang with libFuzzer"
    exit 1
fi

# Build SCP fuzzer
$CC $FUZZ_FLAGS -g -O1 \
    -I"$PROJECT_DIR/include" \
    -o fuzz_scp_hardened \
    fuzz_scp_hardened.c \
    "$PROJECT_DIR/src/formats/scp/uft_scp_hardened_impl.c"

# Build D64 fuzzer
$CC $FUZZ_FLAGS -g -O1 \
    -I"$PROJECT_DIR/include" \
    -o fuzz_d64_hardened \
    fuzz_d64_hardened.c \
    "$PROJECT_DIR/src/formats/d64/uft_d64_hardened_impl.c"

echo "Built: fuzz_scp_hardened, fuzz_d64_hardened"
echo ""
echo "Run with AFL++:"
echo "  afl-fuzz -i corpus_scp -o findings_scp -- ./fuzz_scp_hardened @@"
echo ""
echo "Run with libFuzzer:"
echo "  ./fuzz_scp_hardened corpus_scp"
