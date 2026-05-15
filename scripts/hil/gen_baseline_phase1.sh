#!/usr/bin/env bash
# gen_baseline_phase1.sh — capture the pre-refactor SHA-256 baseline
#                          from main @ MF-149 (commit 5dde6fc).
#
# Half-automated: handles the git checkout + build + hashing, but
# pauses for the actual GUI capture (UFT is GUI-only — no CLI).
#
# Output: tests/baseline_artifacts/gw_known_disk.{scp,sha256} + gw_rpm.txt
#
# Exit codes:
#   0 — baseline captured + hashed successfully
#   1 — user aborted
#   2 — environment / build failure

set -u

MF149="5dde6fc3b807d8e0e9e3e8f2f39fde329e59c5d0"
ARTIFACTS="tests/baseline_artifacts"
SCP_OUT="$ARTIFACTS/gw_known_disk.scp"
SHA_OUT="$ARTIFACTS/gw_known_disk.sha256"
RPM_OUT="$ARTIFACTS/gw_rpm.txt"

# ───────────────────────────────────────────────────────────────────
# Plumbing
# ───────────────────────────────────────────────────────────────────

red()    { printf '\033[31m%s\033[0m\n' "$*"; }
green()  { printf '\033[32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[33m%s\033[0m\n' "$*"; }
bold()   { printf '\033[1m%s\033[0m\n' "$*"; }

pause() {
    printf '\n'
    read -r -p "  Press ENTER when done (or 'q' to abort): " ans
    [ "$ans" = "q" ] && { red "Aborted by user."; exit 1; }
}

confirm() {
    read -r -p "  $1 [y/N]: " ans
    [ "$ans" = "y" ] || [ "$ans" = "Y" ]
}

# ───────────────────────────────────────────────────────────────────
# Pre-flight
# ───────────────────────────────────────────────────────────────────

ROOT="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -n "$ROOT" ] || { red "FATAL: not in a git repo"; exit 2; }
cd "$ROOT"

bold "=== Phase 1 — Pre-refactor baseline capture ==="
echo
echo "  Target commit:   $MF149 (MF-149, last commit before refactor)"
echo "  Output:          $ARTIFACTS/"
echo
yellow "  WARNING: this checks out a historical commit, builds, and asks"
yellow "  you to run the GUI to capture a baseline. Make sure:"
echo "    • working tree is clean (git status)"
echo "    • you know which branch you started on (we will return to it)"
echo "    • your Greaseweazle is plugged in"
echo "    • a known-good DOS-formatted 3.5\" HD floppy (WRITE-PROTECTED)"
echo "      is in the drive"
echo

# Working-tree clean check
if ! git diff --quiet || ! git diff --cached --quiet; then
    red "  Working tree has uncommitted changes."
    red "  Commit or stash them before running this script."
    exit 2
fi

ORIG_BRANCH="$(git symbolic-ref --short HEAD 2>/dev/null || git rev-parse --short HEAD)"
echo "  Will return to: $ORIG_BRANCH after baseline capture"
echo

if ! confirm "Proceed?"; then
    red "Aborted by user."
    exit 1
fi

# ───────────────────────────────────────────────────────────────────
# Step 1 — checkout MF-149 + build
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 1 — checkout MF-149 + build"
echo

set -e
git checkout --detach "$MF149"
set +e

echo
yellow "  HEAD is now at $MF149 (detached). The next step builds the"
yellow "  pre-refactor UFT. Run YOUR usual build commands now:"
echo
echo "    # MinGW / Windows:"
echo "    qmake && mingw32-make -j8"
echo "    # Linux / macOS:"
echo "    qmake && make -j\$(nproc)"
echo
echo "  When the build finishes and UnifiedFloppyTool(.exe) exists,"
echo "  press ENTER. If the build fails, press 'q' to abort and we"
echo "  will return you to $ORIG_BRANCH."
pause

# Sanity check: at least try to find the binary.
if [ ! -e "UnifiedFloppyTool.exe" ] && [ ! -e "UnifiedFloppyTool" ] && [ ! -e "build/UnifiedFloppyTool" ]; then
    red "  No UnifiedFloppyTool binary found in repo root or build/."
    red "  Aborting and returning to $ORIG_BRANCH."
    git checkout "$ORIG_BRANCH"
    exit 2
fi

# ───────────────────────────────────────────────────────────────────
# Step 2 — capture .scp via GUI
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 2 — capture full-track flux via the GUI"
echo
yellow "  UFT is GUI-only. Open UnifiedFloppyTool now and:"
echo
echo "    1. Hardware tab → Controller: Greaseweazle"
echo "    2. Connect"
echo "    3. Read → tracks 0-79, 3 revolutions"
echo "    4. Save the SCP as:"
echo "         $ROOT/$SCP_OUT"
echo
echo "  IMPORTANT: use the EXACT path above so the hash file matches."
echo "  When the file exists, press ENTER."
pause

if [ ! -f "$SCP_OUT" ]; then
    red "  $SCP_OUT was not created. Aborting."
    git checkout "$ORIG_BRANCH"
    exit 2
fi

SIZE=$(stat -c%s "$SCP_OUT" 2>/dev/null || stat -f%z "$SCP_OUT" 2>/dev/null || echo 0)
echo
green "  Captured: $SCP_OUT ($SIZE bytes)"

# ───────────────────────────────────────────────────────────────────
# Step 3 — hash + RPM measurement
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 3 — compute SHA-256 + capture RPM"
echo

(cd "$ARTIFACTS" && sha256sum "$(basename "$SCP_OUT")" > "$(basename "$SHA_OUT")")
green "  Wrote: $SHA_OUT"
cat "$SHA_OUT"
echo

yellow "  Now: in the same GUI session, run the RPM measurement"
yellow "  (Hardware tab → Measure RPM, 50 revolutions). Copy the"
yellow "  result text to:"
echo
echo "    $ROOT/$RPM_OUT"
echo
pause

if [ ! -f "$RPM_OUT" ]; then
    yellow "  $RPM_OUT not present — RPM measurement skipped."
    yellow "  Phase 2 can still proceed but RPM compare will be unavailable."
fi

# ───────────────────────────────────────────────────────────────────
# Step 4 — return to original branch
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 4 — return to $ORIG_BRANCH"
echo

git checkout "$ORIG_BRANCH"

green ""
green "=== Phase 1 complete ==="
echo
echo "  Baseline artefacts:"
ls -l "$ARTIFACTS/"
echo
echo "  Next: run scripts/hil/gen_baseline_phase2.sh to capture the"
echo "  post-refactor result and diff against this baseline."
