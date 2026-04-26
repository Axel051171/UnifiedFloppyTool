#!/usr/bin/env bash
# Install UFT git hooks. Idempotent — safe to re-run.
#
#   bash scripts/install-hooks.sh
#
# Hooks are checked into scripts/git-hooks/. We symlink them into
# .git/hooks/ so updates to the checked-in copy take effect immediately
# without re-installing.

set -e

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

HOOKS_SRC="$REPO_ROOT/scripts/git-hooks"
HOOKS_DST="$REPO_ROOT/.git/hooks"

if [[ ! -d "$HOOKS_SRC" ]]; then
    echo "ERROR: $HOOKS_SRC not found — wrong repo?" >&2
    exit 1
fi
if [[ ! -d "$HOOKS_DST" ]]; then
    echo "ERROR: $HOOKS_DST not found — is this a git repo?" >&2
    exit 1
fi

installed=0
for hook in "$HOOKS_SRC"/*; do
    [[ -f "$hook" ]] || continue
    name="$(basename "$hook")"
    target="$HOOKS_DST/$name"
    chmod +x "$hook"
    # Use a relative symlink so it works inside checkouts of this repo.
    rel="../../scripts/git-hooks/$name"
    if [[ -L "$target" && "$(readlink "$target")" == "$rel" ]]; then
        echo "  $name: already installed"
        continue
    fi
    rm -f "$target"
    ln -s "$rel" "$target" 2>/dev/null || cp "$hook" "$target"
    chmod +x "$target"
    echo "  $name: installed"
    installed=$((installed + 1))
done

echo
echo "Installed $installed hook(s) into .git/hooks/"
echo "Bypass with --no-verify (only when CI gates the same thing)."
