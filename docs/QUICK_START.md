# UFT Quick Start Guide

## 🚀 5-Minuten Setup

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake qt6-base-dev libusb-1.0-0-dev

# macOS
brew install cmake qt6 libusb

# Windows (MSYS2)
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-qt6-base
```

### Build

```bash
git clone https://github.com/your-repo/UnifiedFloppyTool.git
cd UnifiedFloppyTool
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run

```bash
./UnifiedFloppyTool              # GUI
```

---

## 📁 Project Structure

```
UnifiedFloppyTool/
├── include/uft/           # Public headers
│   ├── core/              # Core types & errors
│   ├── formats/           # Format-specific headers
│   ├── hal/               # Hardware abstraction
│   └── compat/            # Cross-platform helpers
├── src/                   # Implementation
│   ├── core/              # Core logic
│   ├── formats/           # Format parsers
│   ├── decoders/          # MFM/GCR/FM decoders
│   ├── hal/               # Hardware drivers
│   └── gui/               # Qt6 UI
├── tests/                 # Test suite
│   ├── fuzz/              # Fuzz tests
│   └── bench/             # Benchmarks
└── docs/                  # Documentation
```

---

## 🔧 Common Tasks

### Read a disk image

```c
#include <uft/uft.h>

uft_image_t *img = uft_image_open("game.d64", NULL);
if (!img) {
    printf("Error: %s\n", uft_get_error_msg());
    return 1;
}

// Access sectors
uft_sector_t *sector = uft_image_read_sector(img, 18, 0, 0);
// ... use sector ...

uft_image_close(img);
```

### Convert between formats

```c
uft_convert_options_t opts = uft_convert_default_options();
uft_convert_result_t result;

uft_error_t err = uft_convert_file("input.g64", "output.d64", 
                                    UFT_FMT_AUTO, UFT_FMT_D64,
                                    &opts, &result);
if (err != UFT_OK) {
    printf("Convert failed: %s\n", uft_error_desc(err));
}
```

### Detect format automatically

```c
uft_format_t fmt;
int confidence;

uft_error_t err = uft_format_detect("unknown.bin", &fmt, &confidence);
if (err == UFT_OK && confidence > 80) {
    printf("Detected: %s (%d%% confidence)\n", 
           uft_format_name(fmt), confidence);
}
```

---

## 📖 Key Documentation

| Doc | Purpose |
|-----|---------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | System overview |
| [API_REFERENCE.md](API_REFERENCE.md) | Full API docs |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | Code guidelines |
| [CODING_STANDARD.md](CODING_STANDARD.md) | Style guide |

---

## 🧪 Testing

```bash
# Unit tests
cd build && ctest --output-on-failure

# Fuzz tests
./tests/fuzz/run_fuzz.sh d64 libfuzzer 60

# Benchmarks
./tests/bench/uft_benchmark 1000
```

---

## 🐛 Debugging Tips

### Enable verbose logging

```bash
export UFT_LOG_LEVEL=debug
./UnifiedFloppyTool
```

### Build with sanitizers

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_C_FLAGS="-fsanitize=address,undefined"
```

### Check for memory leaks

```bash
valgrind --leak-check=full ./UnifiedFloppyTool
```

---

## ❓ Getting Help

- Check [TODO.md](../TODO.md) for known issues
- Read error messages - they include actionable suggestions
- Open an issue with your platform info and error output

---

*"Bei uns geht kein Bit verloren"* 🖴
