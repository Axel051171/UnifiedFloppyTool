#!/usr/bin/env bash
# gen_baseline_phase2.sh — capture post-refactor SHA, diff vs baseline.
#
# Runs on current main HEAD (or whatever branch the user is on).
# Captures the same physical disk that produced the Phase-1 baseline,
# computes SHA-256, and compares against the baseline. PASS only if
# the SHA-256 is identical (byte-for-byte).
#
# Output: tests/baseline_artifacts/post_refactor/gw_known_disk.{scp,sha256}
#         tests/baseline_artifacts/DECISION.md
#
# Exit codes:
#   0 — post-refactor SHA matches baseline (HIL gate PASS)
#   1 — SHA mismatch (HIL gate FAIL — v4.1.4 NOT shippable)
#   2 — environment / build failure / user abort

set -u

ARTIFACTS="tests/baseline_artifacts"
POST="$ARTIFACTS/post_refactor"
BASELINE_SHA="$ARTIFACTS/gw_known_disk.sha256"
BASELINE_SCP="$ARTIFACTS/gw_known_disk.scp"
POST_SCP="$POST/gw_known_disk.scp"
POST_SHA="$POST/gw_known_disk.sha256"
POST_RPM="$POST/gw_rpm.txt"
BASELINE_RPM="$ARTIFACTS/gw_rpm.txt"
DECISION="$ARTIFACTS/DECISION.md"

red()    { printf '\033[31m%s\033[0m\n' "$*"; }
green()  { printf '\033[32m%s\033[0m\n' "$*"; }
yellow() { printf '\033[33m%s\033[0m\n' "$*"; }
bold()   { printf '\033[1m%s\033[0m\n' "$*"; }

pause() {
    printf '\n'
    read -r -p "  Press ENTER when done (or 'q' to abort): " ans
    [ "$ans" = "q" ] && { red "Aborted by user."; exit 2; }
}

confirm() {
    read -r -p "  $1 [y/N]: " ans
    [ "$ans" = "y" ] || [ "$ans" = "Y" ]
}

ROOT="$(git rev-parse --show-toplevel 2>/dev/null)"
[ -n "$ROOT" ] || { red "FATAL: not in a git repo"; exit 2; }
cd "$ROOT"

# ───────────────────────────────────────────────────────────────────
# Pre-flight — Phase 1 must have run
# ───────────────────────────────────────────────────────────────────

bold "=== Phase 2 — Post-refactor capture + diff vs baseline ==="
echo

if [ ! -f "$BASELINE_SCP" ] || [ ! -f "$BASELINE_SHA" ]; then
    red "  Baseline artefacts not found:"
    red "    $BASELINE_SCP"
    red "    $BASELINE_SHA"
    red "  Run scripts/hil/gen_baseline_phase1.sh first."
    exit 2
fi

mkdir -p "$POST"

echo "  Current HEAD:     $(git rev-parse --short HEAD)  ($(git symbolic-ref --short HEAD 2>/dev/null || echo detached))"
echo "  Baseline SHA-256: $(cat "$BASELINE_SHA")"
echo
yellow "  Make sure the SAME physical disk that produced the Phase-1"
yellow "  baseline is in the SAME drive, write-protected, and the"
yellow "  Greaseweazle is connected."
echo

if ! confirm "Proceed?"; then
    red "Aborted by user."
    exit 2
fi

# ───────────────────────────────────────────────────────────────────
# Step 1 — build current HEAD
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 1 — build current HEAD"
echo
yellow "  Run YOUR usual build commands now:"
echo
echo "    # MinGW / Windows:"
echo "    qmake && mingw32-make -j8"
echo "    # Linux / macOS:"
echo "    qmake && make -j\$(nproc)"
echo
echo "  When UnifiedFloppyTool(.exe) is built, press ENTER."
pause

if [ ! -e "UnifiedFloppyTool.exe" ] && [ ! -e "UnifiedFloppyTool" ] && [ ! -e "build/UnifiedFloppyTool" ]; then
    red "  No UnifiedFloppyTool binary found. Aborting."
    exit 2
fi

# ───────────────────────────────────────────────────────────────────
# Step 2 — capture post-refactor .scp via GUI
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 2 — capture full-track flux via GUI (post-refactor)"
echo
yellow "  Open UnifiedFloppyTool now and:"
echo
echo "    1. Hardware tab → Controller: Greaseweazle"
echo "    2. Connect"
echo "    3. Read → tracks 0-79, 3 revolutions  (SAME as Phase 1)"
echo "    4. Save the SCP as:"
echo "         $ROOT/$POST_SCP"
echo
pause

if [ ! -f "$POST_SCP" ]; then
    red "  $POST_SCP not present. Aborting."
    exit 2
fi

