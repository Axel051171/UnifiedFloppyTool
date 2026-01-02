#!/bin/bash
# UFT Fuzz-Test Runner
# 
# Prerequisites:
#   - AFL++: apt install afl++
#   - libFuzzer: clang with fuzzer support
#   - ASan/UBSan: included with clang/gcc
#
# Usage:
#   ./run_fuzz.sh [scp|d64|g64|hfe|adf|all] [afl|libfuzzer] [duration_seconds]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CORPUS_DIR="${SCRIPT_DIR}/corpus"
FINDINGS_DIR="${SCRIPT_DIR}/findings"

TARGET="${1:-all}"
FUZZER="${2:-afl}"
DURATION="${3:-3600}"  # Default: 1 hour

mkdir -p "$BUILD_DIR" "$FINDINGS_DIR"

# Compiler flags
CFLAGS="-g -O1 -fno-omit-frame-pointer"
SANITIZERS="-fsanitize=address,undefined -fno-sanitize-recover=all"

echo "════════════════════════════════════════════════════════════════════════"
echo "  UFT FUZZ-TEST RUNNER"
echo "════════════════════════════════════════════════════════════════════════"
echo ""
echo "  Target:   $TARGET"
echo "  Fuzzer:   $FUZZER"
echo "  Duration: ${DURATION}s"
echo ""

compile_target() {
    local name="$1"
    local src="${SCRIPT_DIR}/fuzz_${name}.c"
    
    if [[ ! -f "$src" ]]; then
        echo "  [SKIP] $name - source not found"
        return 1
    fi
    
    echo "  [BUILD] $name"
    
    if [[ "$FUZZER" == "afl" ]]; then
        if command -v afl-clang-fast &> /dev/null; then
            afl-clang-fast $CFLAGS $SANITIZERS "$src" -o "${BUILD_DIR}/fuzz_${name}"
        else
            echo "    AFL++ not installed, using libFuzzer"
            clang $CFLAGS $SANITIZERS -fsanitize=fuzzer "$src" -o "${BUILD_DIR}/fuzz_${name}"
        fi
    else
        clang $CFLAGS $SANITIZERS -fsanitize=fuzzer "$src" -o "${BUILD_DIR}/fuzz_${name}"
    fi
}

run_target() {
    local name="$1"
    local binary="${BUILD_DIR}/fuzz_${name}"
    local corpus="${CORPUS_DIR}/${name}"
    local findings="${FINDINGS_DIR}/${name}"
    
    if [[ ! -x "$binary" ]]; then
        echo "  [SKIP] $name - binary not found"
        return 1
    fi
    
    mkdir -p "$findings"
    
    echo ""
    echo "  [RUN] $name (${DURATION}s)"
    
    if [[ "$FUZZER" == "afl" ]] && command -v afl-fuzz &> /dev/null; then
        timeout "$DURATION" afl-fuzz \
            -i "$corpus" \
            -o "$findings" \
            -t 1000 \
            -- "$binary" @@ \
            || true
    else
        # libFuzzer
        "$binary" \
            -max_total_time="$DURATION" \
            -artifact_prefix="${findings}/" \
            "$corpus" \
            || true
    fi
}

# Compile
echo "[COMPILE]"
if [[ "$TARGET" == "all" ]]; then
    for t in scp d64 g64 hfe adf; do
        compile_target "$t" || true
    done
else
    compile_target "$TARGET"
fi

# Run
echo ""
echo "[FUZZ]"
if [[ "$TARGET" == "all" ]]; then
    for t in scp d64 g64 hfe adf; do
        run_target "$t" || true
    done
else
    run_target "$TARGET"
fi

echo ""
echo "════════════════════════════════════════════════════════════════════════"
echo "  DONE - Check $FINDINGS_DIR for crashes"
echo "════════════════════════════════════════════════════════════════════════"
