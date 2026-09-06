#!/usr/bin/env bash
# Install UFT git hooks. Idempotent — safe to re-run.
#
#   bash scripts/install-hooks.sh
#
# Hooks are checked into scripts/git-hooks/. We try to symlink them into
# .git/hooks/ so updates to the checked-in copy take effect immediately.
#
# MF-925: DIESE ZUSAGE GILT NICHT UEBERALL, und das Skript hat es
# zweimal falsch gesagt. Gemessen an diesem Baum:
#
#   .git/hooks/pre-push war eine KOPIE, kein Symlink (test -L: nein)
#   `ln -s` meldete dabei ERFOLG (Rueckgabe 0)
#
# Unter MSYS/Git-for-Windows kopiert `ln -s` ohne
# winsymlinks:nativestrict und gibt trotzdem 0 zurueck. Wer den
# Rueckgabewert liest statt `test -L`, glaubt an einen Symlink, der
# nicht da ist.
#
# Eine Kopie treibt: wer danach scripts/git-hooks/ aendert, aendert
# nicht, was git ausfuehrt.
#
# Das Skript sagt das jetzt pro Haken ("symlinked" oder "copied") und
# nennt am Ende die Folge, statt sie im Kopfkommentar zu behaupten.

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
copied=0
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
    # MF-925: NICHT dem Rueckgabewert von `ln -s` glauben, sondern das
    # ERGEBNIS messen. Unter MSYS/Git-for-Windows liefert `ln -s` ohne
    # winsymlinks:nativestrict eine KOPIE und meldet trotzdem Erfolg —
    # gemessen an genau diesem Baum: das Skript sagte "symlinked",
    # `test -L` sagte nein. Eine Zusage, die man nicht nachmisst, ist
    # eine Behauptung.
    ln -s "$rel" "$target" 2>/dev/null || true
    if [[ -L "$target" ]]; then
        how="symlinked"
    else
        rm -f "$target"
        cp "$hook" "$target"
        how="copied"
        copied=$((copied + 1))
    fi
    chmod +x "$target"
    echo "  $name: installed ($how)"
    installed=$((installed + 1))
done

echo
echo "Installed $installed hook(s) into .git/hooks/"
if [[ "$copied" -gt 0 ]]; then
    echo
    echo "HINWEIS: $copied Haken wurde(n) KOPIERT statt verlinkt (ln -s"
    echo "         nicht moeglich — auf Windows ohne Entwicklermodus"
    echo "         der Normalfall). Folge: Aenderungen an"
    echo "         scripts/git-hooks/ wirken ERST nach erneutem Lauf"
    echo "         dieses Skripts. Nach jeder Haken-Aenderung also:"
    echo "             bash scripts/install-hooks.sh"
fi
echo "Bypass with --no-verify (only when CI gates the same thing)."
