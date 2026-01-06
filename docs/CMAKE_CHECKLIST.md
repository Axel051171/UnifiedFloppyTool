# UFT CMakeLists.txt Checkliste

## Bei JEDEM neuen Modul prüfen:

### ✅ Pflicht

- [ ] `target_link_libraries` hat **PRIVATE** oder **PUBLIC**
- [ ] Math-Library via `uft_link_math(target)` - **NIE** direkt `m`
- [ ] Threads via `uft_link_threads(target)` oder `Threads::Threads`
- [ ] C/C++ Standard gesetzt (`C_STANDARD 11` / `CXX_STANDARD 17`)

### ⚠️ Vermeiden

- [ ] Keine hardcodierten Pfade (`/usr/local/...`)
- [ ] Keine `-lm` oder `m` direkt
- [ ] Keine plattformspezifischen Flags ohne Guard

### 🔍 Vor Push

```bash
# Validierungs-Script ausführen
./scripts/validate.sh

# Oder manuell prüfen
grep -rn "target_link_libraries" . --include="CMakeLists.txt" | grep -E "\bm\b" | grep -v "uft_link_math"
```

---

## Beispiel: Korrektes CMakeLists.txt

```cmake
# Modul-Name: uft_example
add_library(uft_example STATIC
    example.c
)

target_include_directories(uft_example PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_example PRIVATE
    uft_core
)

# Math-Library (IMMER über zentrale Funktion!)
uft_link_math(uft_example)

set_target_properties(uft_example PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
```

---

## Häufige Fehler

| ❌ Falsch | ✅ Richtig |
|-----------|-----------|
| `target_link_libraries(t m)` | `uft_link_math(t)` |
| `target_link_libraries(t lib)` | `target_link_libraries(t PRIVATE lib)` |
| `if(NOT WIN32)` | `if(UNIX)` |
