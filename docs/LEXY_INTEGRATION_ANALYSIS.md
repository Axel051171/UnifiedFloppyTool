# Lexy Integration Analysis für UnifiedFloppyTool

## Was ist Lexy?

**lexy** ist eine C++17 Parser-Combinator-Bibliothek mit folgenden Features:
- Pure C++ DSL (keine externe Grammatik-Datei)
- Explizite Backtracking-Kontrolle (kein implizites Raten)
- Header-only, minimale Dependencies
- **Binary Parsing**: Bytes, Bits, TLV-Formate
- Error Recovery mit detaillierten Fehlermeldungen
- Constexpr-fähig (Compile-Time Parsing möglich)

---

## Relevanz für UFT

### ✅ HOHE RELEVANZ

| Feature | UFT Anwendung | Priorität |
|---------|---------------|-----------|
| **Binary Parsing** | Disk-Header, Sektoren, Format-Strukturen | P1 |
| **Bit-Level Rules** | MFM/GCR Bit-Pattern Matching | P1 |
| **TLV Parsing** | IPF, SCP, A2R Container-Formate | P1 |
| **Error Recovery** | Defekte Sektoren/Tracks parsen | P2 |
| **Big/Little Endian** | Cross-Platform Format-Kompatibilität | P2 |

### ⚠️ MITTLERE RELEVANZ

| Feature | UFT Anwendung | Priorität |
|---------|---------------|-----------|
| Text/Unicode | Dateinamen in Disk-Images | P3 |
| Operator Parsing | CLI-Expressions | P3 |
| Tracing | Debug-Output für Parser | P3 |

---

## Konkrete Integrationspunkte

### 1. SCP Format Parser (Pilot-Projekt)

**Aktuell**: Manueller Parser mit memcpy und Offset-Berechnungen

**Mit Lexy**:
```cpp
#include <lexy/dsl.hpp>
namespace dsl = lexy::dsl;

struct scp_header {
    static constexpr auto rule = [] {
        auto magic = dsl::lit_b<'S', 'C', 'P'>;
        auto version = dsl::integer<uint8_t>;
        auto disk_type = dsl::integer<uint8_t>;
        auto revolutions = dsl::integer<uint8_t>;
        auto start_track = dsl::integer<uint8_t>;
        auto end_track = dsl::integer<uint8_t>;
        auto flags = dsl::integer<uint8_t>;
        auto encoding = dsl::integer<uint8_t>;
        auto heads = dsl::integer<uint8_t>;
        auto resolution = dsl::integer<uint8_t>;
        auto checksum = dsl::little_endian<dsl::integer<uint32_t>>;
        
        return magic >> version + disk_type + revolutions +
               start_track + end_track + flags +
               encoding + heads + resolution + checksum;
    }();
    
    static constexpr auto value = lexy::construct<SCP_Header>;
};
```

### 2. IPF Container (TLV-Format)

```cpp
struct ipf_record {
    static constexpr auto rule = [] {
        auto type = dsl::bytes<4>;           // Record type
        auto length = dsl::big_endian<uint32_t>;  // Length
        auto crc = dsl::big_endian<uint32_t>;     // CRC
        auto data = dsl::blob(length);       // Data (TLV nativ!)
        
        return type + length + crc + data;
    }();
};
```

### 3. MFM Sync-Pattern

```cpp
struct mfm_sync {
    // MFM Sync: 0x4489 als Bit-Pattern
    static constexpr auto rule = 
        dsl::lit_b<0x44, 0x89>;  // Einfacher als manuelle Bit-Shifts
};
```

### 4. Error Recovery

```cpp
struct sector_with_recovery {
    static constexpr auto rule = [] {
        auto header = dsl::p<sector_header>;
        auto data = dsl::bytes<512>;
        auto crc = dsl::integer<uint16_t>;
        
        // Bei Fehler: loggen und weitermachen
        return header + data + 
               dsl::try_(crc, dsl::recover(dsl::bytes<2>));
    }();
};
```

---

## Vorteile vs. Aktuell

| Aspekt | Aktuell (C) | Mit Lexy (C++) |
|--------|-------------|----------------|
| Fehlerbehandlung | Manuell, inkonsistent | Automatisch, strukturiert |
| Endianness | `#ifdef` oder Runtime | Deklarativ |
| Bit-Manipulation | Manuelle Shifts | `dsl::bits<N>` |
| TLV-Formate | Schleifen | `dsl::blob` nativ |
| Wartbarkeit | Schwer lesbar | Selbstdokumentierend |
| Compile-Time | Nicht möglich | Constexpr-Validierung |

---

## Integration

### CMake
```cmake
FetchContent_Declare(lexy
    GIT_REPOSITORY https://github.com/foonathan/lexy.git
    GIT_TAG v2025.05.0)
FetchContent_MakeAvailable(lexy)
target_link_libraries(UFT PRIVATE foonathan::lexy)
```

### QMake
```qmake
INCLUDEPATH += $$PWD/3rdparty/lexy/include
CONFIG += c++17
```

---

## Empfehlung

| Phase | Aufgabe | Aufwand |
|-------|---------|---------|
| **P1** | SCP-Parser als Pilot | M |
| **P2** | IPF, A2R, HFE migrieren | L |
| **P3** | Alle Format-Parser | XL |

**Fazit**: Lexy ist sehr gut geeignet für UFT's Binary-Parsing-Anforderungen. Start mit SCP als Proof-of-Concept empfohlen.
