#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# UFT Pre-Push Validation Script
# ═══════════════════════════════════════════════════════════════════════════════
# Führe dieses Script VOR jedem Push aus:
#   ./scripts/validate.sh
# ═══════════════════════════════════════════════════════════════════════════════

set -e
ERRORS=0
WARNINGS=0

echo "═══════════════════════════════════════════════════════════════════════════"
echo "  UFT Pre-Push Validation"
echo "═══════════════════════════════════════════════════════════════════════════"
echo ""

# ─────────────────────────────────────────────────────────────────────────────
# 1. Math-Library Check (KRITISCH!)
# ─────────────────────────────────────────────────────────────────────────────
echo "🔍 [1/5] Checking for unguarded 'm' linking..."

# Suche nach direktem 'm' - aber erlaube die zentrale Funktion in CMakeLists.txt
bad_m=$(grep -rn "target_link_libraries" . --include="CMakeLists.txt" 2>/dev/null | grep -E "\bm\b|\bm\)" | grep -v "uft_link_math" | grep -v "^\./CMakeLists.txt:.*if(UNIX)" || true)

# Prüfe ob die gefundenen Stellen NICHT in der uft_link_math Funktion sind
# Die Funktion in CMakeLists.txt Zeile 23-27 ist OK
bad_m_filtered=""
while IFS= read -r line; do
    # Erlaube CMakeLists.txt Zeilen 23-30 (die Funktion)
    if echo "$line" | grep -q "^\./CMakeLists.txt:2[3-9]:\|^\./CMakeLists.txt:30:"; then
        continue
    fi
    bad_m_filtered="${bad_m_filtered}${line}\n"
done <<< "$bad_m"

if [ -n "$bad_m_filtered" ] && [ "$bad_m_filtered" != "\n" ]; then
    echo "❌ FEHLER: Unguarded 'm' found!"
    echo -e "$bad_m_filtered"
    echo ""
    echo "   FIX: Verwende uft_link_math(target) statt direktem 'm'"
    echo ""
    ERRORS=$((ERRORS + 1))
else
    echo "✅ Keine unguarded 'm' Verlinkungen"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 2. PRIVATE/PUBLIC Check
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "🔍 [2/5] Checking for missing PRIVATE/PUBLIC..."

bad_private=$(grep -rn "target_link_libraries" . --include="CMakeLists.txt" 2>/dev/null | grep -v "PRIVATE\|PUBLIC\|INTERFACE\|#\|function" | grep -v "^\./CMakeLists.txt:" || true)

if [ -n "$bad_private" ]; then
    echo "⚠️  WARNING: Missing PRIVATE/PUBLIC in target_link_libraries:"
    echo "$bad_private"
    echo ""
    WARNINGS=$((WARNINGS + 1))
else
    echo "✅ Alle target_link_libraries haben PRIVATE/PUBLIC"
fi

# ─────────────────────────────────────────────────────────────────────────────
# 3. uft_link_math Funktion existiert
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "🔍 [3/5] Checking uft_link_math function exists..."

if grep -q "function(uft_link_math" CMakeLists.txt 2>/dev/null; then
    echo "✅ uft_link_math() Funktion gefunden"
else
    echo "❌ FEHLER: uft_link_math() Funktion fehlt in CMakeLists.txt!"
    ERRORS=$((ERRORS + 1))
fi

# ─────────────────────────────────────────────────────────────────────────────
# 4. Zähle uft_link_math Aufrufe
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "🔍 [4/5] Counting uft_link_math() calls..."

count=$(grep -rn "uft_link_math(" . --include="CMakeLists.txt" 2>/dev/null | grep -v "function\|#" | wc -l)
echo "   $count Targets verwenden uft_link_math()"

# ─────────────────────────────────────────────────────────────────────────────
# 5. OpenMP Check (MSVC braucht nur /openmp, keine Library)
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "🔍 [5/5] Checking OpenMP configuration..."

if grep -q "OpenMP::OpenMP" CMakeLists.txt 2>/dev/null; then
    if grep -q "if(MSVC)" CMakeLists.txt 2>/dev/null && grep -q "/openmp" CMakeLists.txt 2>/dev/null; then
        echo "✅ OpenMP korrekt konfiguriert (MSVC: /openmp flag)"
    else
        echo "⚠️  WARNING: OpenMP sollte auf MSVC nur /openmp verwenden"
        WARNINGS=$((WARNINGS + 1))
    fi
else
    echo "✅ Kein OpenMP oder korrekt konfiguriert"
fi

# ─────────────────────────────────────────────────────────────────────────────
# Zusammenfassung
# ─────────────────────────────────────────────────────────────────────────────
echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "  Ergebnis"
echo "═══════════════════════════════════════════════════════════════════════════"
echo ""
echo "   Fehler:    $ERRORS"
echo "   Warnungen: $WARNINGS"
echo ""

if [ $ERRORS -gt 0 ]; then
    echo "❌ VALIDATION FAILED - Bitte Fehler beheben vor Push!"
    exit 1
else
    echo "✅ VALIDATION PASSED - Ready to push!"
    exit 0
fi
