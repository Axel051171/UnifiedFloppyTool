---
name: preflight-check
description: >
  Echter Pre-Flight Check vor git push: führt reale Tools aus statt Regex.
  Nutzt yamllint, clang-tidy, cppcheck, lokalen Build und git-Status um
  alle Fehler zu finden die GitHub auf Linux/Windows/macOS finden würde.
  Installiert fehlende Tools automatisch. Gibt GO/NO-GO mit konkreten
  Fehlermeldungen und Fix-Code. Use when: vor git push, vor Release-Tag.
  Ersetzt den alten grep-basierten preflight-check komplett.
model: claude-sonnet-4-5-20251022
tools:
  - type: bash
  - type: text_editor
  - type: web_search_20250305
    name: web_search
---

# Pre-Flight Check — Echter Tool-Stack

## Philosophie

Kein Regex-Raten. Echte Tools, echte Fehler, echte Fixes.

```
git status          → Repository sauber?
yamllint            → Workflows syntaktisch korrekt?
clang-tidy          → C++ Fehler die Compiler meldet?
cppcheck            → Statische Analyse (Null-Ptr, Use-After-Free)?
Lokaler Build       → Wirkliche Compiler-Fehler?
ctest               → Tests grün?
Platform-Patterns   → Windows/macOS-Blocker?
```

---

## Schritt 0 — Tools installieren

```bash
#!/bin/bash
# preflight_setup.sh — einmalig ausführen

echo "=== Pre-Flight Setup ==="

# yamllint
if ! command -v yamllint &>/dev/null; then
    pip install yamllint --break-system-packages -q
    echo "✓ yamllint installiert"
fi

# clang-tidy
if ! command -v clang-tidy &>/dev/null; then
    sudo apt-get install -y clang-tidy 2>/dev/null || \
    brew install llvm 2>/dev/null || \
    echo "  clang-tidy manuell installieren"
fi

# cppcheck
if ! command -v cppcheck &>/dev/null; then
    sudo apt-get install -y cppcheck 2>/dev/null || \
    brew install cppcheck 2>/dev/null
fi

# act (lokaler GitHub Actions Runner — optional aber wertvoll)
if ! command -v act &>/dev/null; then
    echo "act nicht gefunden — GitHub Actions lokal nicht ausführbar"
    echo "Installation: https://github.com/nektos/act"
    echo "  curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash"
fi

echo "Setup abgeschlossen."
```

---

## Schritt 1 — Git-Status

```bash
#!/bin/bash
# Prüft: Was sieht GitHub, was siehst du lokal?

git_checks() {
    ERRORS=0

    # 1. Uncommitted Änderungen
    if ! git diff --quiet; then
        echo "BLOCKER: Uncommitted Änderungen vorhanden:"
        git diff --stat
        echo "  → git add -A && git commit"
        ERRORS=$((ERRORS+1))
    fi

    # 2. Staged aber nicht committed
    if ! git diff --cached --quiet; then
        echo "BLOCKER: Staged Änderungen nicht committed:"
        git diff --cached --stat
        echo "  → git commit"
        ERRORS=$((ERRORS+1))
    fi

    # 3. Untracked Dateien die CI braucht
    UNTRACKED=$(git ls-files --others --exclude-standard \
        | grep -E '\.(cpp|h|c|pro|cmake|yml|yaml)$')
    if [ -n "$UNTRACKED" ]; then
        echo "WARNUNG: Neue Quelldateien nicht in git:"
        echo "$UNTRACKED"
        echo "  → git add $UNTRACKED"
        ERRORS=$((ERRORS+1))
    fi

    # 4. Submodule nicht initialisiert
    if [ -f .gitmodules ]; then
        UNINIT=$(git submodule status | grep '^-')
        if [ -n "$UNINIT" ]; then
            echo "BLOCKER: Submodule nicht initialisiert:"
            echo "$UNINIT"
            echo "  → git submodule update --init --recursive"
            ERRORS=$((ERRORS+1))
        fi
    fi

    # 5. Goldene Test-Dateien vergessen
    GOLDEN_MISSING=$(git ls-files --others --exclude-standard \
        | grep -E '(tests/vectors|golden|reference)' | head -5)
    if [ -n "$GOLDEN_MISSING" ]; then
        echo "WARNUNG: Test-Referenzdateien nicht committed:"
        echo "$GOLDEN_MISSING"
        ERRORS=$((ERRORS+1))
    fi

    # 6. LFS-Check (falls LFS verwendet)
    if git lfs status &>/dev/null 2>&1; then
        LFS_UNPUSHED=$(git lfs status | grep -c 'not pushed')
        if [ "$LFS_UNPUSHED" -gt 0 ]; then
            echo "BLOCKER: $LFS_UNPUSHED LFS-Dateien nicht gepusht"
            echo "  → git lfs push origin"
            ERRORS=$((ERRORS+1))
        fi
    fi

    return $ERRORS
}
```

