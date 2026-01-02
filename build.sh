#!/bin/bash
# UnifiedFloppyTool build script

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Functions
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Parse arguments
BUILD_TYPE="Release"
ENABLE_SIMD="ON"
ENABLE_OPENMP="ON"
ENABLE_LTO="OFF"
ENABLE_ASAN="OFF"

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-simd)
            ENABLE_SIMD="OFF"
            shift
            ;;
        --no-openmp)
            ENABLE_OPENMP="OFF"
            shift
            ;;
        --lto)
            ENABLE_LTO="ON"
            shift
            ;;
        --asan)
            ENABLE_ASAN="ON"
            shift
            ;;
        --clean)
            print_info "Cleaning build directory..."
            rm -rf "$BUILD_DIR"
            exit 0
            ;;
        --help)
            echo "UnifiedFloppyTool Build Script"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --debug      Build in Debug mode (default: Release)"
            echo "  --no-simd    Disable SIMD optimizations"
            echo "  --no-openmp  Disable OpenMP"
            echo "  --lto        Enable Link Time Optimization"
            echo "  --asan       Enable Address Sanitizer"
            echo "  --clean      Clean build directory"
            echo "  --help       Show this help"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Check for Qt
print_info "Checking for Qt..."
if command -v qmake &> /dev/null; then
    QT_VERSION=$(qmake -query QT_VERSION)
    print_info "Found Qt $QT_VERSION"
else
    print_error "Qt not found. Please install Qt 5.12+ or Qt 6.x"
    exit 1
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
print_info "Configuring CMake..."
print_info "  Build Type: $BUILD_TYPE"
print_info "  SIMD: $ENABLE_SIMD"
print_info "  OpenMP: $ENABLE_OPENMP"
print_info "  LTO: $ENABLE_LTO"
print_info "  Address Sanitizer: $ENABLE_ASAN"

cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DUFT_ENABLE_SIMD="$ENABLE_SIMD" \
    -DUFT_ENABLE_OPENMP="$ENABLE_OPENMP" \
    -DUFT_ENABLE_LTO="$ENABLE_LTO" \
    -DUFT_ENABLE_ASAN="$ENABLE_ASAN"

# Build
print_info "Building..."
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build . -j$NPROC

# Success
print_info "Build complete!"
print_info "Binary: $BUILD_DIR/UnifiedFloppyTool"
print_info ""
print_info "Run with: $BUILD_DIR/UnifiedFloppyTool"
