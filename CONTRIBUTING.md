# Contributing to UnifiedFloppyTool

Thank you for your interest in contributing!

## Code Style

- C11 standard
- 4-space indentation
- `snake_case` for functions and variables
- `UPPER_CASE` for constants and macros
- Prefix public API with `uft_`

## Pull Request Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Run tests (`./scripts/validate.sh`)
4. Commit changes (`git commit -am 'Add feature'`)
5. Push to branch (`git push origin feature/my-feature`)
6. Open Pull Request

## Testing

```bash
mkdir build && cd build
cmake -DUFT_BUILD_TESTS=ON ..
cmake --build .
ctest --output-on-failure
```

## Reporting Issues

Please include:
- UFT version
- Operating system
- Steps to reproduce
- Expected vs actual behavior
