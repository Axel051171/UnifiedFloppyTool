# UFT Platform Support Matrix

**Version:** 1.0  
**Status:** VERBINDLICH

---

## 1. Support-Tiers

| Tier | Definition | CI | Release Binary | Support |
|------|------------|----|--------------------|---------|
| **Tier 1** | Primäre Plattformen | ✅ Jeder Commit | ✅ Ja | Voller Support |
| **Tier 2** | Sekundäre Plattformen | ✅ Nightly | ❌ Nein | Community |
| **Tier 3** | Best-Effort | ❌ Manuell | ❌ Nein | Keine Garantie |

---

## 2. Plattform-Matrix

### 2.1 Linux

| Distribution | Arch | Compiler | Tier | Notes |
|--------------|------|----------|------|-------|
| Ubuntu 22.04 LTS | x86_64 | GCC 11+ | **Tier 1** | Primäre Dev-Plattform |
| Ubuntu 24.04 LTS | x86_64 | GCC 13+ | **Tier 1** | |
| Ubuntu 22.04 | ARM64 | GCC 11+ | Tier 2 | Raspberry Pi 4+ |
| Debian 12 | x86_64 | GCC 12+ | Tier 2 | |
| Fedora 39+ | x86_64 | GCC 13+ | Tier 2 | |
| Arch Linux | x86_64 | GCC/Clang | Tier 3 | Rolling Release |

### 2.2 macOS

| Version | Arch | Compiler | Tier | Notes |
|---------|------|----------|------|-------|
| macOS 13 (Ventura) | x86_64 | Clang 14+ | **Tier 1** | |
| macOS 14 (Sonoma) | ARM64 | Clang 15+ | **Tier 1** | Apple Silicon |
| macOS 12 (Monterey) | x86_64 | Clang 14+ | Tier 2 | |

### 2.3 Windows

| Version | Arch | Compiler | Tier | Notes |
|---------|------|----------|------|-------|
| Windows 11 | x64 | MSVC 2022 | **Tier 1** | |
| Windows 10 (21H2+) | x64 | MSVC 2022 | **Tier 1** | |
| Windows 11 | x64 | MinGW-w64 12+ | Tier 2 | |
| Windows 10 | x64 | MinGW-w64 12+ | Tier 2 | |
| Windows 11 | ARM64 | MSVC 2022 | Tier 3 | Experimentell |

---

## 3. Compiler-Anforderungen

### 3.1 Minimum Versionen

| Compiler | Minimum | Empfohlen | Standard |
|----------|---------|-----------|----------|
| GCC | 11.0 | 13.0+ | C17, C++20 |
| Clang | 14.0 | 16.0+ | C17, C++20 |
| MSVC | 19.30 (VS 2022) | 19.35+ | C17, C++20 |

### 3.2 Compiler-Flags

```cmake
# CMakeLists.txt

# C Standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# C++ Standard (für GUI)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wformat=2 -Wformat-security
        -Wnull-dereference
        -Wstack-protector
        -fstack-protector-strong
    )
elseif(MSVC)
    add_compile_options(
        /W4 /WX
        /analyze
        /guard:cf
        /sdl
    )
endif()
```

---

## 4. Dependencies

### 4.1 Required Dependencies

| Library | Version | Purpose | Platform |
|---------|---------|---------|----------|
| CMake | ≥ 3.20 | Build System | All |
| Qt | 6.5+ (oder 5.15) | GUI | All |
| libusb | 1.0.26+ | USB Access | All |

### 4.2 Optional Dependencies

| Library | Version | Purpose | Platform |
|---------|---------|---------|----------|
| OpenMP | 4.5+ | Parallelization | All |
| zlib | 1.2.11+ | Compression | All |
| libfuzzer | - | Fuzzing | Linux/macOS |

### 4.3 Dependency Installation

```bash
# Ubuntu/Debian
sudo apt install cmake ninja-build libusb-1.0-0-dev qt6-base-dev

# macOS
brew install cmake ninja libusb qt@6

# Windows (vcpkg)
vcpkg install libusb:x64-windows qt6:x64-windows
```

---

## 5. Build-Konfigurationen

### 5.1 Standard-Builds

| Config | Verwendung | Flags |
|--------|------------|-------|
| Debug | Entwicklung | `-O0 -g -DDEBUG` |
| Release | Distribution | `-O3 -DNDEBUG` |
| RelWithDebInfo | Profiling | `-O2 -g -DNDEBUG` |
| MinSizeRel | Embedded | `-Os -DNDEBUG` |

### 5.2 Spezielle Builds

```cmake
# Sanitizer-Build
cmake -B build -DSANITIZE=address,undefined

# Fuzzing-Build
cmake -B build -DFUZZ=ON

# Static-Build (für Distribution)
cmake -B build -DBUILD_SHARED_LIBS=OFF -DSTATIC_RUNTIME=ON

# GUI-less Build (CLI only)
cmake -B build -DBUILD_GUI=OFF
```

---

## 6. Known Issues

### 6.1 Linux

- **AppArmor:** Kann USB-Zugriff blockieren
  ```bash
  sudo aa-complain /usr/bin/uft
  ```

### 6.2 macOS

- **Gatekeeper:** Unsigned Binary wird blockiert
  ```bash
  xattr -d com.apple.quarantine /Applications/UFT.app
  ```

### 6.3 Windows

- **USB Driver:** WinUSB/libusb-win32 erforderlich für manche Controller
- **Long Paths:** `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled` = 1

---

## 7. CI Matrix

```yaml
# .github/workflows/ci.yml
jobs:
  build:
    strategy:
      matrix:
        include:
          # Tier 1
          - os: ubuntu-22.04
            compiler: gcc-11
            tier: 1
          - os: ubuntu-24.04
            compiler: gcc-13
            tier: 1
          - os: macos-13
            compiler: clang
            tier: 1
          - os: macos-14
            compiler: clang
            tier: 1
          - os: windows-2022
            compiler: msvc
            tier: 1
          
          # Tier 2
          - os: ubuntu-22.04
            compiler: clang-14
            tier: 2
          - os: windows-2022
            compiler: mingw
            tier: 2
```

---

**DOKUMENT ENDE**
