#!/usr/bin/env bash
# lint_recovery.sh — heuristic checks for the 5 recovery-integrity invariants.
#
# Not a real static analyzer — pattern-matches that catch the bug families
# from the 2026-04-25 audit. Emits warnings, not errors. Each warning can
# be silenced inline with /* INTEGRITY-OK: <reason> */ on the same line.
#
# Usage:
#   bash lint_recovery.sh <file.c> [<file.c> ...]
#
# Exit code: 0 if zero unsuppressed warnings, 1 otherwise.

set -u

if [ $# -lt 1 ]; then
    echo "usage: $0 <file.c> [<file.c> ...]" >&2
    exit 2
fi

WARN_FILE="$(mktemp)"
trap 'rm -f "$WARN_FILE"' EXIT

WARN() {
    # $1=file:line  $2=invariant tag  $3=message  $4=sample line
    local where="$1" inv="$2" msg="$3" sample="$4"
    if echo "$sample" | grep -q "INTEGRITY-OK"; then
        return
    fi
    echo "$where: [$inv] $msg"
    echo "    | $sample"
    echo "1" >> "$WARN_FILE"
}

scan_file() {
    local f="$1"
    [ -r "$f" ] || { echo "$f: cannot read" >&2; return; }

    # I1 — linear interpolation pattern: (end - start) * (j+1) / (gap_len + 1) etc.
    grep -nE '\*\s*\([a-zA-Z_]+\s*\+\s*1\)\s*/\s*\(' "$f" | while IFS=: read -r line content; do
        WARN "$f:$line" "I1" "looks like linear interpolation — binary data must not be interpolated" "$content"
    done

    # I1 — memset to fill a gap discovered during recovery
    grep -nE 'memset\s*\(\s*[a-zA-Z_]+\s*\+\s*gap_start' "$f" | while IFS=: read -r line content; do
        WARN "$f:$line" "I1" "filling gap with memset — also update confidence_map[] and set RECONSTRUCTED flag" "$content"
    done

    # I2 — first-match correction: 'if (crc...) return 1' inside a flip loop
    awk '
        /for\s*\(/ { in_loop = NR + 30 }
        NR <= in_loop && /\^=/ { has_flip = NR }
        NR <= in_loop && has_flip && /if\s*\(.*crc.*\)/ { match_line = NR }
        match_line && NR == match_line + 1 && /return\s+1/ {
            print FILENAME ":" match_line ":" $0
            match_line = 0
        }
    ' "$f" | while IFS=: read -r fn line content; do
        sample=$(sed -n "${line}p" "$f")
        WARN "$f:$line" "I2" "first-match CRC correction — must verify uniqueness across all candidate flips" "$sample"
    done

    # I4 — uint8_t counts[256] used as a counter — likely overflows
    grep -nE 'uint8_t\s+counts\s*\[\s*256\s*\]' "$f" | while IFS=: read -r line content; do
        WARN "$f:$line" "I4" "uint8_t counter array — overflows at 256 inputs; use uint32_t" "$content"
    done

    # I3 — function uses 'passes[X].data[' without referencing 'crc_ok' anywhere in the function
    # (rough heuristic: function name extracted from preceding lines)
    awk -v F="$f" '
        /^[a-zA-Z_].*\)/ && /\{/ { fn_start = NR; fn_name = $0; in_fn = 1; uses_data = 0; uses_crc = 0; fn_end = 0 }
        in_fn && /passes\[.*\]\.data\[/ { uses_data = NR }
        in_fn && /\.crc_ok/ { uses_crc = 1 }
        in_fn && /^\}/ {
            if (uses_data && !uses_crc) {
                print F ":" uses_data ":I3:function uses passes[].data[] but never reads passes[].crc_ok"
            }
            in_fn = 0; uses_data = 0; uses_crc = 0
        }
    ' "$f" | while IFS=: read -r fn line tag msg; do
        sample=$(sed -n "${line}p" "$f")
        WARN "$fn:$line" "$tag" "$msg" "$sample"
    done

    # I5 — write to output[] not paired with confidence_map[] write within ±10 lines
    # Cheap version: collect output[i] = lines, then check if confidence_map[i] appears in nearby window
    grep -n 'output\[[a-zA-Z_]*[ij]' "$f" | while IFS=: read -r line content; do
        # Skip if same line also touches confidence_map
        if echo "$content" | grep -q 'confidence_map'; then continue; fi
        # Check ±10 lines for a confidence_map write
        start=$((line - 10)); [ $start -lt 1 ] && start=1
        end=$((line + 10))
        nearby=$(sed -n "${start},${end}p" "$f")
        if ! echo "$nearby" | grep -q 'confidence_map\['; then
            WARN "$f:$line" "I5" "write to output[] without confidence_map[] update nearby" "$content"
        fi
    done
}

for f in "$@"; do
    scan_file "$f"
done

WARN_COUNT=$(wc -l < "$WARN_FILE" 2>/dev/null || echo 0)
WARN_COUNT=${WARN_COUNT// /}

if [ "$WARN_COUNT" -eq 0 ]; then
    echo "lint_recovery: clean ($# file(s) scanned)"
    exit 0
else
    echo
    echo "lint_recovery: $WARN_COUNT warning(s) — review each, then either fix"
    echo "or annotate the line with /* INTEGRITY-OK: <reason> */ to suppress."
    exit 1
fi
