#!/bin/bash
# Installiert Git Pre-Commit Hook

HOOK_DIR=".git/hooks"
HOOK_FILE="$HOOK_DIR/pre-commit"

if [ ! -d "$HOOK_DIR" ]; then
    echo "Not a git repository. Run 'git init' first."
    exit 1
fi

cat > "$HOOK_FILE" << 'HOOK'
#!/bin/bash
# UFT Pre-Commit Hook
# Verhindert Commits mit bekannten Fehlern

echo "Running UFT pre-commit checks..."

# Führe Validierung aus
if [ -x "./scripts/validate.sh" ]; then
    ./scripts/validate.sh
    if [ $? -ne 0 ]; then
        echo ""
        echo "❌ Pre-commit checks failed!"
        echo "Fix the errors above or use 'git commit --no-verify' to skip."
        exit 1
    fi
fi

echo "✅ Pre-commit checks passed"
exit 0
HOOK

chmod +x "$HOOK_FILE"
echo "✅ Pre-commit hook installed"
echo "   Run 'git commit --no-verify' to skip checks if needed"
