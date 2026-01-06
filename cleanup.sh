#!/bin/bash
#
# UFT Project Cleanup Script
# Entfernt identifizierte Duplikate und alte Code-Versionen
#
# Verwendung:
#   ./cleanup.sh          - Zeigt was gelöscht wird (Dry-Run)
#   ./cleanup.sh --execute - Führt Löschung durch
#   ./cleanup.sh --backup  - Erstellt Backup vor Löschung
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Farben
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Zu löschende Verzeichnisse (Duplikate & alte Versionen)
REMOVE_DIRS=(
    # Duplikate SAMdisk/FluxEngine in extract/
    "src/extract/samdisk"
    "src/extract/fluxengine"
    "src/extract/uft_floppy_lib_v2"
    
    # Alte Library-Versionen
    "src/lib_v2"
    "src/floppy_lib"
    
    # Externer Referenz-Code (nicht Teil des Projekts)
    "reference"
)

# Optionale Löschungen (auskommentiert - manuell aktivieren wenn gewünscht)
OPTIONAL_DIRS=(
    # "docs/TODO"
    # "docs/AUDIT"
    # "build"
)

DRY_RUN=true
DO_BACKUP=false

# Argumente parsen
for arg in "$@"; do
    case $arg in
        --execute)
            DRY_RUN=false
            ;;
        --backup)
            DO_BACKUP=true
            DRY_RUN=false
            ;;
        --help|-h)
            echo "Verwendung: $0 [--execute|--backup]"
            echo "  (keine Argumente) - Dry-Run, zeigt was gelöscht wird"
            echo "  --execute         - Führt Löschung durch"
            echo "  --backup          - Erstellt Backup vor Löschung"
            exit 0
            ;;
    esac
done

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   UFT Project Cleanup Script${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if $DRY_RUN; then
    echo -e "${YELLOW}[DRY-RUN] Keine Änderungen werden vorgenommen${NC}"
    echo -e "${YELLOW}Verwende --execute oder --backup zum Ausführen${NC}"
    echo ""
fi

# Größe berechnen
calc_size() {
    local total=0
    for dir in "${REMOVE_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            size=$(du -sk "$dir" 2>/dev/null | cut -f1)
            total=$((total + size))
        fi
    done
    echo $total
}

TOTAL_SIZE=$(calc_size)
echo -e "${GREEN}Geschätzte Ersparnis: ${TOTAL_SIZE} KB (~$((TOTAL_SIZE/1024)) MB)${NC}"
echo ""

# Backup erstellen
if $DO_BACKUP && ! $DRY_RUN; then
    BACKUP_NAME="uft_backup_$(date +%Y%m%d_%H%M%S).tar.gz"
    echo -e "${BLUE}Erstelle Backup: $BACKUP_NAME${NC}"
    
    BACKUP_TARGETS=""
    for dir in "${REMOVE_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            BACKUP_TARGETS="$BACKUP_TARGETS $dir"
        fi
    done
    
    if [ -n "$BACKUP_TARGETS" ]; then
        tar -czf "$BACKUP_NAME" $BACKUP_TARGETS 2>/dev/null
        echo -e "${GREEN}Backup erstellt: $BACKUP_NAME${NC}"
    fi
    echo ""
fi

# Hauptlöschung
echo -e "${RED}=== ZU LÖSCHENDE VERZEICHNISSE ===${NC}"
echo ""

for dir in "${REMOVE_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        size=$(du -sh "$dir" 2>/dev/null | cut -f1)
        file_count=$(find "$dir" -type f 2>/dev/null | wc -l)
        
        echo -e "  ${RED}✗${NC} $dir"
        echo -e "    Größe: $size | Dateien: $file_count"
        
        if ! $DRY_RUN; then
            rm -rf "$dir"
            echo -e "    ${GREEN}→ Gelöscht${NC}"
        fi
    else
        echo -e "  ${YELLOW}○${NC} $dir (existiert nicht)"
    fi
done

echo ""

# Leere Verzeichnisse aufräumen
if ! $DRY_RUN; then
    echo -e "${BLUE}Räume leere Verzeichnisse auf...${NC}"
    find . -type d -empty -delete 2>/dev/null || true
fi

# Zusammenfassung
echo ""
echo -e "${BLUE}========================================${NC}"
if $DRY_RUN; then
    echo -e "${YELLOW}DRY-RUN abgeschlossen.${NC}"
    echo -e "Zum Ausführen: ${GREEN}./cleanup.sh --execute${NC}"
    echo -e "Mit Backup:    ${GREEN}./cleanup.sh --backup${NC}"
else
    echo -e "${GREEN}Cleanup abgeschlossen!${NC}"
    NEW_SIZE=$(du -sh . 2>/dev/null | cut -f1)
    echo -e "Neue Projektgröße: ${GREEN}$NEW_SIZE${NC}"
fi
echo -e "${BLUE}========================================${NC}"
