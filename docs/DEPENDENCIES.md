# UFT Dependencies

**Version:** 1.0

---

## 1. Required Dependencies

### 1.1 Build System

```
cmake >= 3.20
  - Modern CMake features (targets, generators)
  - FetchContent for vendored dependencies
  - CTest integration
```

### 1.2 Core Libraries

```
libusb >= 1.0.26
  - USB device access
  - Required for hardware controllers
  - Platform: Linux, macOS, Windows
  
  Installation:
    Ubuntu: libusb-1.0-0-dev
    macOS:  brew install libusb
    Windows: vcpkg install libusb:x64-windows
```

### 1.3 GUI (Optional)

```
Qt >= 6.5 (or Qt 5.15 LTS)
  - Widgets, Core, Concurrent
  - Required for GUI application
  
  Installation:
    Ubuntu: qt6-base-dev qt6-tools-dev
    macOS:  brew install qt@6
    Windows: Qt Online Installer or vcpkg
```

---

## 2. Optional Dependencies

### 2.1 Compression

```
zlib >= 1.2.11
  - For ADZ, compressed formats
  - Usually system-provided
```

### 2.2 Parallelization

```
OpenMP >= 4.5
  - Parallel decoding
  - Significant speedup on multi-core
```

### 2.3 Testing

```
Google Test (vendored)
  - Unit testing framework
  - Fetched via CMake FetchContent
```

---

## 3. Vendored Dependencies

Located in `third_party/`:

```
third_party/
├── crc32/           # CRC32 implementation
├── miniz/           # Minimal zlib (fallback)
└── stb/             # stb_image for screenshots
```

---

## 4. Version Locking

### 4.1 CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)
```

### 4.2 vcpkg.json (Windows)

```json
{
  "name": "uft",
  "version": "2.9.0",
  "dependencies": [
    "libusb",
    {
      "name": "qt6",
      "features": ["widgets"]
    }
  ]
}
```

---

## 5. Reproducible Builds

### 5.1 Lock File

```
# dependencies.lock
libusb=1.0.27
qt=6.6.1
zlib=1.3
```

### 5.2 Hash Verification

All vendored sources include SHA256 hashes in CMake.

---

**DOKUMENT ENDE**
