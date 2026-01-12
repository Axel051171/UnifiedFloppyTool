#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# UFT Git Hooks Setup
# ═══════════════════════════════════════════════════════════════════════════════
#
# Installiert Pre-Commit Hook der validate.sh ausführt
#
# Verwendung:
#   ./scripts/setup-hooks.sh
#
# ═══════════════════════════════════════════════════════════════════════════════

set -e

HOOK_DIR=".git/hooks"
PRE_COMMIT="$HOOK_DIR/pre-commit"

if [ ! -d ".git" ]; then
    echo "Fehler: Kein Git-Repository gefunden"
    echo "Bitte im Root-Verzeichnis des Projekts ausführen"
    exit 1
fi

mkdir -p "$HOOK_DIR"

cat > "$PRE_COMMIT" << 'HOOK'
#!/bin/bash
# UFT Pre-Commit Hook
# Führt validate.sh vor jedem Commit aus

echo "🔍 UFT Pre-Commit Validation..."

if [ -f "./scripts/validate.sh" ]; then
    ./scripts/validate.sh
    exit $?
else
    echo "⚠️  scripts/validate.sh nicht gefunden, überspringe Validation"
    exit 0
fi
HOOK

chmod +x "$PRE_COMMIT"

echo "✓ Pre-Commit Hook installiert: $PRE_COMMIT"
echo ""
echo "Der Hook führt './scripts/validate.sh' vor jedem Commit aus."
echo "Um den Hook zu deaktivieren: git commit --no-verify"
