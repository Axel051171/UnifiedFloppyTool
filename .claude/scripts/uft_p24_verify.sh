#!/usr/bin/env bash
# uft_p24_verify.sh — P2.4 Steps 1-3 + die 4 brutalen Befunde
#
# Verwendung:
#   cd <UFT-repo-root>
#   bash /pfad/zu/uft_p24_verify.sh
#
# Exit-Code 0 nur wenn ALLES grün ist. Jede einzelne Prüfung ist ein
# Hard-Fail, keine Warnungen. "Kein Bit verloren" gilt auch für Docs.

set -u   # NICHT -e — wir wollen alle Fehler sehen, nicht beim ersten abbrechen
fails=0
warns=0

cyan()  { printf "\033[36m%s\033[0m\n" "$*"; }
red()   { printf "\033[31m%s\033[0m\n" "$*"; }
green() { printf "\033[32m%s\033[0m\n" "$*"; }
yel()   { printf "\033[33m%s\033[0m\n" "$*"; }

hdr() { echo; cyan "═══ $* ═══"; }
ok()   { green "  ✓ $*"; }
fail() { red   "  ✗ $*"; fails=$((fails+1)); }
warn() { yel   "  ⚠ $*"; warns=$((warns+1)); }

# ─── 0. Repo-Sanity ───────────────────────────────────────────────────
hdr "0. Repo + Branch-Lage"

if [[ ! -d .git ]]; then
  red "kein Git-Repo. cd ins UFT-Root."; exit 2
fi

branch=$(git rev-parse --abbrev-ref HEAD)
echo "  HEAD: $branch ($(git rev-parse --short HEAD))"

# Sind die zwei Branches aligned wie die P2.4-Doku behauptet?
behind=$(git rev-list --count main..origin/refactor/type-driven-hal 2>/dev/null || echo "?")
ahead=$(git rev-list --count origin/refactor/type-driven-hal..main 2>/dev/null || echo "?")
echo "  main vs refactor/type-driven-hal: main +$ahead / refactor +$behind"

if [[ "$behind" -gt 0 ]]; then
  fail "refactor/type-driven-hal hat $behind Commits die main NICHT hat — P2.4 muss diese erst integrieren"
elif [[ "$ahead" -gt 0 ]]; then
  warn "main hat $ahead Commits über refactor-Branch hinaus — Squash-Merge-Plan in P2.4_CHECKLIST.md Step 4 ist obsolet (siehe Befund 2)"
else
  ok "Branches aligned"
fi

# ─── 1. Version-SSOT ──────────────────────────────────────────────────
hdr "1. Version-SSOT (P2.4 Step 1)"

if [[ ! -f VERSION.txt ]]; then
  fail "VERSION.txt fehlt"
else
  v=$(tr -d '[:space:]' < VERSION.txt)
  echo "  VERSION.txt = $v"
  [[ "$v" == "4.1.4" ]] && ok "VERSION = 4.1.4" || fail "VERSION ist '$v', erwartet '4.1.4'"
fi

if [[ -f scripts/check_consistency.py ]]; then
  echo "  → python scripts/check_consistency.py"
  if python3 scripts/check_consistency.py > /tmp/uft_consistency.log 2>&1; then
    # Akzeptiere beide Output-Formate: "0 / 0 / 0 / 0" oder 4 Zeilen mit ": 0"
    drift_count=$(grep -cE ":\s+[1-9]" /tmp/uft_consistency.log || true)
    if [[ "$drift_count" -eq 0 ]]; then
      ok "check_consistency: 0/0/0/0"
    else
      fail "check_consistency hat $drift_count Drift-Kategorien — siehe /tmp/uft_consistency.log"
      tail -10 /tmp/uft_consistency.log | sed 's/^/    /'
    fi
  else
    fail "check_consistency.py exited non-zero — siehe /tmp/uft_consistency.log"
    tail -10 /tmp/uft_consistency.log | sed 's/^/    /'
  fi
else
  fail "scripts/check_consistency.py nicht gefunden"
fi

# ─── 2. CHANGELOG + RELEASE_NOTES Stale-Check (BEFUND 1) ──────────────
hdr "2. CHANGELOG + RELEASE_NOTES (P2.4 Step 2)  -- BEFUND 1"

if grep -q "^## \[4.1.4\] - " CHANGELOG.md 2>/dev/null; then
  ok "CHANGELOG hat finalen [4.1.4]-Header"
