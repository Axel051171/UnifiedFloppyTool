# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 3.x.x   | :white_check_mark: |
| < 3.0   | :x:                |

## Reporting a Vulnerability

Please report security vulnerabilities via GitHub Security Advisories.

Do NOT create public issues for security vulnerabilities.

## Security Considerations

UFT processes potentially untrusted disk image files. The following measures are in place:

- Bounds checking on all buffer operations
- Integer overflow protection
- Input validation for all file formats
- No execution of embedded code from disk images
