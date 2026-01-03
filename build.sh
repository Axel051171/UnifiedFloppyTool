#!/bin/bash
#
# build.sh - UnifiedFloppyTool Build Script
#
set -e

echo "================================"
echo "  UnifiedFloppyTool Builder"
echo "================================"
echo ""

# Check cmake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: cmake not found!"
    echo "Install: sudo apt install cmake build-essential"
    exit 1
fi

# Clean and build
rm -rf build
mkdir -p build
cd build

echo "Configuring..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DUFT_BUILD_GUI=ON -DUFT_BUILD_EXAMPLES=OFF

echo ""
echo "Building..."
cmake --build . --parallel $(nproc 2>/dev/null || echo 4)

echo ""
echo "================================"
echo "  BUILD COMPLETE!"
echo "================================"
echo ""
echo "Run: ./build/UnifiedFloppyTool"
echo ""