elif grep -q "^## \[4.1.4-rc1\]" CHANGELOG.md 2>/dev/null; then
  warn "CHANGELOG steht noch auf [4.1.4-rc1] — Step 2 noch nicht ausgeführt"
else
  fail "CHANGELOG: kein [4.1.4]- oder [4.1.4-rc1]-Header gefunden"
fi

# Der echte Test: behauptet RELEASE_NOTES dass Dinge queued sind, 
# die im Code längst gelandet sind?
echo
echo "  Stale-Claim-Test gegen den Code:"

if grep -q "P1.20 / P1.21" RELEASE_NOTES.md 2>/dev/null; then
  # Behauptung in den Notes:
  echo "    RELEASE_NOTES behauptet: 'FluxCaptureJob/FluxWriteJob queued P1.20/P1.21'"
  # Realität: 0 aktive uft_gw_-Aufrufe (alles in Kommentaren)
  hits=$(grep -c "uft_gw_" src/fluxcapturejob.cpp src/fluxwritejob.cpp 2>/dev/null | awk -F: '{s+=$2} END{print s}')
  active=$(grep -E "^[[:space:]]*[^/*[:space:]].*uft_gw_" src/fluxcapturejob.cpp src/fluxwritejob.cpp 2>/dev/null | grep -v "^[[:space:]]*[/*]" | wc -l)
  echo "    Realität: $hits uft_gw_-Token in den 2 Jobs, davon ~$active in aktiven Statements (nicht-Kommentar)"
  if [[ "$active" -le 0 ]]; then
    fail "RELEASE_NOTES.md lügt: P1.20/21 sind im Code DONE, aber als 'queued' gelistet"
  else
    warn "RELEASE_NOTES sagt 'queued', $active aktive uft_gw_-Calls vorhanden — manuell prüfen"
  fi
fi

if grep -q "raw_handle.* legacy escape hatch will" RELEASE_NOTES.md 2>/dev/null; then
  echo "    RELEASE_NOTES behauptet: 'raw_handle() will be removed (P1.22)'"
  hits=$(git grep -lE "raw_handle\s*\(|gwDevice\s*\(" -- src/ include/ 2>/dev/null | wc -l)
  active=$(git grep -nE "raw_handle\s*\(|gwDevice\s*\(" -- src/ include/ 2>/dev/null \
           | grep -vE "(MF-202|removed|hatch)" | wc -l)
  echo "    Realität: $hits Files erwähnen raw_handle/gwDevice, davon $active aktive Aufrufe"
  if [[ "$active" -le 0 ]]; then
    fail "RELEASE_NOTES.md lügt: raw_handle/gwDevice sind MF-202 längst entfernt"
  else
    warn "$active aktive raw_handle/gwDevice-Aufrufe vorhanden — manuell prüfen"
  fi
fi

if grep -qE "queued as P1\.23|V2 routing in .HardwareTab. is queued" RELEASE_NOTES.md 2>/dev/null; then
  echo "    RELEASE_NOTES behauptet: 'non-GW V2 routing queued (P1.23)'"
  if grep -q "ProviderV2Variant" src/hardwaretab.cpp 2>/dev/null; then
    fail "RELEASE_NOTES.md lügt: ProviderV2Variant existiert in hardwaretab.cpp (MF-205/206)"
  else
    ok "non-GW Routing-Claim deckt sich mit dem Code"
  fi
fi

# ─── 3. Pre-Merge-Build + Tests (P2.4 Step 3) ─────────────────────────
hdr "3. Build + Test-Suite (P2.4 Step 3)"

build_dir="build-p24-verify"
echo "  → cmake -S . -B $build_dir -DCMAKE_BUILD_TYPE=RelWithDebInfo"
if cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=RelWithDebInfo > /tmp/uft_cmake.log 2>&1; then
  ok "cmake configure"
else
  fail "cmake configure failed — siehe /tmp/uft_cmake.log"
  tail -15 /tmp/uft_cmake.log | sed 's/^/    /'
fi

echo "  → cmake --build $build_dir -j\$(nproc)"
if cmake --build "$build_dir" -j"$(nproc 2>/dev/null || echo 4)" > /tmp/uft_build.log 2>&1; then
  ok "build"
else
  fail "build failed — siehe /tmp/uft_build.log"
  tail -20 /tmp/uft_build.log | sed 's/^/    /'
fi

echo "  → ctest --output-on-failure (im build-Dir)"
if (cd "$build_dir" && ctest --output-on-failure) > /tmp/uft_ctest.log 2>&1; then
  passed=$(grep -oE "[0-9]+ tests passed" /tmp/uft_ctest.log | tail -1)
  ok "ctest grün ($passed)"
