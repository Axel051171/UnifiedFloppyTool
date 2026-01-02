#!/bin/bash
# UFT v3.1 - Build Test Script

set -e  # Exit on error

echo "========================================="
echo "  UnifiedFloppyTool v3.1 - Build Test"
echo "========================================="
echo ""

# Check for Qt
echo "[1/5] Checking Qt installation..."
if command -v qmake >/dev/null 2>&1; then
    QMAKE_VERSION=$(qmake -version | grep "Using Qt version" | cut -d' ' -f4)
    echo "✓ Found Qt $QMAKE_VERSION"
else
    echo "✗ qmake not found! Please install Qt5 or Qt6"
    exit 1
fi

# Check for GCC/Clang
echo ""
echo "[2/5] Checking C/C++ compiler..."
if command -v g++ >/dev/null 2>&1; then
    GCC_VERSION=$(g++ --version | head -1)
    echo "✓ Found $GCC_VERSION"
elif command -v clang++ >/dev/null 2>&1; then
    CLANG_VERSION=$(clang++ --version | head -1)
    echo "✓ Found $CLANG_VERSION"
else
    echo "✗ No C++ compiler found!"
    exit 1
fi

# Generate Makefile
echo ""
echo "[3/5] Generating Makefile..."
qmake UnifiedFloppyTool.pro
if [ $? -eq 0 ]; then
    echo "✓ Makefile generated successfully"
else
    echo "✗ qmake failed!"
    exit 1
fi

# Compile
echo ""
echo "[4/5] Compiling project..."
make -j$(nproc)
if [ $? -eq 0 ]; then
    echo "✓ Compilation successful!"
else
    echo "✗ Compilation failed!"
    exit 1
fi

# Check binary
echo ""
echo "[5/5] Checking binary..."
if [ -f "UnifiedFloppyTool_debug" ] || [ -f "UnifiedFloppyTool" ]; then
    ls -lh UnifiedFloppyTool* 2>/dev/null || true
    echo "✓ Binary created successfully!"
else
    echo "✗ Binary not found!"
    exit 1
fi

echo ""
echo "========================================="
echo "  BUILD SUCCESSFUL! ✓"
echo "========================================="
echo ""
echo "Run with:"
echo "  ./UnifiedFloppyTool_debug"
echo ""
echo "Or for release build:"
echo "  qmake CONFIG+=release"
echo "  make"
echo "  ./UnifiedFloppyTool"
echo ""
