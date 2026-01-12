# Contributing to UnifiedFloppyTool

Thank you for your interest in contributing to UnifiedFloppyTool!

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/UnifiedFloppyTool.git`
3. Create a feature branch: `git checkout -b feature/my-feature`
4. Make your changes
5. Test on at least one platform (Linux, Windows, or macOS)
6. Commit with clear messages: `git commit -m "Add: new feature description"`
7. Push and create a Pull Request

## Code Style

- Use C++17 features where appropriate
- Follow Qt naming conventions (camelCase for methods, m_ prefix for members)
- Keep functions focused and under 50 lines where possible
- Add comments for complex algorithms

## Commit Message Format

```
Type: Short description

Longer description if needed.

Fixes #123
```

Types: `Add`, `Fix`, `Update`, `Remove`, `Refactor`, `Docs`

## Adding New Format Support

1. Create format files in `src/formats/`
2. Register in the format registry
3. Add tests
4. Document in README.md

## Reporting Issues

- Use the issue templates
- Include version, OS, and hardware info
- Attach log files if available
- Provide steps to reproduce

## Questions?

Open a Discussion or Issue - we're happy to help!