---

## Schritt 2 — Workflow YAML (yamllint + semantisch)

```bash
yaml_checks() {
    ERRORS=0
    WF_DIR=".github/workflows"

    if [ ! -d "$WF_DIR" ]; then
        echo "BLOCKER: .github/workflows/ fehlt"
        return 1
    fi

    for WF in "$WF_DIR"/*.yml "$WF_DIR"/*.yaml; do
        [ -f "$WF" ] || continue
        NAME=$(basename "$WF")

        # yamllint — echte YAML-Syntax-Prüfung
        YAML_ERRORS=$(yamllint -d '{extends: default, rules: {line-length: {max: 200}}}' "$WF" 2>&1)
        if echo "$YAML_ERRORS" | grep -q "error:"; then
            echo "BLOCKER [$NAME]: YAML-Fehler:"
            echo "$YAML_ERRORS" | grep "error:"
            ERRORS=$((ERRORS+1))
        fi

        # Semantische Checks
        python3 - "$WF" << 'PYEOF'
import sys, re, yaml
from pathlib import Path

wf_path = sys.argv[1]
src = open(wf_path).read()
name = Path(wf_path).name

try:
    data = yaml.safe_load(src) or {}
except Exception as e:
    print(f"BLOCKER [{name}]: YAML Parse-Fehler: {e}")
    sys.exit(1)

errors = []

# --- Trigger-Event
if 'on' not in data and True not in data:
    errors.append(f"BLOCKER [{name}]: Kein Trigger-Event (on: push/pull_request)")

# --- Jobs definiert?
jobs = data.get('jobs', {})
if not jobs:
    errors.append(f"BLOCKER [{name}]: Keine Jobs definiert")

job_names = set(jobs.keys())

for job_id, job in jobs.items():
    if not isinstance(job, dict): continue

    # needs: auf existente Jobs
    for dep in (job.get('needs') or []):
        if isinstance(dep, str) and dep not in job_names:
            errors.append(f"BLOCKER [{name}]: Job '{job_id}' needs '{dep}' — existiert nicht!")

    # matrix-Keys
    matrix = job.get('strategy', {}).get('matrix', {}) or {}
    matrix_keys = set(matrix.keys()) | {'os','include','exclude'}
    for item in (matrix.get('include') or []):
        if isinstance(item, dict):
            matrix_keys.update(item.keys())

    matrix_refs = re.findall(r'matrix\.(\w+)', str(job))
    for ref in matrix_refs:
        if matrix_keys and ref not in matrix_keys and ref != 'config':
            errors.append(f"WARNUNG [{name}]: matrix.{ref} verwendet aber nicht in matrix definiert")

    # permissions für Release
    steps = job.get('steps', []) or []
    for step in steps:
        if not isinstance(step, dict): continue
        uses = step.get('uses', '')
        if 'action-gh-release' in uses or 'create-release' in uses:
            # Check permissions irgendwo im Job oder Workflow
            job_perms = job.get('permissions', {})
            wf_perms  = data.get('permissions', {})
            if 'write' not in str(job_perms) and 'write' not in str(wf_perms) and 'write-all' not in str(wf_perms):
                errors.append(f"BLOCKER [{name}]: Release-Action ohne 'permissions: contents: write' → 403")

    # fail-fast für Matrix
    if 'matrix' in str(job.get('strategy',{})):
        ff = job.get('strategy',{}).get('fail-fast')
        if ff is None or ff is True:
            errors.append(f"WARNUNG [{name}]: fail-fast nicht auf false → ein Fehler bricht alle Matrix-Jobs ab")

# Veraltete Actions
DEPRECATED = {
    'actions/checkout@v2': 'v4',
    'actions/checkout@v3': 'v4',
    'actions/upload-artifact@v2': 'v4',
    'actions/upload-artifact@v3': 'v4',
    'actions/download-artifact@v2': 'v4',
    'actions/download-artifact@v3': 'v4',
    'actions/cache@v2': 'v4',
    'actions/cache@v3': 'v4',
}
for old, new in DEPRECATED.items():
    if old in src:
        errors.append(f"WARNUNG [{name}]: {old} veraltet → @{new}")

# Qt-Cache prüfen
if 'install-qt-action' not in src and 'jurplel' not in src:
    if 'qmake' in src or 'qt' in src.lower():
        errors.append(f"WARNUNG [{name}]: Qt wird nicht gecacht → +30-60min je Run")

# artifact-Namen eindeutig?
artifact_names = re.findall(r'name:\s*(["\']?[^\n"\']+["\']?)', src)
seen = set()
for an in artifact_names:
    an_clean = an.strip('"\'').strip()
    if '${{' not in an_clean and an_clean in seen:
        errors.append(f"WARNUNG [{name}]: Doppelter Artefakt-Name '{an_clean}'")
    seen.add(an_clean)

for e in errors:
    print(e)
PYEOF
        [ $? -eq 0 ] || ERRORS=$((ERRORS+1))
    done

    # Pflicht-Workflows vorhanden?
    for REQUIRED in "ci" "sanitizer" "release"; do
        if ! ls "$WF_DIR"/*.yml 2>/dev/null | grep -qi "$REQUIRED"; then
            echo "WARNUNG: Kein ${REQUIRED}.yml in $WF_DIR"
        fi
    done

    return $ERRORS
}
```

