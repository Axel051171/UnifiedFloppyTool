#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════════
# UFT Pre-Push Validator v2.0
# ═══════════════════════════════════════════════════════════════════════════════
#
# Prüft ALLE bekannten CI-Fehlerquellen LOKAL bevor du pushst.
# Verhindert: "funktioniert lokal, bricht in CI"
#
# USAGE:
#   ./scripts/validate.sh           # Alle Checks
#   ./scripts/validate.sh --quick   # Nur schnelle Checks (kein Build)
#   ./scripts/validate.sh --fix     # Auto-Fix wo möglich
#   ./scripts/validate.sh --staged  # Nur staged files prüfen
#
# INSTALLATION als Git Hook:
#   ln -sf ../../scripts/validate.sh .git/hooks/pre-push
#
# ═══════════════════════════════════════════════════════════════════════════════

# Don't use set -e - we want to collect all errors, not exit on first one

# ═══════════════════════════════════════════════════════════════════════════════
# Configuration
# ═══════════════════════════════════════════════════════════════════════════════

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

ERRORS=0
WARNINGS=0
FIXED=0

QUICK=0
FIX=0
STAGED_ONLY=0
VERBOSE=0

# Parse arguments
for arg in "$@"; do
    case $arg in
        --quick|-q) QUICK=1 ;;
        --fix|-f) FIX=1 ;;
        --staged|-s) STAGED_ONLY=1 ;;
        --verbose|-v) VERBOSE=1 ;;
        --help|-h)
            echo "UFT Pre-Push Validator"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --quick, -q    Skip build test (faster)"
            echo "  --fix, -f      Auto-fix issues where possible"
            echo "  --staged, -s   Only check staged files"
            echo "  --verbose, -v  Show more details"
            echo "  --help, -h     Show this help"
            exit 0
            ;;
    esac
done

# ═══════════════════════════════════════════════════════════════════════════════
# Helper Functions
# ═══════════════════════════════════════════════════════════════════════════════

section() {
    echo ""
    echo -e "${CYAN}┌─────────────────────────────────────────────────────────────┐${NC}"
    echo -e "${CYAN}│${NC} ${BOLD}$1${NC}"
    echo -e "${CYAN}└─────────────────────────────────────────────────────────────┘${NC}"
}

ok() { echo -e "  ${GREEN}✓${NC} $1"; }
warn() { echo -e "  ${YELLOW}⚠${NC} $1"; WARNINGS=$((WARNINGS + 1)); }
fail() { echo -e "  ${RED}✗${NC} $1"; ERRORS=$((ERRORS + 1)); }
fix() { echo -e "  ${GREEN}⚡${NC} Fixed: $1"; FIXED=$((FIXED + 1)); }
info() { [ "$VERBOSE" = "1" ] && echo -e "  ${BLUE}ℹ${NC} $1"; }

