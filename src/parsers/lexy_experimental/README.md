# Lexy-basierte Parser (Experimental)

Dieses Verzeichnis enthält experimentelle Parser-Implementierungen 
mit der [lexy](https://github.com/foonathan/lexy) Parser-Combinator-Bibliothek.

## Voraussetzungen

- C++17 Compiler
- lexy v2025.05.0 oder neuer

## Installation

```bash
# Als Git Submodule
git submodule add https://github.com/foonathan/lexy.git 3rdparty/lexy

# Oder mit CMake FetchContent (automatisch)
```

## Dateien

- `scp_parser_lexy.hpp` - SCP Format Parser (Pilot)
- `ipf_parser_lexy.hpp` - IPF Container Parser (Planned)
- `sector_parser_lexy.hpp` - Generic Sector Parser (Planned)

## Status

| Parser | Status | Tests |
|--------|--------|-------|
| SCP Header | ✅ Ready | ⏳ |
| SCP Track | ⏳ WIP | ❌ |
| IPF | 📝 Planned | ❌ |