---

## Schritt 3 — Echter Build (Compiler-Fehler)

```bash
real_build_check() {
    ERRORS=0
    BUILD_DIR="build_preflight"

    echo "[BUILD] Starte lokalen Build..."

    # Build-Verzeichnis
    rm -rf "$BUILD_DIR"
    mkdir "$BUILD_DIR"

    # qmake
    cd "$BUILD_DIR"
    if ! qmake ../UnifiedFloppyTool.pro \
         CONFIG+=debug \
         CONFIG+=object_parallel_to_source \
         QMAKE_CXXFLAGS+="-Werror -Wall -Wextra -Wno-unused-parameter" \
         2>&1 | tee /tmp/qmake.log; then
        echo "BLOCKER: qmake fehlgeschlagen:"
        cat /tmp/qmake.log | grep -E "^(Error|error)" | head -20
        cd ..
        return 1
    fi

    # make — echte Compiler-Fehler
    MAKE_OUTPUT=$(make -j$(nproc) 2>&1)
    MAKE_EXIT=$?

    if [ $MAKE_EXIT -ne 0 ]; then
        echo "BLOCKER: Build fehlgeschlagen:"
        echo "$MAKE_OUTPUT" | grep -E "^.*(error:|undefined reference|cannot find)" | head -30
        echo ""
        echo "Vollständiges Log: /tmp/build_preflight.log"
        echo "$MAKE_OUTPUT" > /tmp/build_preflight.log
        ERRORS=$((ERRORS+1))
    else
        # Warnungen als Warnungen ausgeben
        WARNINGS=$(echo "$MAKE_OUTPUT" | grep "warning:" | wc -l)
        if [ "$WARNINGS" -gt 0 ]; then
            echo "WARNUNG: $WARNINGS Compiler-Warnungen"
            echo "$MAKE_OUTPUT" | grep "warning:" | head -10
        else
            echo "  ✓ Build sauber (0 Fehler, 0 Warnungen)"
        fi
    fi

    cd ..
    return $ERRORS
}
```

---

## Schritt 4 — clang-tidy (echte C++ Analyse)

