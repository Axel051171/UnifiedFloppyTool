#!/bin/bash
# UFT macOS Release Package Builder
# Creates a distributable ZIP or DMG
#
# Usage: ./build_macos.sh [version]

set -e

VERSION="${1:-3.7.0}"
PACKAGE_NAME="uft-$VERSION-macos"

echo "════════════════════════════════════════════════════════════"
echo " UFT macOS Release Builder"
echo " Version: $VERSION"
echo "════════════════════════════════════════════════════════════"

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    ARCH_SUFFIX="arm64"
else
    ARCH_SUFFIX="x64"
fi
PACKAGE_NAME="${PACKAGE_NAME}-${ARCH_SUFFIX}"

echo " Architecture: $ARCH_SUFFIX"

# Create build directory
BUILD_DIR="$(pwd)/build"
RELEASE_DIR="$(pwd)/release"
rm -rf "$RELEASE_DIR"
mkdir -p "$RELEASE_DIR"

# Build if needed
if [ ! -d "$BUILD_DIR" ]; then
    echo "Building UFT..."
    cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DUFT_ENABLE_OPENMP=OFF \
        -DCMAKE_OSX_ARCHITECTURES="$ARCH"
    cmake --build "$BUILD_DIR" --parallel
fi

# Check for app bundle or executable
APP_PATH="$BUILD_DIR/UnifiedFloppyTool.app"
EXE_PATH="$BUILD_DIR/UnifiedFloppyTool"

if [ -d "$APP_PATH" ]; then
    echo "Found app bundle, deploying..."
    cp -r "$APP_PATH" "$RELEASE_DIR/"
    
    # Deploy Qt dependencies
    if command -v macdeployqt &> /dev/null; then
        macdeployqt "$RELEASE_DIR/UnifiedFloppyTool.app" -verbose=0
    fi
elif [ -f "$EXE_PATH" ]; then
    echo "Found executable (no GUI)..."
    cp "$EXE_PATH" "$RELEASE_DIR/"
else
    echo "Warning: No executable found"
fi

# Copy documentation
cp README.md "$RELEASE_DIR/"
cp CHANGELOG.md "$RELEASE_DIR/"
[ -f "BUILD_INSTRUCTIONS.md" ] && cp "BUILD_INSTRUCTIONS.md" "$RELEASE_DIR/"

# Create version info
cat > "$RELEASE_DIR/VERSION.txt" << EOF
UnifiedFloppyTool v$VERSION
macOS $ARCH_SUFFIX Release

Build Date: $(date '+%Y-%m-%d %H:%M:%S')
Build Type: Release
Minimum macOS: 11.0 (Big Sur)

For installation instructions, see README.md
For changes in this version, see CHANGELOG.md

Note: This build is for $ARCH_SUFFIX architecture only.
EOF

# Create ZIP archive
echo "Creating ZIP archive..."
ZIP_PATH="${PACKAGE_NAME}.zip"
rm -f "$ZIP_PATH"
cd "$RELEASE_DIR"
zip -r "../$ZIP_PATH" .
cd ..

# Calculate size
ZIP_SIZE=$(du -h "$ZIP_PATH" | cut -f1)
FILE_COUNT=$(unzip -l "$ZIP_PATH" | tail -1 | awk '{print $2}')

echo ""
echo "════════════════════════════════════════════════════════════"
echo " Package created: $ZIP_PATH"
echo " Size: $ZIP_SIZE"
echo " Files: $FILE_COUNT"
echo "════════════════════════════════════════════════════════════"

# Optional: Create DMG
if command -v hdiutil &> /dev/null && [ -d "$RELEASE_DIR/UnifiedFloppyTool.app" ]; then
    echo ""
    echo "Creating DMG..."
    DMG_PATH="${PACKAGE_NAME}.dmg"
    rm -f "$DMG_PATH"
    hdiutil create -volname "UnifiedFloppyTool" \
        -srcfolder "$RELEASE_DIR" \
        -ov -format UDZO \
        "$DMG_PATH"
    echo " DMG created: $DMG_PATH"
fi

# Cleanup
rm -rf "$RELEASE_DIR"

echo ""
echo "Done!"
