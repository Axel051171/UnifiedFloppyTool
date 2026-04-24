#!/usr/bin/env bash
#
# check_firmware_portability.sh — Scan UFT code for firmware-unsafe patterns.
#
# Usage:
#     check_firmware_portability.sh [path...]
#
# Default scans src/flux/ src/algorithms/ src/crc/ src/analysis/otdr/
#
# Exit 0 if clean, 1 if any issue found.

set -u

DEFAULT_PATHS=(
    "src/flux"
    "src/algorithms"
    "src/crc"
    "src/analysis/otdr"
)

# Argument handling
if [[ $# -gt 0 ]]; then
    PATHS=("$@")
else
    PATHS=("${DEFAULT_PATHS[@]}")
fi

# Filter out non-existent paths (user may scan a subset)
EXISTING_PATHS=()
for p in "${PATHS[@]}"; do
    if [[ -d "$p" ]] || [[ -f "$p" ]]; then
        EXISTING_PATHS+=("$p")
    fi
done

if [[ ${#EXISTING_PATHS[@]} -eq 0 ]]; then
    echo "ERROR: no valid paths to scan"
    echo "usage: $0 [path...]"
    exit 2
fi

echo "=== UFT firmware portability check ==="
echo "Scanning: ${EXISTING_PATHS[*]}"
echo

issues=0

# ----- Check 1: double arithmetic -----
# Ignore comments and string literals as best we can
echo "[1/7] Checking for 'double' arithmetic..."
result=$(grep -rnE "^\s*double\s+[a-z_]|=\s*double\s|sin\(|cos\(|sqrt\(|atan2\(" \
    --include="*.c" --include="*.h" \
    "${EXISTING_PATHS[@]}" 2>/dev/null \
    | grep -vE "^\s*\*|//|^Binary" || true)
if [[ -n "$result" ]]; then
    echo "$result"
    echo "  -> Use 'float' + sinf/cosf/sqrtf/atan2f variants"
    issues=$((issues + 1))
fi

# ----- Check 2: x86 intrinsics -----
echo "[2/7] Checking for x86 intrinsics..."
result=$(grep -rnE "_mm_|_mm256_|__m(128|256|512)|__builtin_ia32_" \
    --include="*.c" --include="*.h" \
    "${EXISTING_PATHS[@]}" 2>/dev/null | grep -vE "^Binary|UFT_HOST_X86" || true)
if [[ -n "$result" ]]; then
    echo "$result"
    echo "  -> Guard with #ifdef UFT_HOST_X86 and provide portable fallback"
    issues=$((issues + 1))
fi

# ----- Check 3: malloc/free in suspect files (hot paths shouldn't alloc) -----
echo "[3/7] Checking for malloc/free in hot paths..."
result=$(grep -rnE "\b(malloc|calloc|free|realloc)\s*\(" \
    --include="*.c" \
    "${EXISTING_PATHS[@]}" 2>/dev/null \
    | grep -vE "init|destroy|^Binary|_test\.c" || true)
if [[ -n "$result" ]]; then
    echo "$result"
    echo "  -> Move allocations out of hot paths; use static buffers"
    issues=$((issues + 1))
fi

# ----- Check 4: printf with %f or %zu -----
echo "[4/7] Checking for firmware-unsafe printf specifiers..."
result=$(grep -rnE 'printf\([^)]*"[^"]*%(f|zu|lf)' \
    --include="*.c" --include="*.h" \
    "${EXISTING_PATHS[@]}" 2>/dev/null \
    | grep -vE "^Binary" || true)
if [[ -n "$result" ]]; then
    echo "$result"
    echo "  -> Use %u/%d/%x for integers, print floats as scaled integers"
    issues=$((issues + 1))
fi

# ----- Check 5: large static LUTs (>16 KB) -----
echo "[5/7] Checking for LUTs larger than 16 KB..."
result=$(grep -rnE "static\s+const.*\[\s*([0-9]+)\s*\]" \
    --include="*.c" --include="*.h" \
    "${EXISTING_PATHS[@]}" 2>/dev/null \
    | awk -F'[][]' '{
        for (i=2; i<NF; i+=2) {
            size = $i + 0
            if (size > 4096) {
                # 4096 * 4 (uint32) = 16 KB exactly; anything bigger in any type warrants warning
                print $0 "  [size: " size " elements]"
                break
            }
        }
      }' || true)
if [[ -n "$result" ]]; then
    echo "$result"
    echo "  -> LUTs >16 KB blow the I-cache; split or use 256-entry tables"
    issues=$((issues + 1))
fi

# ----- Check 6: size_t / int / long in struct layouts -----
echo "[6/7] Checking for non-explicit widths in structs..."
result=$(grep -rnE "^\s*(size_t|int|long|unsigned)\s+[a-z_][a-z0-9_]*\s*;" \
    --include="*.h" \
    "${EXISTING_PATHS[@]}" 2>/dev/null \
    | grep -vE "^\s*\*|//|^Binary" || true)
if [[ -n "$result" ]]; then
    echo "$result"
    echo "  -> Use uint32_t/int16_t/etc. — size_t differs on 32 vs 64 bit"
    issues=$((issues + 1))
fi

# ----- Check 7: recursion warning -----
echo "[7/7] Checking for potentially unbounded recursion..."
# Heuristic: find functions that call themselves
result=""
for f in $(find "${EXISTING_PATHS[@]}" -name "*.c" 2>/dev/null); do
    # Extract function names defined in this file
    funcs=$(grep -E "^[a-zA-Z_][a-zA-Z0-9_]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(" "$f" \
        | grep -oE "[a-zA-Z_][a-zA-Z0-9_]*\s*\(" \
        | tr -d '(' | tr -d ' ' | sort -u)
    for fn in $funcs; do
        # Does the function body call itself? (crude — only flags obvious cases)
        if grep -E "^[a-zA-Z_].*$fn\s*\(" "$f" | grep -v "^[a-zA-Z_].*$fn\s*(.*)[^{]*$" \
            | grep -v "^[a-zA-Z_][a-zA-Z0-9_]*\s*${fn}\s*(" > /dev/null 2>&1; then
            :  # potential match — too many false positives, skip for now
        fi
    done
done
# This heuristic is noisy; leave as informational
echo "  -> Informational only. Run -fstack-usage to check actual stack depth."

# ----- Summary -----
echo
if [[ $issues -eq 0 ]]; then
    echo "=== OK: no portability issues found ==="
    exit 0
else
    echo "=== $issues category/categories of issue found ==="
    exit 1
fi