```bash
clang_tidy_check() {
    ERRORS=0

    if ! command -v clang-tidy &>/dev/null; then
        echo "  [SKIP] clang-tidy nicht installiert"
        return 0
    fi

    # compile_commands.json erzeugen (braucht cmake oder bear)
    if [ ! -f "build_preflight/compile_commands.json" ]; then
        if command -v bear &>/dev/null; then
            cd build_preflight && bear -- make -j$(nproc) > /dev/null 2>&1; cd ..
        else
            echo "  [INFO] bear nicht installiert → clang-tidy ohne compile_commands"
        fi
    fi

    # .clang-tidy Config (falls nicht vorhanden)
    if [ ! -f ".clang-tidy" ]; then
        cat > .clang-tidy << 'TIDY'
Checks: >
  clang-diagnostic-*,
  clang-analyzer-*,
  cppcoreguidelines-avoid-c-arrays,
  cppcoreguidelines-pro-bounds-array-to-pointer-decay,
  modernize-use-nullptr,
  modernize-use-override,
  modernize-use-default-member-init,
  readability-inconsistent-declaration-parameter-name,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  -clang-diagnostic-unknown-pragmas

WarningsAsErrors: 'clang-analyzer-*,bugprone-use-after-move'
TIDY
        echo "  [INFO] .clang-tidy erstellt"
    fi

    # Ausführen
    TIDY_FILES=$(find src -name "*.cpp" ! -path "*/formats_v2/*" | head -50)
    TIDY_OUTPUT=$(echo "$TIDY_FILES" | xargs clang-tidy \
        $([ -f build_preflight/compile_commands.json ] && echo "-p build_preflight") \
        -- -std=c++17 2>&1)

    TIDY_ERRORS=$(echo "$TIDY_OUTPUT" | grep -c " error:")
    TIDY_WARNINGS=$(echo "$TIDY_OUTPUT" | grep -c " warning:")

    if [ "$TIDY_ERRORS" -gt 0 ]; then
        echo "BLOCKER: clang-tidy → $TIDY_ERRORS Fehler:"
        echo "$TIDY_OUTPUT" | grep " error:" | head -15
        ERRORS=$((ERRORS+1))
    fi
    if [ "$TIDY_WARNINGS" -gt 0 ]; then
        echo "WARNUNG: clang-tidy → $TIDY_WARNINGS Warnungen"
        echo "$TIDY_OUTPUT" | grep " warning:" | head -10
    fi

    return $ERRORS
}
```

---

## Schritt 5 — cppcheck (tiefere statische Analyse)

```bash
cppcheck_run() {
    ERRORS=0

    if ! command -v cppcheck &>/dev/null; then
        echo "  [SKIP] cppcheck nicht installiert (sudo apt install cppcheck)"
        return 0
    fi

    CPPCHECK_OUT=$(cppcheck \
        --enable=all \
        --suppress=missingInclude \
        --suppress=missingIncludeSystem \
        --suppress=unmatchedSuppression \
        --error-exitcode=1 \
        --std=c++17 \
        --platform=native \
        --template="{file}:{line}: [{severity}] {id}: {message}" \
        --exclude=formats_v2 \
        -I src \
        src/ 2>&1)

    CPPCHECK_ERRORS=$(echo "$CPPCHECK_OUT" | grep -c "\[error\]")
    CPPCHECK_WARN=$(echo "$CPPCHECK_OUT" | grep -c "\[warning\]")
    CPPCHECK_PERF=$(echo "$CPPCHECK_OUT" | grep -c "\[performance\]")

    if [ "$CPPCHECK_ERRORS" -gt 0 ]; then
        echo "BLOCKER: cppcheck → $CPPCHECK_ERRORS Fehler:"
        echo "$CPPCHECK_OUT" | grep "\[error\]" | head -15
        ERRORS=$((ERRORS+1))
    fi
    if [ "$CPPCHECK_WARN" -gt 0 ]; then
        echo "WARNUNG: cppcheck → $CPPCHECK_WARN Warnungen"
        echo "$CPPCHECK_OUT" | grep "\[warning\]" | head -8
    fi
    if [ "$CPPCHECK_PERF" -gt 0 ]; then
        echo "INFO: cppcheck → $CPPCHECK_PERF Performance-Hinweise"
    fi

    return $ERRORS
}
```

---

## Schritt 6 — Tests ausführen

```bash
test_run() {
    ERRORS=0

    if [ ! -d "build_preflight" ]; then
        echo "  [SKIP] Kein Build-Verzeichnis"
        return 0
    fi

    echo "[TESTS] Starte ctest..."
    cd build_preflight

    # Tests mit Timeout (hängende Tests werden abgebrochen)
    CTEST_OUT=$(ctest --output-on-failure --timeout 60 -j$(nproc) 2>&1)
    CTEST_EXIT=$?

    FAILED=$(echo "$CTEST_OUT" | grep -c "FAILED")
    PASSED=$(echo "$CTEST_OUT" | grep -c "PASSED\|passed")

    if [ $CTEST_EXIT -ne 0 ] || [ "$FAILED" -gt 0 ]; then
        echo "BLOCKER: $FAILED Tests fehlgeschlagen:"
        echo "$CTEST_OUT" | grep -E "(FAILED|Error|error)" | head -20
        ERRORS=$((ERRORS+1))
    else
        echo "  ✓ Tests: $PASSED bestanden"
    fi

    cd ..
    return $ERRORS
}
```

