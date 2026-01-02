# Contributing to UnifiedFloppyTool

First off, thanks for taking the time to contribute! ❤️

All types of contributions are encouraged and valued.

---

## 🐛 Reporting Bugs

Before submitting a bug report:
- Use the latest version
- Search existing issues
- Collect information (OS, Qt version, compiler, steps to reproduce)

**Submit via:** [GitHub Issues](https://github.com/yourusername/UnifiedFloppyTool/issues)

---

## 💡 Suggesting Enhancements

- Check if it already exists
- Describe the use case
- Explain why it's useful

**Submit via:** [GitHub Issues](https://github.com/yourusername/UnifiedFloppyTool/issues)

---

## 🔧 Development Workflow

1. Fork the repository
2. Create branch: `git checkout -b feature/my-feature`
3. Make changes
4. Run tests: `./run_tests.sh`
5. Commit: `git commit -m 'Add feature'`
6. Push: `git push origin feature/my-feature`
7. Create Pull Request

---

## 📝 Code Style

### C Code (Core)
- Linux kernel style
- `snake_case` for functions/variables
- Check malloc() return values

### C++ Code (GUI)
- Qt conventions
- `camelCase` for functions/variables
- `PascalCase` for classes

### General
- 4 spaces (no tabs)
- Max 100 chars per line
- Doxygen comments for public APIs

---

## 🧪 Testing

```bash
# Run all tests
./run_tests.sh

# With sanitizers
ENABLE_ASAN=1 ENABLE_UBSAN=1 ./run_tests.sh

# Unit tests only
cd build && ctest
```

---

## ✅ PR Checklist

- [ ] Tests pass locally
- [ ] ASan/UBSan clean
- [ ] No new clang-tidy warnings
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] Code follows style guidelines

---

**Thank you for contributing!** 🎉
