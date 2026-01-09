#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# UFT Pre-Commit Validation Script
# ═══════════════════════════════════════════════════════════════════════════════
# 
# Dieses Script prüft auf häufige Fehler BEVOR Code committed wird.
# Basierend auf den tatsächlichen Build-Fehlern die wir gefunden haben.
#
# Verwendung:
#   ./scripts/validate.sh           # Alles prüfen
#   ./scripts/validate.sh --quick   # Nur kritische Prüfungen
#
# ═══════════════════════════════════════════════════════════════════════════════

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

ERRORS=0
WARNINGS=0

error() {
    echo -e "${RED}❌ ERROR: $1${NC}"
    ERRORS=$((ERRORS + 1))
}

warning() {
    echo -e "${YELLOW}⚠️  WARNING: $1${NC}"
    WARNINGS=$((WARNINGS + 1))
}

success() {
    echo -e "${GREEN}✓ $1${NC}"
}

echo "═══════════════════════════════════════════════════════════════"
echo "       UFT Pre-Commit Validation"
echo "═══════════════════════════════════════════════════════════════"
echo ""

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 1: Neue Header haben Include-Guards
# ═══════════════════════════════════════════════════════════════════════════════
echo "=== Check 1: Include-Guards ==="

for header in $(find include src -name "*.h" -type f 2>/dev/null); do
    if ! grep -q "#ifndef\|#pragma once" "$header" 2>/dev/null; then
        error "$header: Fehlender Include-Guard"
    fi
done
success "Include-Guards geprüft"

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 2: Keine undeklarierten Typen in neuen Headern
# ═══════════════════════════════════════════════════════════════════════════════
echo ""
echo "=== Check 2: Typ-Definitionen ==="

# Diese Typen MÜSSEN existieren (basierend auf unseren Fehlern)
REQUIRED_TYPES=(
    "uft_sector_t"
    "uft_track_t"
    "uft_prot_scheme_t"
    "uft_md_session_t"
)

for type in "${REQUIRED_TYPES[@]}"; do
    if ! grep -rq "typedef.*$type\|} $type;" include/ 2>/dev/null; then
        error "Typ '$type' nicht in include/ definiert"
    fi
done
success "Kritische Typen vorhanden"

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 3: Keine fehlenden Konstanten
# ═══════════════════════════════════════════════════════════════════════════════
echo ""
echo "=== Check 3: Kritische Konstanten ==="

REQUIRED_CONSTANTS=(
    "UFT_CRC16_INIT"
    "UFT_CRC16_POLY"
    "UFT_MFM_MARK_IDAM"
    "UFT_MFM_MARK_DAM"
    "UFT_OK"
    "UFT_ERROR"
)

for const in "${REQUIRED_CONSTANTS[@]}"; do
    if ! grep -rq "#define $const" include/ 2>/dev/null; then
        error "Konstante '$const' nicht definiert"
    fi
done
success "Kritische Konstanten vorhanden"

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 4: MSVC-Kompatibilität (atomic)
# ═══════════════════════════════════════════════════════════════════════════════
echo ""
echo "=== Check 4: MSVC-Kompatibilität ==="

# Dateien die atomic verwenden müssen uft_atomics.h inkludieren
for file in $(grep -rl "atomic_int\|atomic_bool" --include="*.c" src/ 2>/dev/null); do
    if ! grep -q "uft_atomics.h\|<stdatomic.h>" "$file" 2>/dev/null; then
        warning "$file: Verwendet atomic ohne uft_atomics.h Include"
    fi
done
success "Atomic-Verwendung geprüft"

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 5: Struct-Member-Vollständigkeit
# ═══════════════════════════════════════════════════════════════════════════════
echo ""
echo "=== Check 5: Struct-Member ==="

# uft_prot_scheme_t muss diese Members haben
if grep -q "typedef.*uft_prot_scheme_t\|} uft_prot_scheme_t;" include/uft/uft_protection.h 2>/dev/null; then
    for member in "id" "platform" "indicators" "notes"; do
        if ! grep -A 100 "typedef struct {" include/uft/uft_protection.h 2>/dev/null | \
             grep -B 100 "} uft_prot_scheme_t;" | grep -q "$member"; then
            error "uft_prot_scheme_t: Member '$member' fehlt"
        fi
    done
fi

# uft_md_session muss stats haben
if grep -q "typedef.*uft_md_session" include/uft/uft_multi_decode.h 2>/dev/null; then
    if ! grep -A 50 "typedef struct uft_md_session" include/uft/uft_multi_decode.h | \
         grep -q "stats"; then
        error "uft_md_session: 'stats' Member fehlt"
    fi
fi
success "Struct-Member geprüft"

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 6: CMakeLists.txt Syntax
# ═══════════════════════════════════════════════════════════════════════════════
echo ""
echo "=== Check 6: CMake Syntax ==="

for cmake in $(find . -name "CMakeLists.txt" -type f 2>/dev/null); do
    if ! cmake -P "$cmake" 2>/dev/null; then
        # Nur warnen, nicht blockieren
        warning "$cmake: Möglicherweise Syntax-Fehler"
    fi
done 2>/dev/null || true
success "CMake-Dateien geprüft"

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 7: Keine harten Include-Pfade
# ═══════════════════════════════════════════════════════════════════════════════
echo ""
echo "=== Check 7: Include-Pfade ==="

# Prüfe auf absolute Pfade
if grep -r '#include "/\|#include "C:\|#include "D:' --include="*.c" --include="*.h" . 2>/dev/null; then
    error "Absolute Include-Pfade gefunden"
fi
success "Keine absoluten Pfade"

# ═══════════════════════════════════════════════════════════════════════════════
# ERGEBNIS
# ═══════════════════════════════════════════════════════════════════════════════
echo ""
echo "═══════════════════════════════════════════════════════════════"
if [ $ERRORS -gt 0 ]; then
    echo -e "${RED}FEHLGESCHLAGEN: $ERRORS Fehler, $WARNINGS Warnungen${NC}"
    echo "Bitte Fehler beheben vor dem Commit!"
    exit 1
else
    if [ $WARNINGS -gt 0 ]; then
        echo -e "${YELLOW}BESTANDEN mit $WARNINGS Warnungen${NC}"
    else
        echo -e "${GREEN}ALLE PRÜFUNGEN BESTANDEN${NC}"
    fi
    exit 0
fi