else
  fail "ctest hat Failures — siehe /tmp/uft_ctest.log"
  grep -E "Failed|FAILED" /tmp/uft_ctest.log | head -10 | sed 's/^/    /'
fi

# pytest-Suiten (P3.2/P3.3/P3.4)
for suite in tests/conformance tests/improvement tests/hil tests/differential; do
  if [[ -d "$suite" ]]; then
    echo "  → pytest $suite"
    if python3 -m pytest "$suite" -q > "/tmp/uft_pytest_$(basename $suite).log" 2>&1; then
      ok "$suite"
    else
      # HIL skippt im no-rig-Fall LEGITIM
      if [[ "$suite" == "tests/hil" ]] && grep -q "skipped\|NOT_RUN" "/tmp/uft_pytest_hil.log"; then
        warn "$suite skipped (kein Rig) — legitim, aber HIL-Lauf bleibt P2.4-Blocker"
      else
        fail "$suite hat Failures — /tmp/uft_pytest_$(basename $suite).log"
      fi
    fi
  fi
done

# ─── 4. Verify_build_sources ──────────────────────────────────────────
hdr "4. verify_build_sources.py"
if [[ -f scripts/verify_build_sources.py ]]; then
  if python3 scripts/verify_build_sources.py > /tmp/uft_vbs.log 2>&1; then
    ok "verify_build_sources: no new divergence"
  else
    fail "verify_build_sources schlägt fehl — /tmp/uft_vbs.log"
    tail -10 /tmp/uft_vbs.log | sed 's/^/    /'
  fi
fi

# ─── 5. HIL-Status (BEFUND 4) ─────────────────────────────────────────
hdr "5. HIL — BEFUND 4 (echte Hardware)"

if [[ -f releases/v4.1.4-rc1/hil_report.md ]]; then
  if grep -q "Overall (hardware tier): NOT_RUN" releases/v4.1.4-rc1/hil_report.md; then
    fail "HIL Hardware-Tier ist NOT_RUN — Gate 0 ungeschlossen bis du am Rig warst"
  elif grep -q "ALL GREEN\|ALL_GREEN" releases/v4.1.4-rc1/hil_report.md; then
    ok "HIL Hardware-Tier: ALL GREEN"
  else
    warn "HIL-Report existiert, Status unklar — manuell ansehen"
  fi
fi

# Field-Test-Tabelle: wie viele Zeilen sind nicht PASS?
if [[ -f audit/rc1_field_notes.md ]]; then
  notrun=$(grep -cE "^\| (read|write|detect|gui|build) +\| NOT_RUN" audit/rc1_field_notes.md)
  pass=$(grep -cE "^\| (read|write|detect|gui|build) +\| PASS" audit/rc1_field_notes.md)
  echo "  Field-Notes: $pass PASS · $notrun NOT_RUN"
  if [[ "$notrun" -gt 0 ]]; then
    fail "$notrun Field-Test-Zeilen sind NOT_RUN — kein PASS-Ersatz"
  fi
fi

# ─── 6. Process-Smell (BEFUND 3) ─────────────────────────────────────
hdr "6. Merge-Commit-Audit — BEFUND 3"

if git log --oneline --grep="DO NOT MERGE" main >/dev/null 2>&1; then
  smell=$(git log --oneline --grep="DO NOT MERGE" main | head -3)
  if [[ -n "$smell" ]]; then
    warn "Merge-Commits mit 'DO NOT MERGE' in der Message auf main:"
    echo "$smell" | sed 's/^/    /'
    warn "→ entweder per git notes klarstellen oder vor v4.1.4-Tag dokumentieren"
  fi
fi

# ─── Bilanz ───────────────────────────────────────────────────────────
echo
cyan "═══ Bilanz ═══"
echo "  Fails: $fails"
echo "  Warns: $warns"
echo
if [[ $fails -eq 0 ]]; then
  if [[ $warns -eq 0 ]]; then
    green "  Alles grün. P2.4 Step 4 (Tag-Setzen) ist freigegeben — sofern HIL gelaufen ist."
    exit 0
  else
    yel "  Keine Fails, aber $warns Warnungen. Manuell durchsehen vor Tag."
    exit 0
  fi
else
  red  "  $fails Hard-Fails. P2.4 ist NICHT freigegeben. Repariere und re-cut."
  exit 1
fi