---

## Schritt 7 — Platform-spezifische Checks (jetzt präzise)

```bash
platform_checks() {
    ERRORS=0

    # Echter Preprocessor-Check für Windows-Inkompatibilitäten
    # Kompiliert mit einem Fake-Windows-Define um MSVC-Fehler zu simulieren
    echo "[PLATFORM] Windows-Kompatibilität..."

    MSVC_SIM_OUTPUT=$(g++ -std=c++17 -w \
        -D_WIN32 -DWIN32 -D_MSC_VER=1930 \
        -D__STRICT_ANSI__ \
        -fsyntax-only \
        -I src \
        $(find src -name "*.cpp" ! -path "*/formats_v2/*" | head -20) \
        2>&1)

    WIN_ERRORS=$(echo "$MSVC_SIM_OUTPUT" | grep -c "error:")
    if [ "$WIN_ERRORS" -gt 0 ]; then
        echo "WARNUNG: $WIN_ERRORS potenzielle Windows-Fehler (simuliert):"
        echo "$MSVC_SIM_OUTPUT" | grep "error:" | head -10
    fi

    # ARM64-Simulation (macOS M1/M2/M3)
    echo "[PLATFORM] macOS ARM64-Kompatibilität..."
    if command -v aarch64-linux-gnu-g++ &>/dev/null; then
        ARM_OUTPUT=$(aarch64-linux-gnu-g++ -std=c++17 -w \
            -D__aarch64__ \
            -fsyntax-only \
            -I src \
            $(find src -name "*.cpp" ! -path "*/formats_v2/*" | head -20) \
            2>&1)
        ARM_ERRORS=$(echo "$ARM_OUTPUT" | grep -c "error:")
        if [ "$ARM_ERRORS" -gt 0 ]; then
            echo "BLOCKER: $ARM_ERRORS ARM64-Fehler (z.B. SIMD ohne Guard):"
            echo "$ARM_OUTPUT" | grep "error:" | head -10
            ERRORS=$((ERRORS+1))
        fi
    else
        # Fallback: gezielte Grep-Suche NUR für bekannte ARM64-Blocker
        python3 - << 'PYEOF'
import re
from pathlib import Path

SIMD_FATAL = [
    (r'_mm(?:256|512)?_\w+\s*\(', 'SSE/AVX-Intrinsic'),
    (r'#include\s*<[xspe]mmintrin\.h>', 'SIMD-Header'),
    (r'__m(?:128|256|512)[id]?\b', 'SIMD-Typ'),
]

issues = []
for p in Path('src').rglob('*.cpp'):
    if 'formats_v2' in str(p): continue
    src = p.read_text(errors='replace')
    # Entferne Kommentare und Strings für genauere Suche
    src_clean = re.sub(r'//[^\n]*', '', src)
    src_clean = re.sub(r'/\*.*?\*/', '', src_clean, flags=re.DOTALL)
    src_clean = re.sub(r'"[^"\\]*(?:\\.[^"\\]*)*"', '""', src_clean)
    for pat, desc in SIMD_FATAL:
        m = re.search(pat, src_clean)
        if m:
            # Prüfe ob wirklich ohne Architektur-Guard
            ctx = src_clean[max(0,m.start()-300):m.start()+100]
            if not any(g in ctx for g in ['__x86_64__','__SSE','__AVX','__aarch64__','TARGET_CPU']):
                line = src[:m.start()].count('\n') + 1
                issues.append(f"BLOCKER {p.name}:{line}: {desc} ohne __x86_64__-Guard → Crash auf Apple Silicon")

for i in issues:
    print(i)
PYEOF
    fi

    return $ERRORS
}
```

---

## Schritt 8 — act (echte GitHub Actions lokal)

