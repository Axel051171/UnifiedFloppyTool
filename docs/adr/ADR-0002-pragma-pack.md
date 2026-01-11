# ADR-0002: Pragma Pack für Struct-Packing

**Status:** Accepted  
**Datum:** 2025-01-11  
**Autor:** UFT Team

---

## Kontext

Disk-Image-Formate (D64, ADF, HFE, etc.) haben binäre Header mit exaktem 
Byte-Layout. C-Compiler fügen normalerweise Padding zwischen Struct-Membern 
ein, was die Header unbrauchbar macht.

Beispiel HFE-Header (muss exakt 512 Bytes sein):
```c
typedef struct {
    char     signature[8];      // Offset 0
    uint8_t  revision;          // Offset 8
    uint8_t  num_tracks;        // Offset 9
    uint8_t  num_sides;         // Offset 10
    // ... weitere Felder
} hfe_header_t;
```

## Entscheidung

**Wir verwenden `#pragma pack(push, 1)` / `#pragma pack(pop)` für alle 
binären Struktur-Definitionen.**

```c
#pragma pack(push, 1)
typedef struct {
    char     signature[8];
    uint8_t  revision;
    // ...
} hfe_header_t;
#pragma pack(pop)
```

## Begründung

| Ansatz | MSVC | GCC | Clang | Portabel |
|--------|------|-----|-------|----------|
| `__attribute__((packed))` | ❌ | ✅ | ✅ | ❌ |
| `#pragma pack` | ✅ | ✅ | ✅ | ✅ |
| `__declspec(align)` | ✅ | ❌ | ❌ | ❌ |
| Manuelle Serialisierung | ✅ | ✅ | ✅ | ✅ (aber aufwändig) |

**`#pragma pack` ist der einzige Ansatz, der auf allen drei Compilern 
ohne Präprozessor-Bedingungen funktioniert.**

## Konsequenzen

### Positiv ✅
- Ein Code für alle Plattformen
- Keine `#ifdef _MSC_VER` Verzweigungen
- Compiler-Warnung bei vergessenen `pop`

### Negativ ⚠️
- Push/Pop muss immer paarweise sein
- Vergessenes `pop` beeinflusst alle folgenden Structs
- Leichte Performance-Einbuße bei unaligned Access

### Coding-Guideline
```c
// ✅ RICHTIG
#pragma pack(push, 1)
typedef struct { ... } header_t;
#pragma pack(pop)

// ❌ FALSCH - kein pop
#pragma pack(push, 1)
typedef struct { ... } header_t;
// Alle folgenden Structs sind jetzt auch gepackt!
```

## Alternativen (verworfen)

### Makro-Wrapper
```c
#ifdef _MSC_VER
#define UFT_PACKED_BEGIN __pragma(pack(push, 1))
#define UFT_PACKED_END   __pragma(pack(pop))
#else
#define UFT_PACKED_BEGIN
#define UFT_PACKED_END
#define UFT_PACKED __attribute__((packed))
#endif
```
- **Verworfen weil:** Zu komplex, fehleranfällig, schwerer lesbar

### Manuelle Byte-Serialisierung
```c
void hfe_header_read(hfe_header_t *h, const uint8_t *buf) {
    memcpy(h->signature, buf, 8);
    h->revision = buf[8];
    // ...
}
```
- **Verworfen weil:** 50+ Formate = 50+ Serializer = Wartungsalptraum

---

## Änderungshistorie

| Datum | Autor | Änderung |
|-------|-------|----------|
| 2025-01-11 | UFT Team | Initial erstellt |
