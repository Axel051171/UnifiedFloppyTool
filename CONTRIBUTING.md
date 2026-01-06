# Contributing to UnifiedFloppyTool

Thank you for your interest in contributing to UFT!

## Build Requirements

- CMake 3.16+
- Qt 6.6+
- C11/C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)

## Building

```bash
cmake -B build -G Ninja
cmake --build build
```

## Code Style

- C11 for core modules
- C++17 for Qt GUI
- `uft_` prefix for all public symbols
- 4 spaces indentation

## Testing

Run tests with:
```bash
cd build && ctest
```

## Pull Request Guidelines

1. Fork the repository
2. Create a feature branch
3. Write tests for new functionality
4. Ensure CI passes on all platforms
5. Submit PR with clear description
