#!/bin/bash
#
# UFT Duplicate Detection Script
# Für CI/CD Integration - Verhindert neue Duplikate
#
# Exit Codes:
#   0 = Keine Duplikate gefunden
#   1 = Duplikate gefunden (CI sollte fehlschlagen)
#

set -euo pipefail

UFT_ROOT="${UFT_ROOT:-$(pwd)}"
OUTPUT_FORMAT="${1:-text}"  # text, json, github

# Farben
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Finde alle .c und .h Dateien und berechne MD5
find_duplicates() {
    find "$UFT_ROOT/src" -type f \( -name "*.c" -o -name "*.h" \) -exec md5sum {} \; 2>/dev/null | \
    sort | \
    uniq -D -w32
}

# Zähle Duplikat-Gruppen
count_duplicate_groups() {
    find "$UFT_ROOT/src" -type f \( -name "*.c" -o -name "*.h" \) -exec md5sum {} \; 2>/dev/null | \
    sort | \
    uniq -d -w32 | \
    wc -l
}

# Berechne verschwendete LOC
calculate_wasted_loc() {
    local dups
    dups=$(find_duplicates | awk '{print $2}')
    
    if [ -n "$dups" ]; then
        echo "$dups" | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}'
    else
        echo "0"
    fi
}

# Generiere Bericht
generate_report() {
    local dup_groups
    local wasted_loc
    local dups
    
    dup_groups=$(count_duplicate_groups)
    wasted_loc=$(calculate_wasted_loc)
    dups=$(find_duplicates)
    
    case "$OUTPUT_FORMAT" in
        json)
            echo "{"
            echo "  \"duplicate_groups\": $dup_groups,"
            echo "  \"wasted_loc\": $wasted_loc,"
            echo "  \"duplicates\": ["
            
            local first=true
            echo "$dups" | while IFS= read -r line; do
                if [ -n "$line" ]; then
                    hash=$(echo "$line" | awk '{print $1}')
                    file=$(echo "$line" | awk '{print $2}')
                    if [ "$first" = true ]; then
                        first=false
                    else
                        echo ","
                    fi
                    echo -n "    {\"hash\": \"$hash\", \"file\": \"$file\"}"
                fi
            done
            
            echo ""
            echo "  ]"
            echo "}"
            ;;
            
        github)
            # GitHub Actions Annotations
            if [ "$dup_groups" -gt 0 ]; then
                echo "::error::Found $dup_groups duplicate file groups wasting $wasted_loc LOC"
                
                echo "$dups" | while IFS= read -r line; do
                    if [ -n "$line" ]; then
                        file=$(echo "$line" | awk '{print $2}')
                        echo "::warning file=$file::Duplicate file detected"
                    fi
                done
            fi
            ;;
            
        *)  # text
            echo "=============================================="
            echo "UFT Duplicate Detection Report"
            echo "=============================================="
            echo ""
            echo "Duplicate Groups: $dup_groups"
            echo "Wasted LOC: $wasted_loc"
            echo ""
            
            if [ "$dup_groups" -gt 0 ]; then
                echo "Duplicate Files:"
                echo "----------------"
                
                # Gruppiere nach Hash
                echo "$dups" | awk '{
                    hash = $1
                    file = $2
                    if (hash != prev_hash && prev_hash != "") {
                        print ""
                    }
                    print "  " file " [" hash "]"
                    prev_hash = hash
                }'
                
                echo ""
                echo -e "${RED}FAIL: Duplicates detected!${NC}"
            else
                echo -e "${GREEN}PASS: No duplicates found.${NC}"
            fi
            ;;
    esac
}

# Hauptausführung
main() {
    local dup_groups
    
    dup_groups=$(count_duplicate_groups)
    
    generate_report
    
    if [ "$dup_groups" -gt 0 ]; then
        exit 1
    else
        exit 0
    fi
}

main