# ───────────────────────────────────────────────────────────────────
# Step 3 — hash + diff
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 3 — SHA-256 + diff vs baseline"
echo

(cd "$POST" && sha256sum "$(basename "$POST_SCP")" > "$(basename "$POST_SHA")")
echo
echo "  Baseline:      $(cat "$BASELINE_SHA")"
echo "  Post-refactor: $(cat "$POST_SHA")"
echo

# Compare the SHAs.  Strip filename to compare hex only.
BASE_HEX="$(awk '{print $1}' "$BASELINE_SHA")"
POST_HEX="$(awk '{print $1}' "$POST_SHA")"

RESULT="UNKNOWN"
if [ "$BASE_HEX" = "$POST_HEX" ]; then
    RESULT="PASS"
    green "  ✓ SHA-256 IDENTICAL — no regression detected"
else
    RESULT="FAIL"
    red   "  ✗ SHA-256 MISMATCH — possible regression"
    red   "    Investigate with:"
    red   "      diff <(xxd '$BASELINE_SCP') <(xxd '$POST_SCP') | head"
fi

# ───────────────────────────────────────────────────────────────────
# Step 4 — optional RPM compare
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 4 — RPM measurement (optional)"
echo

if [ -f "$BASELINE_RPM" ]; then
    yellow "  Run RPM measurement in the GUI (50 revolutions) and save"
    yellow "  the output to:"
    echo
    echo "    $ROOT/$POST_RPM"
    echo
    pause

    if [ -f "$POST_RPM" ]; then
        echo "  Baseline RPM:      $(cat "$BASELINE_RPM" | head -1)"
        echo "  Post-refactor RPM: $(cat "$POST_RPM" | head -1)"
        yellow "  Compare manually: within ±0.5 RPM is PASS, otherwise FAIL."
    else
        yellow "  $POST_RPM not present — RPM compare skipped."
    fi
else
    yellow "  No baseline RPM file at $BASELINE_RPM — skipping."
fi

# ───────────────────────────────────────────────────────────────────
# Step 5 — write DECISION.md
# ───────────────────────────────────────────────────────────────────

bold ""
bold "Step 5 — write DECISION.md"
echo

cat > "$DECISION" <<EOF
# HIL Baseline Decision — $(date -u +%Y-%m-%dT%H:%M:%SZ)

## SHA-256 (full-track flux capture, tracks 0-79, 3 revs)

| | SHA-256 | File |
|---|---|---|
| Baseline (MF-149)  | \`$BASE_HEX\` | \`$BASELINE_SCP\` |
| Post-refactor HEAD | \`$POST_HEX\` | \`$POST_SCP\` |

## Verdict

**$RESULT**

EOF

if [ "$RESULT" = "PASS" ]; then
    cat >> "$DECISION" <<EOF
The full-track flux capture of the same physical disk is byte-for-byte
identical between the pre-refactor baseline (\`main @ MF-149\`,
\`5dde6fc\`) and the post-refactor HEAD. The Type-Driven HAL refactor
has introduced **no semantic regression** in the Greaseweazle read
path.

Goal §STOP-Conditions §3 is cleared. Phase 3 baseline gate: **GREEN**.
EOF
else
    cat >> "$DECISION" <<EOF
The full-track flux capture of the same physical disk **does not match**
between the pre-refactor baseline and the post-refactor HEAD. This is
a possible semantic regression introduced by the Type-Driven HAL
refactor.

Goal §STOP-Conditions §3 triggers: **v4.1.4 is NOT shippable** until
the mismatch is diagnosed and either fixed or explicitly accepted
(with documented forensic justification).

Next steps:
1. Diff the two .scp files: \`diff <(xxd '$BASELINE_SCP') <(xxd '$POST_SCP') | head\`
2. Re-run Phase 2 once with a fresh capture to rule out one-shot flux
   jitter (write-protected disk minimises this but is not zero).
3. If still mismatched: file under \`docs/REFACTOR_NOTES.md\` →
   "Hardware Truth Failure".
4. Do **NOT** tag \`v4.1.4\` until this is resolved.
EOF
fi

cat >> "$DECISION" <<EOF

## Reproduce

\`\`\`bash
bash scripts/hil/gen_baseline_phase1.sh   # if baseline missing
bash scripts/hil/gen_baseline_phase2.sh   # this run
\`\`\`
EOF

green "  Wrote: $DECISION"

# ───────────────────────────────────────────────────────────────────
# Exit
# ───────────────────────────────────────────────────────────────────

bold ""
if [ "$RESULT" = "PASS" ]; then
    green "=== Phase 2 complete — HIL baseline gate PASS ==="
    exit 0
else
    red "=== Phase 2 complete — HIL baseline gate FAIL ==="
    red "    v4.1.4 is NOT shippable until the mismatch is resolved."
    exit 1
fi
