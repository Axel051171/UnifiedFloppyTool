#!/bin/bash
# UFT Wöchentlicher Qualitäts-Audit
# Phase 5 der Master-Anweisung
#
# Usage: bash scripts/uft_audit.sh > audit_$(date +%Y%m%d).md

set -u
cd "$(dirname "$0")/.."

date_iso=$(date -Iseconds 2>/dev/null || date +%Y-%m-%dT%H:%M:%S)

echo "# UFT Wöchentlicher Audit $date_iso"
echo ""
echo "## Baseline-Metriken"
echo ""

# 1. Fehler-Gesamtzahl pro Bereich
echo "### Compile-Fehler pro Bereich"
total_err=0
for area in core formats algorithms analysis protection ml pipeline codec decoder encoding pll hal gui; do
    if [ -d "src/$area" ]; then
        n=$(find "src/$area" -name "*.c" 2>/dev/null | \
            xargs -I{} bash -c 'gcc -Wall -I include -I src -std=c99 -fsyntax-only "{}" 2>&1 | grep -c " error:"' 2>/dev/null | \
            awk '{s+=$1} END{print s+0}')
        total_err=$((total_err + n))
        printf "  - %-12s %d\n" "$area:" "$n"
    fi
done
echo "  - **Total:** $total_err"
echo ""

# 2. Plugin-Status
echo "### Plugin-Status"
echo "  - Registry: $(grep -c 'extern const uft_format_plugin_t uft_format_plugin_' src/formats/format_registry/uft_format_registry.c)"
write_cnt=$(grep -rln '\.write_track' src/formats/ --include='*.c' | grep -v parser_v | grep -v format_registry | wc -l)
verify_cnt=$(grep -rln '\.verify_track' src/formats/ --include='*.c' | grep -v parser_v | grep -v format_registry | wc -l)
plugin_b_cnt=$(grep -rln 'UFT_REGISTER_FORMAT_PLUGIN' src/formats/ --include='*.c' | grep -v parser_v | wc -l)
echo "  - Mit write_track: $write_cnt"
echo "  - Mit verify_track: $verify_cnt"
echo "  - Plugin-B Dateien: $plugin_b_cnt"
echo ""

# 3. Stubs ohne Plugin-B
echo "### Stub-Zustand"
stub_count=0
for f in $(find src/formats -name "*_parser_v*.c" 2>/dev/null); do
    fmt=$(basename "$f" | sed 's/_parser_v[0-9]*.c//' | sed 's/^uft_//')
    pbcount=$(grep -rl "uft_format_plugin_${fmt}\\b" src/formats/ --include="*.c" 2>/dev/null | \
              grep -v "_parser_v\\|format_registry" | wc -l)
    if [ "$pbcount" -eq 0 ]; then
        stub_count=$((stub_count + 1))
    fi
done
echo "  - v3-Stubs ohne Plugin-B: $stub_count"
echo ""

# 4. Tote Dateien
echo "### Code-Hygiene"
orphans=$(find src -name "*.c" 2>/dev/null | while read f; do
    base=$(basename "$f")
    if [ $(grep -c "^[^#]*$base" UnifiedFloppyTool.pro 2>/dev/null) -eq 0 ]; then
        echo "1"
    fi
done | wc -l)
echo "  - Verwaiste .c Dateien (nicht in .pro): $orphans"
echo ""

# 5. Plugin-Audit-Ausnahmen
echo "### Plugin-Standard-Abweichungen"
declare -A issue_count
issue_count[K2_include]=0
issue_count[K10_total_sectors]=0
issue_count[K8_write]=0
issue_count[K9_verify]=0

for f in $(grep -rln "UFT_REGISTER_FORMAT_PLUGIN" src/formats/ --include="*.c" | \
           grep -v format_registry | grep -v parser_v | grep -v dsk_generic); do
    grep -q "uft_format_common.h" "$f" || issue_count[K2_include]=$((issue_count[K2_include] + 1))
    grep -q "total_sectors" "$f" || issue_count[K10_total_sectors]=$((issue_count[K10_total_sectors] + 1))
    grep -q "\.write_track" "$f" || issue_count[K8_write]=$((issue_count[K8_write] + 1))
    grep -q "\.verify_track" "$f" || issue_count[K9_verify]=$((issue_count[K9_verify] + 1))
done

for key in K2_include K10_total_sectors K8_write K9_verify; do
    echo "  - $key: ${issue_count[$key]}"
done
echo ""

# 6. Git-Stand
echo "### Git-Stand"
echo "  - Branch: $(git branch --show-current)"
echo "  - Letzter Commit: $(git log -1 --format='%h %s')"
echo "  - Uncommitted changes: $(git status -s | wc -l)"