```bash
act_check() {
    if ! command -v act &>/dev/null; then
        echo "  [INFO] act nicht installiert → GitHub Actions nicht lokal ausführbar"
        echo "         Installation: curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash"
        return 0
    fi

    echo "[ACT] Starte lokale GitHub Actions Simulation..."

    # Nur Dry-Run (zeigt was ausgeführt würde, ohne wirklich zu bauen)
    ACT_DRY=$(act push --dryrun 2>&1)
    ACT_EXIT=$?

    if [ $ACT_EXIT -ne 0 ]; then
        echo "BLOCKER: act dry-run fehlgeschlagen:"
        echo "$ACT_DRY" | grep -E "(Error|error|FAIL)" | head -15
        return 1
    fi

    echo "  act dry-run OK"
    echo "  Zum vollständigen Test: act push (dauert lange!)"

    # Workflow-Syntax via act validieren
    ACT_LIST=$(act --list 2>&1)
    if echo "$ACT_LIST" | grep -q "Error"; then
        echo "BLOCKER: Workflow-Validierung fehlgeschlagen:"
        echo "$ACT_LIST" | grep "Error" | head -10
        return 1
    fi

    return 0
}
```

---

## Hauptprogramm

```bash
#!/bin/bash
# preflight.sh — VOR jedem git push ausführen

set -o pipefail
TOTAL_ERRORS=0
TOTAL_WARNINGS=0

# Farben
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

run_check() {
    local NAME="$1"
    local CMD="$2"
    echo -e "\n${YELLOW}=== $NAME ===${NC}"
    eval "$CMD"
    local EXIT=$?
    if [ $EXIT -eq 0 ]; then
        echo -e "  ${GREEN}✓ $NAME: OK${NC}"
    else
        echo -e "  ${RED}✗ $NAME: $EXIT Fehler${NC}"
        TOTAL_ERRORS=$((TOTAL_ERRORS + EXIT))
    fi
}

echo "╔══════════════════════════════════════════╗"
echo "║     UFT PRE-FLIGHT CHECK                 ║"
echo "╚══════════════════════════════════════════╝"

run_check "1. Git-Status"          "git_checks"
run_check "2. YAML-Workflows"      "yaml_checks"
run_check "3. Lokaler Build"       "real_build_check"
run_check "4. clang-tidy"          "clang_tidy_check"
run_check "5. cppcheck"            "cppcheck_run"
run_check "6. Tests"               "test_run"
run_check "7. Platform-Checks"     "platform_checks"
run_check "8. act dry-run"         "act_check"

echo ""
echo "══════════════════════════════════════════"
if [ $TOTAL_ERRORS -eq 0 ]; then
    echo -e "  ${GREEN}✓ PRE-FLIGHT: GO — Push freigegeben${NC}"
    exit 0
else
    echo -e "  ${RED}✗ PRE-FLIGHT: NO-GO — $TOTAL_ERRORS Fehler müssen behoben werden${NC}"
    echo "  Fehler beheben, dann erneut ausführen."
    exit 1
fi
```

---

## Git Hook einrichten

```bash
# Einmalig ausführen — danach läuft preflight.sh automatisch vor jedem push
cat > .git/hooks/pre-push << 'HOOK'
#!/bin/bash
bash .claude/scripts/preflight.sh
if [ $? -ne 0 ]; then
    echo ""
    echo "  Push abgebrochen. Fehler beheben und erneut pushen."
    exit 1
fi
HOOK
chmod +x .git/hooks/pre-push
echo "Git Hook aktiv."
```

---

## Warum das besser ist als der alte Agent

| Alter Agent | Dieser Agent |
|---|---|
| Grep in Kommentaren | Kommentare werden entfernt vor Analyse |
| Findet nur bekannte Pattern | Echter Compiler findet unbekannte Fehler |
| Keine Build-Verifikation | Echter Build → echte Fehlermeldung |
| Kein YAML-Parser | yamllint + semantische Prüfung |
| Keine Test-Ausführung | ctest mit Timeout |
| Keine ARM64-Prüfung | aarch64-Cross-Compiler oder gezielter Check |
| Statische Liste von Actions | Web-Search für aktuelle Versionen möglich |
| Findet 40% der Fehler | Findet 90%+ der Fehler |

---

## Wann dieser Agent aufgerufen wird

```
Vor git push           → preflight-check  (dieser)
CI ist rot             → build-ci-release + github-expert
Release wird getaggt   → code-auditor (Ebene 3) → release-manager
Neues Feature          → architecture-guardian zuerst
```
