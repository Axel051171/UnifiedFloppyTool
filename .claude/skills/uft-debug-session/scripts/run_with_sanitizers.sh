#!/usr/bin/env bash
#
# run_with_sanitizers.sh — Build UFT with ASan+UBSan and run a repro.
#
# Usage:
#     run_with_sanitizers.sh <uft-command> <fixture-path>
#
# Example:
#     run_with_sanitizers.sh "info" tests/vectors/scp/repro_bug_NNN.scp

set -u

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <uft-command> [args...]"
    echo "example: $0 info tests/vectors/scp/bad.scp"
    exit 2
fi

UFT_ROOT="${UFT_ROOT:-$(pwd)}"
cd "$UFT_ROOT"

echo "=== Building with ASan + UBSan ==="
make clean >/dev/null 2>&1 || true
export CFLAGS="-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-fsanitize=address,undefined"

if ! qmake; then
    echo "ERROR: qmake failed"
    exit 1
fi

if ! make -j"$(nproc)"; then
    echo "ERROR: build failed"
    exit 1
fi

echo
echo "=== Running: ./uft $* ==="
export ASAN_OPTIONS="halt_on_error=1:abort_on_error=1:print_stats=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

./uft "$@"
rc=$?

echo
if [[ $rc -eq 0 ]]; then
    echo "=== Completed cleanly (exit 0) ==="
else
    echo "=== Exit code: $rc (check output above for ASan/UBSan findings) ==="
fi

exit $rc
