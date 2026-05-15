#!/usr/bin/env bash
# run_matrix_local.sh — run the CI build matrix locally, skip what host can't.
#
# Usage:
#   bash run_matrix_local.sh --config <name>
#   bash run_matrix_local.sh --all
#
# Configs:
#   linux-c++17     — Qt 6.7.x, gcc, C++17 baseline
#   linux-c++20     — Qt 6.10.x, gcc, C++20
#   macos           — Qt 6.10.x, clang, C++20 (only on macOS)
#   windows-mingw   — Qt 6.10.x, MinGW (only on Windows)
#   windows-msvc    — Qt 6.x, MSVC (only on Windows w/ Visual Studio)
#
# The script picks runnable configs based on `uname -s`. Non-runnable
# entries print "n/a on this host" and are skipped (not failed).

set -u

HOST=$(uname -s)
CONFIGS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --all)            CONFIGS=(linux-c++17 linux-c++20 linux-debian macos windows-mingw windows-msvc); shift ;;
        --config)         CONFIGS+=("$2"); shift 2 ;;
        -h|--help)
            echo "usage: $0 [--all | --config <name>]"
            echo "configs: linux-c++17 linux-c++20 linux-debian macos windows-mingw windows-msvc"
            exit 0
            ;;
        *)                echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

[ ${#CONFIGS[@]} -eq 0 ] && {
    echo "error: pass --all or --config <name>" >&2; exit 2;
}

run_one() {
    local cfg="$1"
    local builddir="build-matrix-$cfg"
    local qmake_args=""
    local make_cmd=""
    local can_run=0

    case "$cfg" in
        linux-c++17)
            [ "$HOST" = "Linux" ] && can_run=1
            qmake_args="CONFIG+=release"
            make_cmd="make -j$(nproc)"
            ;;
        linux-c++20)
            [ "$HOST" = "Linux" ] && can_run=1
            qmake_args="CONFIG+=release CONFIG+=cxx20"
            make_cmd="make -j$(nproc)"
            ;;
        macos)
            [ "$HOST" = "Darwin" ] && can_run=1
            qmake_args="CONFIG+=release CONFIG+=cxx20"
            make_cmd="make -j$(sysctl -n hw.ncpu)"
            ;;
        linux-debian)
            # Debian build runs inside a debian:12 container — host needs
            # docker. We don't try to detect "are we already inside Debian"
            # because the runner script is for parity with CI, not local
            # native builds. If you ARE on Debian, use linux-c++17 directly.
            if [ "$HOST" = "Linux" ] && command -v docker >/dev/null 2>&1; then
                can_run=1
            fi
            qmake_args="CONFIG+=release"   # Debian stable Qt → C++17 baseline
            # Wrap the build inside a debian:12 container with apt Qt
            make_cmd="docker run --rm -v \$(pwd)/..:/uft -w /uft/$builddir debian:12 bash -c '
                apt-get update -q &&
                apt-get install -y -q --no-install-recommends \
                    qt6-base-dev qt6-serialport-dev libusb-1.0-0-dev \
                    build-essential pkg-config &&
                qmake6 $qmake_args ../UnifiedFloppyTool.pro &&
                make -j\$(nproc)
            '"
            ;;
        windows-mingw)
            case "$HOST" in MINGW*|MSYS*|CYGWIN*) can_run=1 ;; esac
            qmake_args="CONFIG+=release CONFIG+=cxx20"
            make_cmd="mingw32-make -j$(nproc 2>/dev/null || echo 4)"
            ;;
        windows-msvc)
            case "$HOST" in MINGW*|MSYS*|CYGWIN*) can_run=1 ;; esac
            qmake_args="-spec win32-msvc CONFIG+=release CONFIG+=cxx20"
            make_cmd="nmake"
            ;;
        *)
            echo "✗ $cfg: unknown config name"
            return 2
            ;;
    esac

    echo
    echo "═══════════════════════════════════════════════════════════════"
    echo "  $cfg"
    echo "═══════════════════════════════════════════════════════════════"

    if [ "$can_run" -eq 0 ]; then
        echo "  n/a on this host ($HOST) — skipped"
        return 0
    fi

    rm -rf "$builddir"
    mkdir -p "$builddir"
    cd "$builddir"

    echo "  qmake $qmake_args ../UnifiedFloppyTool.pro"
    if ! qmake $qmake_args ../UnifiedFloppyTool.pro 2>&1 | tee qmake.log; then
        echo "  ✗ qmake failed for $cfg"
        cd ..
        return 1
    fi

    echo "  $make_cmd"
    if ! eval "$make_cmd" 2>&1 | tee build.log; then
        echo "  ✗ build failed for $cfg"
        echo "  → run: bash <skill>/scripts/classify_build_error.sh $builddir/build.log"
        cd ..
        return 1
    fi

    echo "  ✓ $cfg passed"
    cd ..
    return 0
}

OVERALL=0
RAN=0
SKIPPED=0
FAILED=()

for cfg in "${CONFIGS[@]}"; do
    if run_one "$cfg"; then
        # check whether it actually ran or was skipped
        if [ -d "build-matrix-$cfg" ] && [ -s "build-matrix-$cfg/build.log" ]; then
            RAN=$((RAN + 1))
        else
            SKIPPED=$((SKIPPED + 1))
        fi
    else
        OVERALL=1
        FAILED+=("$cfg")
    fi
done

echo
echo "═══════════════════════════════════════════════════════════════"
echo "  Summary: $RAN ran, $SKIPPED skipped, ${#FAILED[@]} failed"
echo "═══════════════════════════════════════════════════════════════"
[ ${#FAILED[@]} -gt 0 ] && {
    echo "Failed configs:"
    for c in "${FAILED[@]}"; do echo "  - $c"; done
}

exit $OVERALL
