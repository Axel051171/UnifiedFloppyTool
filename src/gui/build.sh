#!/bin/bash
# UFT GUI Build Script
# Requirements: Qt 6.x, CMake 3.20+

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║           UnifiedFloppyTool GUI Build Script                  ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Check Qt
if ! command -v qmake6 &> /dev/null && ! command -v qmake &> /dev/null; then
    echo "❌ Qt not found. Please install Qt 6.x"
    echo "   Ubuntu/Debian: sudo apt install qt6-base-dev"
    echo "   Fedora: sudo dnf install qt6-qtbase-devel"
    echo "   macOS: brew install qt@6"
    exit 1
fi

# Detect qmake
QMAKE=$(command -v qmake6 || command -v qmake)
QT_VERSION=$($QMAKE -query QT_VERSION)
echo "✓ Found Qt $QT_VERSION"

# Build method selection
echo ""
echo "Select build method:"
echo "  1) qmake (recommended for Qt Creator)"
echo "  2) CMake"
read -p "Choice [1]: " BUILD_METHOD
BUILD_METHOD=${BUILD_METHOD:-1}

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [ "$BUILD_METHOD" = "2" ]; then
    echo ""
    echo "Building with CMake..."
    cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
else
    echo ""
    echo "Building with qmake..."
    $QMAKE "$SCRIPT_DIR/uft_gui.pro" CONFIG+=release
    make -j$(nproc)
fi

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                    ✅ Build Complete!                         ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Executable: $BUILD_DIR/uft_gui"
echo ""
echo "Run with: ./uft_gui"
