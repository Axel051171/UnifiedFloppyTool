# Contributing to UnifiedFloppyTool

Thank you for your interest in contributing to UnifiedFloppyTool! This document provides guidelines and instructions for contributing.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Contribution Workflow](#contribution-workflow)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Documentation](#documentation)

## 🤝 Code of Conduct

We are committed to providing a welcoming and inclusive environment. Please:

- Be respectful and constructive
- Welcome newcomers and help them get started
- Focus on what is best for the community
- Show empathy towards other community members

## 🚀 Getting Started

### Prerequisites

- Qt 5.12+ or Qt 6.x
- C++ compiler (GCC 7+, Clang 6+, MSVC 2017+)
- CMake 3.20+ or qmake
- Git

### Fork and Clone

```bash
# Fork the repository on GitHub
# Clone your fork
git clone https://github.com/YOUR_USERNAME/UnifiedFloppyTool.git
cd UnifiedFloppyTool

# Add upstream remote
git remote add upstream https://github.com/ORIGINAL_OWNER/UnifiedFloppyTool.git
```

## 💻 Development Setup

### Build with CMake

```bash
mkdir build && cd build
cmake .. -DUFT_ENABLE_SIMD=ON -DUFT_ENABLE_OPENMP=ON
make
```

### Build with qmake

```bash
qmake UnifiedFloppyTool.pro
make
```

### Qt Creator

1. Open `UnifiedFloppyTool.pro` in Qt Creator
2. Configure project for your kit
3. Build → Run

## 🔄 Contribution Workflow

### 1. Create a Branch

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/bug-description
```

### 2. Make Changes

- Write clear, concise code
- Follow coding standards
- Add tests for new features
- Update documentation

### 3. Commit

```bash
git add .
git commit -m "Add feature: brief description

- Detailed explanation
- Why this change is needed
- Any breaking changes"
```

**Commit Message Format:**
```
<type>: <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Formatting
- `refactor`: Code restructuring
- `test`: Tests
- `chore`: Maintenance

### 4. Push and Pull Request

```bash
git push origin feature/your-feature-name
```

Then create a Pull Request on GitHub with:
- Clear title
- Description of changes
- Related issue numbers
- Screenshots (if UI changes)

## 📝 Coding Standards

### C++ Code

```cpp
// Use descriptive names
class FluxAnalyzer {
public:
    // Public methods first
    bool analyzeTrack(int track);
    
private:
    // Private methods
    void processFluxData();
    
    // Member variables with m_ prefix
    int m_trackCount;
    QVector<FluxSample> m_samples;
};

// Constants in UPPER_CASE
const int MAX_RETRIES = 10;

// Use nullptr, not NULL
Widget* widget = nullptr;
```

### C Code

```c
// Use snake_case for functions
int uft_decode_mfm(const uint8_t* data, size_t len);

// Structs with _t suffix
typedef struct flux_sample_t {
    uint32_t timing;
    uint8_t index;
} flux_sample_t;

// Use stdint.h types
uint32_t timing;
int32_t offset;
```

### Qt/UI Code

- Use Qt Designer for UI files
- Connect signals/slots properly
- Use `Q_OBJECT` macro for QObject-derived classes
- Use Qt containers (QVector, QString, etc.)

## 🧪 Testing

### Unit Tests (Future)

```bash
cd build
ctest --output-on-failure
```

### Manual Testing

1. Test with real hardware (if available)
2. Test all disk formats
3. Test edge cases
4. Test on different platforms

## 📚 Documentation

### Code Documentation

```cpp
/**
 * @brief Decodes MFM data to raw bytes
 * @param data Input MFM-encoded data
 * @param length Length of input data
 * @return Vector of decoded bytes
 * 
 * This function decodes Modified Frequency Modulation (MFM)
 * encoded data using optimized algorithms. SIMD is used if available.
 */
QVector<uint8_t> decodeMFM(const uint8_t* data, size_t length);
```

### User Documentation

- Update README.md if needed
- Add examples to docs/
- Update CHANGELOG.md

## 🎯 Areas to Contribute

### High Priority

- [ ] Hardware communication backend
- [ ] Format detection algorithms
- [ ] Protection detection implementation
- [ ] Batch processing engine
- [ ] Format converter backend

### Medium Priority

- [ ] Additional disk format support
- [ ] Performance optimizations
- [ ] Extended SIMD support
- [ ] Cross-platform testing

### Low Priority

- [ ] UI polish
- [ ] Extended documentation
- [ ] Translation support
- [ ] Additional utilities

## 🐛 Bug Reports

### Good Bug Report Includes:

- Clear title
- Steps to reproduce
- Expected behavior
- Actual behavior
- System information (OS, Qt version, etc.)
- Screenshots/logs if applicable

### Example:

```markdown
**Bug: D64 files not loading on Windows**

**Steps to reproduce:**
1. Open UnifiedFloppyTool on Windows 10
2. Go to Tab 1: Workflow
3. Select Source → File
4. Choose a D64 file

**Expected:** File loads successfully
**Actual:** Error dialog appears

**Environment:**
- Windows 10 Pro 64-bit
- Qt 6.5.0
- UnifiedFloppyTool v3.1.0

**Log:**
```
Error: Cannot read D64 file
...
```
```

## 💡 Feature Requests

### Good Feature Request Includes:

- Clear use case
- Why it's needed
- How it should work
- Examples if possible

## 📧 Questions?

- Open a Discussion on GitHub
- Join our Discord (if available)
- Email maintainers

## 🙏 Thank You!

Every contribution, no matter how small, is valuable. Thank you for helping make UnifiedFloppyTool better!

---

**Happy Contributing! 🚀**
