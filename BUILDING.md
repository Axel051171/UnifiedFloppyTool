# Building UnifiedFloppyTool

## Requirements

| | |
|---|---|
| Qt | 6.5 or newer — Core, Widgets, SerialPort (CI builds 6.7.3 and 6.10.1) |
| Compiler | C++17 or C++20: GCC 13+, Clang 15+, MinGW 13 (Windows) |
| libusb | 1.0 — for SCP-Direct and XUM1541 hardware support |
| Python | 3 — qmake runs `scripts/generate_version_header.py` |

## Linux (Ubuntu/Debian)

```bash
sudo apt install build-essential qt6-base-dev qt6-tools-dev \
    libqt6serialport6-dev libusb-1.0-0-dev libgl1-mesa-dev python3
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j"$(nproc)"
```

## macOS

```bash
brew install qt@6 libusb
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release CONFIG+=cxx20
make -j"$(sysctl -n hw.ncpu)"
```

## Windows (MinGW)

Install Qt 6 with the MinGW kit from qt.io, then in a Qt MinGW shell:

```bat
mkdir build && cd build
qmake ..\UnifiedFloppyTool.pro CONFIG+=release CONFIG+=cxx20
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

`CONFIG += object_parallel_to_source` is set in the `.pro` file and is
required: several source files share a basename in different directories.

## Linux device access

```bash
sudo cp tools/99-floppy-devices.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```
