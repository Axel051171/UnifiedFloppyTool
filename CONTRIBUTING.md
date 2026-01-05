# Contributing to UnifiedFloppyTool

Thank you for your interest in contributing to UFT! This document provides guidelines for contributing.

## Code of Conduct

Be respectful, constructive, and professional in all interactions.

## How to Contribute

### Reporting Bugs

1. Check existing [Issues](https://github.com/Axel051171/UnifiedFloppyTool/issues) first
2. Use the bug report template
3. Include:
   - UFT version
   - Operating system
   - Steps to reproduce
   - Expected vs actual behavior
   - Sample files (if applicable)

### Suggesting Features

1. Open an issue with `[Feature]` prefix
2. Describe the use case
3. Explain expected behavior

### Pull Requests

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make changes following the coding style
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit PR against `develop` branch

## Coding Style

### C Code

```c
/* Function documentation */
int uft_function_name(const char *param, size_t size) {
    int result = 0;
    
    /* Validate inputs */
    if (!param || size == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Implementation */
    for (size_t i = 0; i < size; i++) {
        /* ... */
    }
    
    return result;
}
```

**Rules:**
- 4 spaces indentation (no tabs)
- K&R brace style
- `uft_` prefix for all public functions
- `snake_case` for functions and variables
- `UPPER_CASE` for constants and macros
- Check all return values
- Free all allocations

### C++ / Qt Code

```cpp
void UftClassName::methodName(const QString &param)
{
    if (param.isEmpty()) {
        return;
    }
    
    // Implementation
    m_memberVariable = param;
    emit signalName(param);
}
```

**Rules:**
- 4 spaces indentation
- Allman brace style for classes
- `m_` prefix for member variables
- `CamelCase` for classes
- `camelCase` for methods
- Use Qt containers (QVector, QString, etc.)

### Commit Messages

Use conventional commits:

```
type(scope): short description

Longer description if needed.

Fixes #123
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Formatting
- `refactor`: Code restructuring
- `test`: Adding tests
- `chore`: Maintenance

### Branch Naming

- `feature/description` - New features
- `fix/issue-number` - Bug fixes
- `docs/description` - Documentation
- `refactor/description` - Refactoring

## Testing Requirements

**All PRs must include tests:**

1. **Unit tests** for new functions
2. **Parser tests** for format changes
3. **Regression tests** if fixing a bug

Run tests before submitting:

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
cd build && ctest --output-on-failure
```

## Review Process

1. All PRs require at least one review
2. CI must pass (build + tests)
3. No new warnings allowed
4. Documentation must be updated

## Development Setup

```bash
# Clone
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool

# Build with debug
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run tests
cd build && ctest

# Run with sanitizers
cmake -B build-asan -DSANITIZER=address
cmake --build build-asan
```

## Questions?

- Open a [Discussion](https://github.com/Axel051171/UnifiedFloppyTool/discussions)
- Check the [Documentation](docs/)

---

Thank you for contributing to digital preservation!
