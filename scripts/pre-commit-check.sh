#!/bin/bash
# UFT Pre-Commit Check Script
# Installieren: cp scripts/pre-commit-check.sh .git/hooks/pre-commit

echo "═══════════════════════════════════════════════════════════════"
echo "                    UFT Pre-Commit Check"
echo "═══════════════════════════════════════════════════════════════"

ERRORS=0
WARNINGS=0

# 1. Prüfe auf problematische Include-Pfade
echo -n "📁 Include-Pfade... "
BAD_INCLUDES=$(git diff --cached --name-only --diff-filter=ACM | xargs grep -l '#include.*"tracks/track_generator.h"' 2>/dev/null || true)
if [ -n "$BAD_INCLUDES" ]; then
    echo "❌"
    echo "   Relative Include-Pfade gefunden in:"
    echo "$BAD_INCLUDES" | sed 's/^/     - /'
    ERRORS=$((ERRORS + 1))
else
    echo "✓"
fi

# 2. Prüfe auf direkte stdatomic.h
echo -n "⚛️  Atomics... "
DIRECT_ATOMIC=$(git diff --cached --name-only --diff-filter=ACM | xargs grep -l '#include.*<stdatomic.h>' 2>/dev/null || true)
if [ -n "$DIRECT_ATOMIC" ]; then
    echo "❌"
    echo "   Direkter <stdatomic.h> Include (nicht MSVC-kompatibel):"
    echo "$DIRECT_ATOMIC" | sed 's/^/     - /'
    echo "   → Benutze: #include \"uft_atomics.h\""
    ERRORS=$((ERRORS + 1))
else
    echo "✓"
fi

# 3. Prüfe auf Legacy-Error-Codes ohne compat header
echo -n "🔢 Error-Codes... "
for f in $(git diff --cached --name-only --diff-filter=ACM -- '*.c' 2>/dev/null); do
    if [ -f "$f" ]; then
        if grep -q "UFT_ERROR_FORMAT_NOT_SUPPORTED\|UFT_ERR_INVALID_PARAM\|UFT_ERR_FILE_OPEN" "$f"; then
            if ! grep -q "uft_error_compat.h" "$f"; then
                echo "⚠️"
                echo "   $f: Legacy-Error-Codes ohne uft_error_compat.h"
                WARNINGS=$((WARNINGS + 1))
            fi
        fi
    fi
done
[ $WARNINGS -eq 0 ] && echo "✓"

# 4. Syntax-Check der geänderten C-Dateien
echo -n "🔧 Syntax... "
SYNTAX_ERRORS=0
for f in $(git diff --cached --name-only --diff-filter=ACM -- '*.c' 2>/dev/null); do
    if [ -f "$f" ]; then
        if ! gcc -fsyntax-only -c "$f" \
            -I include -I include/uft -I include/uft/core \
            -I include/tracks -I src 2>/dev/null; then
            SYNTAX_ERRORS=$((SYNTAX_ERRORS + 1))
        fi
    fi
done
if [ $SYNTAX_ERRORS -gt 0 ]; then
    echo "⚠️ $SYNTAX_ERRORS Dateien mit Warnungen"
else
    echo "✓"
fi

echo "═══════════════════════════════════════════════════════════════"

if [ $ERRORS -gt 0 ]; then
    echo "❌ $ERRORS Fehler gefunden - Commit abgebrochen!"
    echo "   Behebe die Fehler und versuche es erneut."
    exit 1
fi

if [ $WARNINGS -gt 0 ]; then
    echo "⚠️  $WARNINGS Warnungen - Commit wird fortgesetzt."
fi

echo "✓ Pre-Commit Check bestanden"
exit 0
