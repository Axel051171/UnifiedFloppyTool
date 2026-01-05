#!/bin/bash
# Build .deb package for UFT

set -e

VERSION="${1:-3.3.0}"
ARCH="${2:-amd64}"
BUILD_DIR="build-deb"
PKG_NAME="uft-${VERSION}-linux-${ARCH}"

echo "Building UFT ${VERSION} .deb package..."

# Create directory structure
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}/DEBIAN"
mkdir -p "${BUILD_DIR}/usr/bin"
mkdir -p "${BUILD_DIR}/usr/lib"
mkdir -p "${BUILD_DIR}/usr/share/doc/uft"
mkdir -p "${BUILD_DIR}/usr/share/applications"
mkdir -p "${BUILD_DIR}/usr/share/icons/hicolor/256x256/apps"

# Copy binaries
cp build/uft "${BUILD_DIR}/usr/bin/" || echo "Warning: uft binary not found"
cp build/lib*.so* "${BUILD_DIR}/usr/lib/" 2>/dev/null || true

# Copy docs
cp README.md LICENSE CHANGELOG.md "${BUILD_DIR}/usr/share/doc/uft/"

# Copy control file
sed "s/Version: .*/Version: ${VERSION}/" packaging/deb/control > "${BUILD_DIR}/DEBIAN/control"

# Build package
dpkg-deb --build "${BUILD_DIR}" "dist/${PKG_NAME}.deb"

echo "Package created: dist/${PKG_NAME}.deb"
