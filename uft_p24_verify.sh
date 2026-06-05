#!/usr/bin/env bash
# uft_p24_verify.sh — verification gate for .claude/goals/v4.1.4-final.md
#
# Runs the four BEFUND checks from the goal spec and reports pass/fail
# per BEFUND. Pure read-only — never modifies the repo. Safe to re-run
# at any time during the v4.1.4-final workflow.
#
# Exit codes:
#   0 — all four BEFUNDs cleared (Phase 1+3 satisfied)
#   1 — one or more BEFUNDs still open
#   2 — environment problem (not in repo, missing tools, etc.)
#
# BEFUND mapping:
#   1 — RELEASE_NOTES.md stale "queued for v4.1.5" claims (doc layer)
#   2 — main vs refactor/type-driven-hal divergence (git layer)
#   3 — PR #26 "DO NOT MERGE" merge needs git-notes explanation
#   4 — HIL hardware gate (audit/rc1_field_notes.md NOT_RUN count)
#
# Usage:
#   bash uft_p24_verify.sh           # full report
#   bash uft_p24_verify.sh --quiet   # exit code only
#   bash uft_p24_verify.sh --befund 2  # one BEFUND only

set -u
# Deliberately NOT 'set -e' — we want to run all checks even if one fails.

# ────────────────────────────────────────────────────────────────────
# Plumbing
# ────────────────────────────────────────────────────────────────────

QUIET=0
ONLY_BEFUND=0
for arg in "$@"; do
    case "$arg" in
        --quiet|-q) QUIET=1 ;;
        --befund) shift; ONLY_BEFUND="${1:-0}"; shift ;;
        --befund=*) ONLY_BEFUND="${arg#*=}" ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
    esac
done

FAILS=0
PASSES=0

say()  { [ "$QUIET" = "1" ] || printf '%s\n' "$*"; }
ok()   { PASSES=$((PASSES+1)); [ "$QUIET" = "1" ] || printf '  \033[32m✓\033[0m %s\n' "$*"; }
bad()  { FAILS=$((FAILS+1));   [ "$QUIET" = "1" ] || printf '  \033[31m✗\033[0m %s\n' "$*"; }
hdr()  { [ "$QUIET" = "1" ] || printf '\n\033[1m── %s\033[0m\n' "$*"; }
note() { [ "$QUIET" = "1" ] || printf '    %s\n' "$*"; }

want()      { [ "$ONLY_BEFUND" = "0" ] || [ "$ONLY_BEFUND" = "$1" ]; }
git_root()  { git rev-parse --show-toplevel 2>/dev/null; }

# ────────────────────────────────────────────────────────────────────
# Pre-flight
# ────────────────────────────────────────────────────────────────────

ROOT="$(git_root)"
if [ -z "$ROOT" ]; then
    printf 'FATAL: not inside a git repo\n' >&2
    exit 2
fi
cd "$ROOT"

# Ensure refactor branch is fetched — BEFUND 2 needs it.
git fetch --quiet origin 2>/dev/null || true
git fetch --quiet origin refs/notes/commits:refs/notes/commits 2>/dev/null || true

say "uft_p24_verify.sh — v4.1.4-final goal verification"
say "Repo: $ROOT"
say "HEAD: $(git rev-parse --short HEAD)  ($(git rev-parse --abbrev-ref HEAD))"

# ────────────────────────────────────────────────────────────────────
# BEFUND 1 — RELEASE_NOTES.md does not lie about v4.1.5 backlog
# ────────────────────────────────────────────────────────────────────
if want 1; then
    hdr "BEFUND 1 — RELEASE_NOTES.md ehrlich"

    if [ ! -f RELEASE_NOTES.md ]; then
        bad "RELEASE_NOTES.md not found"
    else
        # The three stale claims from RC1 that MF-240 must have removed.
        # All three describe work that is already done (MF-200/201/202/205/206).
        STALE1=$(grep -c "FluxCaptureJob.*uft_gw_\|FluxWriteJob.*uft_gw_" RELEASE_NOTES.md || true)
        STALE2=$(grep -c "raw_handle()\s*legacy escape hatch" RELEASE_NOTES.md || true)
        STALE3=$(grep -c "8 non-Greaseweazle controllers'.*V2 routing.*queued as P1.23" RELEASE_NOTES.md || true)

        if [ "$STALE1" = "0" ] && [ "$STALE2" = "0" ] && [ "$STALE3" = "0" ]; then
            ok "no stale 'queued for v4.1.5' claims (P1.20/P1.21/P1.22/P1.23 — already shipped)"
        else
            bad "RELEASE_NOTES.md lügt: P1.20/21/22/23 claims still present (counts: $STALE1/$STALE2/$STALE3)"
        fi

        # The honest-backlog block must mention at least the goal's headline items.
        HONEST_HITS=0
        for term in "M3.x" "ARCH-7 sub-B" "ARCH-8" "§1.2" "Plugin-Compliance" "ARCH-9" "Emulator-CI"; do
            grep -qF "$term" RELEASE_NOTES.md && HONEST_HITS=$((HONEST_HITS+1))
        done
        if [ "$HONEST_HITS" -ge 6 ]; then
            ok "honest-backlog block populated ($HONEST_HITS / 7 goal items present)"
        else
            bad "honest-backlog block incomplete ($HONEST_HITS / 7 goal items present)"
        fi
    fi