get_files() {
    local pattern=$1
    if [ "$STAGED_ONLY" = "1" ]; then
        git diff --cached --name-only --diff-filter=ACM | grep -E "$pattern" 2>/dev/null || true
    else
        find . -type f -name "$pattern" 2>/dev/null | grep -v build | grep -v ".git" || true
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 1: Windows.h Reserved Names
# Diese Variablennamen werden von Windows.h als Makros definiert!
# ═══════════════════════════════════════════════════════════════════════════════

check_windows_reserved() {
    section "Windows.h Reserved Names"
    
    # Windows.h definiert diese als Makros - NIEMALS als Variablennamen verwenden!
    # small -> char, near -> __near, far -> __far, etc.
    
    local found=0
    
    while IFS= read -r file; do
        [ -z "$file" ] && continue
        
        # Suche nach Variablendeklarationen mit reservierten Namen
        while IFS= read -r match; do
            [ -z "$match" ] && continue
            
            local linenum=$(echo "$match" | cut -d: -f1)
            local content=$(echo "$match" | cut -d: -f2-)
            
            if [ "$FIX" = "1" ]; then
                # Auto-fix: Rename
                sed -i "${linenum}s/\bsmall\b/small_buf/g" "$file"
                sed -i "${linenum}s/\bnear\b/near_val/g" "$file"
                sed -i "${linenum}s/\bfar\b/far_val/g" "$file"
                fix "$file:$linenum"
            else
                fail "$file:$linenum - Reserved name: $content"
                found=1
            fi
        done < <(grep -n -E "^\s*(uint8_t|int8_t|char|int|unsigned|void)\s+(small|near|far)\s*[\[;,=]" "$file" 2>/dev/null || true)
        
    done < <(find src tests -name "*.c" -o -name "*.h" 2>/dev/null)
    
    [ $found -eq 0 ] && ok "No Windows.h reserved name conflicts"
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 2: Qt Module Dependencies
# Stellt sicher dass alle verwendeten Qt-Module auch gelinkt werden
# ═══════════════════════════════════════════════════════════════════════════════

check_qt_modules() {
    section "Qt Module Dependencies"
    
    [ ! -f "CMakeLists.txt" ] && { warn "No CMakeLists.txt found"; return; }
    
    # Map: Qt-Include -> Modul-Name | CMake-Target | CI-Modul
    declare -A QT_MODULE=()
    declare -A QT_TARGET=()
    declare -A QT_CI=()
    
    QT_MODULE["QSerialPort"]="SerialPort"
    QT_TARGET["QSerialPort"]="Qt6::SerialPort"
    QT_CI["QSerialPort"]="qtserialport"
    
    QT_MODULE["QSerialPortInfo"]="SerialPort"
    QT_TARGET["QSerialPortInfo"]="Qt6::SerialPort"
    QT_CI["QSerialPortInfo"]="qtserialport"
    
    QT_MODULE["QNetworkAccessManager"]="Network"
    QT_TARGET["QNetworkAccessManager"]="Qt6::Network"
    QT_CI["QNetworkAccessManager"]="qtnetwork"
    
    for include in "${!QT_MODULE[@]}"; do
        if grep -rq "$include" src/*.cpp src/*.h 2>/dev/null; then
            local module="${QT_MODULE[$include]}"
            local target="${QT_TARGET[$include]}"
            local ci_module="${QT_CI[$include]}"
            
            # Check CMakeLists.txt
            if ! grep -q "$target" CMakeLists.txt; then
                fail "Code uses $include but $target not in CMakeLists.txt"
            else
                ok "$target is linked"
            fi
            
            # Check CI workflow
            if [ -f ".github/workflows/ci.yml" ]; then
                if ! grep -q "$ci_module" .github/workflows/ci.yml; then
                    fail "CI missing module: $ci_module"
                fi
            fi
        fi
    done
    
    ok "Qt module check complete"
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 3: Windows Linker Dependencies
# Prüft ob Windows-spezifische Libraries gelinkt werden
# ═══════════════════════════════════════════════════════════════════════════════

check_windows_libs() {
    section "Windows Library Dependencies"
    
    # Map: API-Funktion -> Benötigte Library
    declare -A WIN_LIBS=(
        ["PathMatchSpec"]="shlwapi"
        ["PathFindExtension"]="shlwapi"
        ["PathRemoveExtension"]="shlwapi"
        ["SHGetFolderPath"]="shell32"
        ["CoInitialize"]="ole32"
        ["RegOpenKey"]="advapi32"
        ["WinHttpOpen"]="winhttp"
        ["InternetOpen"]="wininet"
    )
    
    for func in "${!WIN_LIBS[@]}"; do
        if grep -rq "$func" src/ --include="*.c" --include="*.cpp" 2>/dev/null; then
            lib="${WIN_LIBS[$func]}"
            
            if ! grep -qi "$lib" CMakeLists.txt; then
                fail "Code uses $func() but $lib not linked"
                
                if [ "$FIX" = "1" ]; then
                    # Finde passende Stelle und füge hinzu
                    if grep -q "if(WIN32)" CMakeLists.txt; then
                        sed -i "/if(WIN32)/a\    target_link_libraries(\${PROJECT_NAME} PRIVATE $lib)" CMakeLists.txt
                        fix "Added $lib to CMakeLists.txt"
                    fi
                fi
            else
                ok "$lib is linked for $func()"
            fi
        fi
    done
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 4: Include Guards
# ═══════════════════════════════════════════════════════════════════════════════

check_include_guards() {
    section "Include Guards"
    
    local missing=0
    local checked=0
    
    while IFS= read -r h; do
        [ -z "$h" ] && continue
        checked=$((checked + 1))
        
        if ! head -25 "$h" | grep -qE "^#ifndef|^#pragma once"; then
            if [ "$FIX" = "1" ]; then
                # Add #pragma once at top
                local tmp=$(mktemp)
                echo "#pragma once" > "$tmp"
                echo "" >> "$tmp"
                cat "$h" >> "$tmp"
                mv "$tmp" "$h"
                fix "$h - Added #pragma once"
            else
                warn "$h - Missing include guard"
                missing=$((missing + 1))
            fi
        fi
    done < <(find include src -name "*.h" 2>/dev/null | head -100)
    
    if [ $missing -eq 0 ]; then
        ok "Checked $checked headers - all have guards"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 5: MSVC C99/C11 Compatibility
# ═══════════════════════════════════════════════════════════════════════════════

check_msvc_compat() {
    section "MSVC Compatibility"
    
    # Check for __attribute__((packed)) without MSVC alternative
    local bad_packed=$(grep -rn "__attribute__\s*(\s*(\s*packed" include/ src/ --include="*.h" 2>/dev/null | grep -v "pragma pack" | head -3 || true)
    
    if [ -n "$bad_packed" ]; then
        echo "$bad_packed" | while read -r line; do
            warn "$line - Use UFT_PACKED or #pragma pack instead"
        done
    else
        ok "No problematic __attribute__((packed)) usage"
    fi
    
    # Check for C99 designated initializers in complex ways MSVC doesn't like
    # (simplified check)
    ok "MSVC compatibility check complete"
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 6: Version Consistency
# ═══════════════════════════════════════════════════════════════════════════════

check_version() {
    section "Version Consistency"
    
    local v_file=$(cat VERSION 2>/dev/null | tr -d '[:space:]')
    local v_cmake=$(grep -oP 'project\s*\([^)]*VERSION\s+\K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt 2>/dev/null | head -1)
    local v_header=$(grep -oP 'UFT_VERSION_STRING\s+"\K[0-9]+\.[0-9]+\.[0-9]+' include/uft/uft_version.h 2>/dev/null)
    
    if [ -z "$v_file" ]; then
        warn "VERSION file missing or empty"
        return
    fi
    
    local mismatch=0
    
    [ "$v_file" != "$v_cmake" ] && { fail "VERSION ($v_file) != CMakeLists.txt ($v_cmake)"; mismatch=1; }
    [ "$v_file" != "$v_header" ] && { fail "VERSION ($v_file) != uft_version.h ($v_header)"; mismatch=1; }
    
    if [ $mismatch -eq 1 ] && [ "$FIX" = "1" ]; then
        sed -i "s/VERSION [0-9]\+\.[0-9]\+\.[0-9]\+/VERSION $v_file/" CMakeLists.txt
        sed -i "s/UFT_VERSION_STRING \"[^\"]*\"/UFT_VERSION_STRING \"$v_file\"/" include/uft/uft_version.h 2>/dev/null || true
        fix "Synchronized versions to $v_file"
    elif [ $mismatch -eq 0 ]; then
        ok "Version consistent: $v_file"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 7: Build Test
# ═══════════════════════════════════════════════════════════════════════════════

check_build() {
    [ "$QUICK" = "1" ] && { info "Skipping build test (--quick)"; return; }
    
    section "Build Test"
    
    local build_dir="build_validate"
    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    
    echo -e "  ${BLUE}→${NC} Configuring..."
    if ! cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DUFT_BUILD_GUI=OFF > "$build_dir/cmake.log" 2>&1; then
        fail "CMake failed - see $build_dir/cmake.log"
        tail -10 "$build_dir/cmake.log" | sed 's/^/    /'
        return
    fi
    ok "CMake configuration"
    
    echo -e "  ${BLUE}→${NC} Building..."
    if ! cmake --build "$build_dir" -j$(nproc 2>/dev/null || echo 4) > "$build_dir/build.log" 2>&1; then
        fail "Build failed - see $build_dir/build.log"
        grep -E "error:|Error:" "$build_dir/build.log" | head -5 | sed 's/^/    /'
        return
    fi
    ok "Build successful"
    
    echo -e "  ${BLUE}→${NC} Testing..."
    if ! (cd "$build_dir" && ctest --output-on-failure > test.log 2>&1); then
        fail "Tests failed - see $build_dir/test.log"
        grep -E "FAILED|Error" "$build_dir/test.log" | head -5 | sed 's/^/    /'
        return
    fi
    local passed=$(grep -oP '\d+(?= tests passed)' "$build_dir/test.log" 2>/dev/null || echo "all")
    ok "Tests passed ($passed)"
    
    # Cleanup on success
    rm -rf "$build_dir"
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 8: Common CI Failures
# ═══════════════════════════════════════════════════════════════════════════════

check_common_issues() {
    section "Common CI Issues"
    
    # Hardcoded paths
    if grep -rn '"/home/\|"C:\\Users\|"/Users/' src/ tests/ --include="*.c" --include="*.cpp" 2>/dev/null | head -1 | grep -q .; then
        fail "Hardcoded user paths found"
    else
        ok "No hardcoded paths"
    fi
    
    # Missing stdbool.h for bool
    local missing_bool=$(grep -rln '\bbool\b' src/ --include="*.c" 2>/dev/null | while read f; do
        grep -L "stdbool.h\|uft_common.h" "$f"
    done | head -3)
    
    if [ -n "$missing_bool" ]; then
        echo "$missing_bool" | while read f; do
            warn "$f uses 'bool' without stdbool.h include"
        done
    else
        ok "All files using bool have proper includes"
    fi
    
    # Check for simulation code in release
    local sim_count=$(grep -rn "// Simulate\|// FAKE\|// TODO: real" src/*.cpp 2>/dev/null | wc -l)
    if [ "$sim_count" -gt 5 ]; then
        warn "$sim_count simulation stubs found - consider implementing"
    else
        ok "Minimal simulation code ($sim_count)"
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════════

echo ""
echo -e "${BOLD}${BLUE}╔═══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║${NC}           ${BOLD}UFT Pre-Push Validator v2.0${NC}                        ${BOLD}${BLUE}║${NC}"
echo -e "${BOLD}${BLUE}║${NC}           Bei uns geht kein Bit verloren                      ${BOLD}${BLUE}║${NC}"
echo -e "${BOLD}${BLUE}╚═══════════════════════════════════════════════════════════════╝${NC}"

# Ensure we're in project root
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: Run from project root directory${NC}"
    exit 1
fi

# Run all checks
check_windows_reserved
check_qt_modules
check_windows_libs
check_include_guards
check_msvc_compat
check_version
check_common_issues
check_build

# ═══════════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════════

section "Summary"

if [ $ERRORS -gt 0 ]; then
    echo -e "  ${RED}${BOLD}✗ $ERRORS ERRORS${NC} - Fix before pushing!"
fi
if [ $WARNINGS -gt 0 ]; then
    echo -e "  ${YELLOW}⚠ $WARNINGS warnings${NC} - consider fixing"
fi
if [ $FIXED -gt 0 ]; then
    echo -e "  ${GREEN}⚡ $FIXED issues auto-fixed${NC}"
fi
if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "  ${GREEN}${BOLD}✓ All checks passed!${NC}"
fi

echo ""

if [ $ERRORS -gt 0 ]; then
    echo -e "${RED}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  ❌ VALIDATION FAILED - DO NOT PUSH                           ║${NC}"
    echo -e "${RED}║                                                               ║${NC}"
    echo -e "${RED}║  Run with --fix to auto-fix some issues:                      ║${NC}"
    echo -e "${RED}║    ./scripts/validate.sh --fix                                ║${NC}"
    echo -e "${RED}╚═══════════════════════════════════════════════════════════════╝${NC}"
    exit 1
else
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║  ✅ READY TO PUSH                                             ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    exit 0
fi

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 9: Qt6 Deprecated Functions  
# ═══════════════════════════════════════════════════════════════════════════════

check_qt6_deprecated() {
    log_section "Check: Qt6 Deprecated Functions"
    
    # qrand() was removed in Qt6
    local qrand_usage=$(grep -rn "\bqrand\s*(" src/ --include="*.cpp" 2>/dev/null | head -3)
    if [ -n "$qrand_usage" ]; then
        echo "$qrand_usage" | while read -r line; do
            log_error "$line - qrand() removed in Qt6, use QRandomGenerator"
        done
    else
        log_ok "No deprecated qrand() usage"
    fi
    
    # qsrand() was also removed
    local qsrand_usage=$(grep -rn "\bqsrand\s*(" src/ --include="*.cpp" 2>/dev/null | head -3)
    if [ -n "$qsrand_usage" ]; then
        echo "$qsrand_usage" | while read -r line; do
            log_error "$line - qsrand() removed in Qt6, use QRandomGenerator"
        done
    fi
}

# ═══════════════════════════════════════════════════════════════════════════════
# CHECK 10: UI/Code Mismatch
# ═══════════════════════════════════════════════════════════════════════════════

check_ui_code_match() {
    log_section "Check: UI/Code Widget Consistency"
    
    # Find all ui->widget references in cpp files
    for cpp in src/*.cpp; do
        [ -f "$cpp" ] || continue
        
        # Extract base name for corresponding .ui file
        base=$(basename "$cpp" .cpp)
        ui_file="forms/tab_${base}.ui"
        [ -f "$ui_file" ] || ui_file="forms/${base}.ui"
        [ -f "$ui_file" ] || continue
        
        # Find ui-> references
        widgets=$(grep -oE "ui->[a-zA-Z_]+" "$cpp" | sed 's/ui->//' | sort | uniq)
        
        for widget in $widgets; do
            # Skip setupUi
            [ "$widget" = "setupUi" ] && continue
            
            # Check if widget exists in UI file
            if ! grep -q "name=\"$widget\"" "$ui_file" 2>/dev/null; then
                log_warn "$cpp references ui->$widget but not found in $ui_file"
            fi
        done
    done
    
    log_ok "UI/Code consistency check complete"
}
