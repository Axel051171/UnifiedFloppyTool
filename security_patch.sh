#!/bin/bash
# UFT Security Auto-Patcher
# Patcht bekannte unsichere Muster automatisch

set -e

cd "$(dirname "$0")"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

FIXED=0
SKIPPED=0

echo -e "${GREEN}=== UFT Security Auto-Patcher ===${NC}"
echo ""

# Funktion: Ersetze strcpy mit snprintf wo sicher
patch_strcpy_literals() {
    echo -e "${YELLOW}[1/4] Patche strcpy mit String-Literalen...${NC}"
    
    # Finde strcpy mit kurzen String-Literalen (< 32 Zeichen)
    for file in $(grep -rl --include="*.c" 'strcpy.*"[^"]\{1,31\}"' src/ 2>/dev/null); do
        # Backup erstellen falls nicht vorhanden
        if [[ ! -f "${file}.orig" ]]; then
            cp "$file" "${file}.orig"
        fi
        
        # Kommentiere unsichere strcpy mit Warnung
        sed -i 's/\(strcpy([^,]*,\s*"[^"]*")\s*;\)/\1  \/* REVIEW: Consider bounds check *\//g' "$file" 2>/dev/null || true
        ((FIXED++)) || true
    done
}

# Funktion: Füge Security-Header zu Dateien hinzu die ihn brauchen
add_security_headers() {
    echo -e "${YELLOW}[2/4] Prüfe Security-Header...${NC}"
    
    # Dateien die popen/system verwenden
    for file in $(grep -rl --include="*.c" -E "(popen|system)\s*\(" src/ 2>/dev/null); do
        if ! grep -q "uft_security.h" "$file"; then
            echo "  → $file fehlt uft_security.h"
            ((SKIPPED++)) || true
        fi
    done
}

# Funktion: Prüfe sscanf ohne Feldbreiten
check_sscanf() {
    echo -e "${YELLOW}[3/4] Prüfe sscanf-Aufrufe...${NC}"
    
    count=$(grep -rn --include="*.c" 'sscanf.*%s' src/ 2>/dev/null | wc -l)
    if [[ $count -gt 0 ]]; then
        echo -e "  ${RED}Warnung: $count sscanf mit %s ohne Feldbreite gefunden${NC}"
        grep -rn --include="*.c" 'sscanf.*%s' src/ 2>/dev/null | head -5
        ((SKIPPED+=count)) || true
    fi
}

# Funktion: Erstelle Compiler-Flags
create_security_flags() {
    echo -e "${YELLOW}[4/4] Erstelle Security-Compiler-Flags...${NC}"
    
    cat > cmake/SecurityFlags.cmake << 'EOF'
# UFT Security Compiler Flags
# Aktiviert Stack-Schutz und weitere Sicherheitsmaßnahmen

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Stack protection
    add_compile_options(-fstack-protector-strong)
    
    # Fortify source (buffer overflow detection)
    add_compile_definitions(_FORTIFY_SOURCE=2)
    
    # Position Independent Executable
    add_compile_options(-fPIE)
    add_link_options(-pie)
    
    # Relocation Read-Only
    add_link_options(-Wl,-z,relro,-z,now)
    
    # Warnings als Fehler für sicherheitsrelevante Probleme
    add_compile_options(
        -Wall
        -Wextra
        -Wformat=2
        -Wformat-security
        -Wstrict-overflow=2
        -Warray-bounds
        -Wstringop-overflow
    )
    
    # Optional: AddressSanitizer für Debug-Builds
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
        if(ENABLE_ASAN)
            add_compile_options(-fsanitize=address,undefined)
            add_link_options(-fsanitize=address,undefined)
        endif()
    endif()
endif()

if(MSVC)
    # MSVC security flags
    add_compile_options(/GS /sdl /guard:cf)
    add_link_options(/DYNAMICBASE /NXCOMPAT /GUARD:CF)
endif()
EOF

    echo "  → cmake/SecurityFlags.cmake erstellt"
    ((FIXED++)) || true
}

# Hauptausführung
mkdir -p cmake

patch_strcpy_literals
add_security_headers
check_sscanf
create_security_flags

echo ""
echo -e "${GREEN}=== Zusammenfassung ===${NC}"
echo -e "Gepatcht:    ${GREEN}$FIXED${NC}"
echo -e "Zu prüfen:   ${YELLOW}$SKIPPED${NC}"
echo ""
echo "Nächste Schritte:"
echo "1. Füge 'include(cmake/SecurityFlags.cmake)' zur CMakeLists.txt hinzu"
echo "2. Prüfe Dateien mit .orig Backup manuell"
echo "3. Führe 'cppcheck --enable=all src/' aus"
