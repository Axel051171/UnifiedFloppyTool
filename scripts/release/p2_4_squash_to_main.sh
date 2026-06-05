#!/usr/bin/env bash
# scripts/release/p2_4_squash_to_main.sh
# V415-PLAN P2.4 hand-off — Squash-merge tests/v4.1.5-hardening → main
# and tag v4.1.4-final once the RC1-Window closes (2026-05-29).
#
# This script is INTENTIONALLY a checklist with explicit confirmations,
# not a black-box automation. Tagging a release is irreversible.

set -euo pipefail

REQUIRED_BRANCH="tests/v4.1.5-hardening"
TARGET_BRANCH="main"
PROPOSED_TAG="v4.1.4"
RC1_WINDOW_END="2026-05-29"

cd "$(git rev-parse --show-toplevel)"

# ─────────────────────────────────────────────────────────────────────
echo "==> P2.4 pre-flight checks"

current_branch=$(git rev-parse --abbrev-ref HEAD)
if [[ "$current_branch" != "$REQUIRED_BRANCH" ]]; then
    echo "❌ ABORT: not on $REQUIRED_BRANCH (current: $current_branch)"
    exit 1
fi

today=$(date +%Y-%m-%d)
if [[ "$today" < "$RC1_WINDOW_END" ]]; then
    echo "❌ ABORT: RC1-Window ends $RC1_WINDOW_END (today: $today)"
    echo "         Wait for the window to close before promoting."
    exit 1
fi

# Verify CI green on origin
echo "==> Verifying CI green on origin/$REQUIRED_BRANCH"
gh run list -b "$REQUIRED_BRANCH" -L 4 --json status,conclusion \
    | jq -e '[.[] | select(.status == "completed" and .conclusion == "success")] | length == 4' \
    > /dev/null || {
    echo "❌ ABORT: CI is not 4/4 green on origin/$REQUIRED_BRANCH"
    exit 1
}

# Verify Hardware-Truth-Tests passed (HIL.GW must have been run)
HIL_REPORT="releases/$PROPOSED_TAG/hil_report.md"
if [[ ! -f "$HIL_REPORT" ]]; then
    echo "❌ ABORT: $HIL_REPORT not found — HIL.GW has not been run."
    echo "         Run: python tests/hil/run_hil.py --out $HIL_REPORT"
    exit 1
fi
if ! grep -q "10/10.*PASS\|all checks.*PASS" "$HIL_REPORT"; then
    echo "❌ ABORT: $HIL_REPORT does not show 10/10 PASS for Greaseweazle"
    cat "$HIL_REPORT" | tail -20
    exit 1
fi

# ─────────────────────────────────────────────────────────────────────
echo "==> All gates passed. Squash + tag plan:"
echo "    1. git checkout $TARGET_BRANCH && git pull origin $TARGET_BRANCH"
echo "    2. git merge --squash $REQUIRED_BRANCH"
echo "    3. git commit  (use commit message from CHANGELOG.md v4.1.4 section)"
echo "    4. git tag -a $PROPOSED_TAG -m '...'"
echo "    5. git push origin $TARGET_BRANCH"
echo "    6. git push origin $PROPOSED_TAG"
echo "    7. Optional: git branch -D $REQUIRED_BRANCH && git push origin --delete $REQUIRED_BRANCH"
echo ""
echo "Each step requires explicit confirmation. Run interactively:"
echo "    bash $0 --execute"

if [[ "${1:-}" != "--execute" ]]; then
    exit 0
fi

# ─────────────────────────────────────────────────────────────────────
# Interactive execution path
echo ""
read -rp "Confirm squash-merge $REQUIRED_BRANCH → $TARGET_BRANCH? [y/N] " yn
[[ "$yn" == "y" ]] || exit 0

git checkout "$TARGET_BRANCH"
git pull origin "$TARGET_BRANCH"
git merge --squash "$REQUIRED_BRANCH"
echo ""
echo "Stage looks like:"
git status --short
echo ""
read -rp "Continue to commit + tag? [y/N] " yn
[[ "$yn" == "y" ]] || { echo "Stopping. Run: git reset --merge"; exit 0; }

git commit
git tag -a "$PROPOSED_TAG" -m "v4.1.4 — see CHANGELOG.md"
git push origin "$TARGET_BRANCH"
git push origin "$PROPOSED_TAG"

echo ""
echo "✅ Done. $PROPOSED_TAG pushed. release.yml should now build artefacts."
echo "Monitor: gh run watch \$(gh run list -L 1 --json databaseId -q '.[0].databaseId')"
