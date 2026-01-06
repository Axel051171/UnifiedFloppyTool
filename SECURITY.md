# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 3.3.x   | :white_check_mark: |
| 3.2.x   | :white_check_mark: |
| 3.1.x   | :x:                |
| < 3.0   | :x:                |

## Reporting a Vulnerability

We take security vulnerabilities seriously. If you discover a security issue, please report it responsibly.

### How to Report

1. **DO NOT** create a public GitHub issue for security vulnerabilities
2. Email: security@unifiedfloppytool.org (or create a private security advisory on GitHub)
3. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Any suggested fixes (optional)

### What to Expect

- **Response Time**: We aim to respond within 48 hours
- **Fix Timeline**: Critical vulnerabilities will be patched within 7 days
- **Disclosure**: We follow coordinated disclosure (90 days)
- **Credit**: Security researchers will be credited in the changelog (if desired)

## Security Considerations

### Parser Security

UFT parses untrusted disk image files. The following safeguards are implemented:

- **Buffer overflow protection**: All buffer operations are bounds-checked
- **Integer overflow checks**: Size calculations are validated
- **Format string safety**: No user-controlled format strings
- **Memory safety**: Strict ownership model, no dangling pointers
- **I/O validation**: All file operations check return values

### Known Attack Surfaces

1. **Disk Image Parsers**: Malformed disk images could trigger vulnerabilities
   - Mitigation: Fuzzing, bounds checking, sanitizer builds
   
2. **Hardware Interface**: USB device communication
   - Mitigation: libusb sandboxing, device validation

3. **File System Operations**: Reading/writing to disk
   - Mitigation: Path validation, safe file operations

### Security Testing

The project includes:
- 7 Fuzz targets (under `tests/fuzz/`)
- Static analysis with clang-tidy
- AddressSanitizer builds
- UndefinedBehaviorSanitizer builds

### Building with Sanitizers

```bash
# AddressSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=address
cmake --build build

# UndefinedBehaviorSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=undefined
cmake --build build
```

## Security Changelog

### v3.3.0
- Fixed P0-SEC-001: Buffer overflow protection in all parsers
- Fixed P0-SEC-002: Integer overflow checks in size calculations
- Fixed P0-SEC-003: Format string vulnerabilities eliminated
- Fixed P0-SEC-004: Complete memory safety audit

### Previous Versions
See CHANGELOG.md for historical security fixes.