fi

# ────────────────────────────────────────────────────────────────────
# BEFUND 2 — main vs refactor/type-driven-hal divergence
# ────────────────────────────────────────────────────────────────────
if want 2; then
    hdr "BEFUND 2 — main vs origin/refactor/type-driven-hal"

    if ! git rev-parse --verify origin/main >/dev/null 2>&1 \
       || ! git rev-parse --verify origin/refactor/type-driven-hal >/dev/null 2>&1
    then
        bad "could not resolve origin/main or origin/refactor/type-driven-hal"
    else
        DIVERGE=$(git rev-list --left-right --count \
            origin/main...origin/refactor/type-driven-hal 2>/dev/null)
        LEFT=${DIVERGE%%[[:space:]]*}
        RIGHT=${DIVERGE##*[[:space:]]}

        note "git rev-list --left-right --count origin/main...origin/refactor/type-driven-hal"
        note "→ $LEFT (main ahead)    $RIGHT (refactor ahead)"

        if [ "$RIGHT" = "0" ] && [ "$LEFT" -ge 5 ]; then
            ok "main has $LEFT commits refactor lacks; refactor has 0 commits main lacks"
        else
            bad "expected '≥5    0', got '$LEFT    $RIGHT' — squash-merge story does not hold"
        fi

        # src/ + include/ must be empty between main and refactor
        SRC_DIFF=$(git diff origin/main origin/refactor/type-driven-hal -- src/ include/ 2>/dev/null | head -c 1)
        if [ -z "$SRC_DIFF" ]; then
            ok "src/ + include/ identical between main and refactor branch"
        else
            bad "src/ or include/ differ between main and refactor — investigate"
        fi
    fi
fi

# ────────────────────────────────────────────────────────────────────
# BEFUND 3 — PR #26 "DO NOT MERGE" carries explanation
# ────────────────────────────────────────────────────────────────────
if want 3; then
    hdr "BEFUND 3 — PR #26 merge commit 21ff31b5 has git-notes"

    MERGE_SHA="21ff31b5bf5f1e8786f429baf1839d88d9542d43"
    if ! git cat-file -e "$MERGE_SHA" 2>/dev/null; then
        bad "merge commit $MERGE_SHA not present locally — fetch main"
    else
        NOTE=$(git notes show "$MERGE_SHA" 2>/dev/null || true)
        if [ -n "$NOTE" ] && printf '%s' "$NOTE" | grep -qF "MF-242"; then
            ok "git notes show 21ff31b5 returns MF-242 explanation"
            note "first line: $(printf '%s' "$NOTE" | head -1)"
        elif [ -n "$NOTE" ]; then
            bad "git note present but does not mention MF-242"
        else
            bad "no git note on 21ff31b5 — run: git notes add -m '...' $MERGE_SHA"
        fi

        # Long-form rationale doc should also be present
        if [ -f "releases/v4.1.4/PR26_MERGE_RATIONALE.md" ]; then
            ok "releases/v4.1.4/PR26_MERGE_RATIONALE.md exists (long-form trail)"
        else
            bad "releases/v4.1.4/PR26_MERGE_RATIONALE.md missing"
        fi
    fi
fi

# ────────────────────────────────────────────────────────────────────
# BEFUND 4 — HIL hardware gate
# ────────────────────────────────────────────────────────────────────
if want 4; then
    hdr "BEFUND 4 — HIL hardware-tier (audit/rc1_field_notes.md)"

    NOTES=audit/rc1_field_notes.md
    if [ ! -f "$NOTES" ]; then
        bad "$NOTES not found — Phase 3 cannot be assessed"
    else
        NOT_RUN=$(grep -c "NOT_RUN" "$NOTES" || true)
        PASS=$(grep -c "| PASS " "$NOTES" || true)
        FAIL=$(grep -c "| FAIL " "$NOTES" || true)

        note "PASS=$PASS  FAIL=$FAIL  NOT_RUN=$NOT_RUN"

        if [ "$FAIL" -gt 0 ]; then
            bad "$FAIL FAIL row(s) in field notes — release blocker until triaged"
        elif [ "$NOT_RUN" -gt 0 ]; then
            bad "$NOT_RUN NOT_RUN row(s) — HIL gate not yet closed (real hardware needed)"
        else
            ok "all rows PASS — HIL gate closed"
        fi

        # Baseline artefacts prerequisite
        if [ -d tests/baseline_artifacts ]; then
            BASELINE_FILES=$(find tests/baseline_artifacts -name '*.sha256' 2>/dev/null | wc -l)
            if [ "$BASELINE_FILES" -gt 0 ]; then
                ok "tests/baseline_artifacts/ exists with $BASELINE_FILES .sha256 file(s)"
            else
                bad "tests/baseline_artifacts/ exists but has no .sha256 files"
            fi
        else
            bad "tests/baseline_artifacts/ does NOT exist — Phase 3 prerequisite missing"
        fi
    fi
fi

# ────────────────────────────────────────────────────────────────────
# Summary
# ────────────────────────────────────────────────────────────────────
hdr "Summary"
say "  $PASSES passed, $FAILS failed"

if [ "$FAILS" = "0" ]; then
    say ""
    say "  All requested BEFUND checks cleared."
    exit 0
else
    say ""
    say "  $FAILS BEFUND(s) still open — see lines marked ✗ above."
    exit 1
fi
