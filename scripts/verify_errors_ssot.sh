#!/usr/bin/env bash
# scripts/verify_errors_ssot.sh
#
# Regenerates the error-subsystem derived files into a temp dir and diffs
# them against the committed copies. Exit 0 = in sync, 1 = drift (commit
# stale), 2 = generator error.
#
# Wire into CI via tests/CMakeLists.txt:
#   add_test(NAME ssot_errors COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/verify_errors_ssot.sh)
#   set_tests_properties(ssot_errors PROPERTIES LABELS ssot-compliance)
#
# Intended to also be runnable as a pre-commit hook.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TSV="${REPO_ROOT}/data/errors.tsv"
LEGACY_TSV="${REPO_ROOT}/data/errors_legacy_aliases.tsv"
GEN_DIR="${REPO_ROOT}/scripts/generators"

if [[ ! -f "${TSV}" ]]; then
    echo "verify_errors_ssot: ${TSV} missing" >&2
    exit 2
fi
if [[ ! -f "${LEGACY_TSV}" ]]; then
    echo "verify_errors_ssot: ${LEGACY_TSV} missing" >&2
    exit 2
fi

# Prefer python3; fall back to python on Windows/MinGW.
PY="$(command -v python3 || command -v python || true)"
if [[ -z "${PY}" ]]; then
    echo "verify_errors_ssot: no python interpreter on PATH" >&2
    exit 2
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "${TMPDIR}"' EXIT

# 1. Regenerate into TMPDIR.
"${PY}" "${GEN_DIR}/gen_errors_h.py"       "${TSV}" > "${TMPDIR}/uft_error.h"
"${PY}" "${GEN_DIR}/gen_errors_strings.py" "${TSV}" > "${TMPDIR}/uft_error_strings.c"
"${PY}" "${GEN_DIR}/gen_errors_compat.py"  "${TSV}" "${LEGACY_TSV}" > "${TMPDIR}/uft_error_compat_gen.h"

# 2. Compare against committed copies. Post-cutover (Phase 4b-4, 2026-04-23)
#    the generator is canonical — any drift is a real error, not a warning.
#    The SSOT_ERRORS_CUTOVER env var is retained as a local-dev escape
#    hatch (set to 1 to downgrade drift to a warning while regenerating),
#    but the default is now hard-fail. CI must not set it.
STATUS=0
CUTOVER="${SSOT_ERRORS_CUTOVER:-0}"
check_one() {
    local generated="$1" committed="$2" label="$3"
    if [[ ! -s "${generated}" ]]; then
        echo "verify_errors_ssot: ${label} generator produced empty output" >&2
        STATUS=2
        return
    fi
    if [[ ! -f "${committed}" ]]; then
        echo "verify_errors_ssot: ${label} missing from tree (${committed})" >&2
        STATUS=1
        return
    fi
    if ! diff -u "${committed}" "${generated}" > "${TMPDIR}/${label}.diff"; then
        if [[ "${CUTOVER}" == "1" ]]; then
            echo "verify_errors_ssot: drift in ${label} (SSOT_ERRORS_CUTOVER=1 — WARN only)" >&2
        else
            echo "verify_errors_ssot: DRIFT in ${label}" >&2
            echo "  committed:  ${committed}" >&2
            echo "  regenerated: ${generated}" >&2
            echo "  run: make -C ${REPO_ROOT} generate-errors" >&2
            head -n 40 "${TMPDIR}/${label}.diff" >&2 || true
            STATUS=1
        fi
    fi
}

check_one "${TMPDIR}/uft_error.h" \
          "${REPO_ROOT}/include/uft/uft_error.h" \
          "uft_error.h"
check_one "${TMPDIR}/uft_error_strings.c" \
          "${REPO_ROOT}/src/core/uft_error_strings.c" \
          "uft_error_strings.c"
check_one "${TMPDIR}/uft_error_compat_gen.h" \
          "${REPO_ROOT}/include/uft/core/uft_error_compat_gen.h" \
          "uft_error_compat_gen.h"

if [[ ${STATUS} -eq 0 ]]; then
    echo "verify_errors_ssot: OK"
fi
exit ${STATUS}
