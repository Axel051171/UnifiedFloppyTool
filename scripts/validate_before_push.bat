@echo off
REM ═══════════════════════════════════════════════════════════════════════════
REM UFT Pre-Push Validator - Run BEFORE every git push
REM ═══════════════════════════════════════════════════════════════════════════

setlocal enabledelayedexpansion
set ERRORS=0

echo.
echo ══════════════════════════════════════════════════════════════
echo   UFT Pre-Push Validation
echo ══════════════════════════════════════════════════════════════
echo.

REM ─────────────────────────────────────────────────────────────────
REM 1. Check critical files exist
REM ─────────────────────────────────────────────────────────────────
echo [1/6] Checking critical files...

if not exist "UnifiedFloppyTool.pro" (
    echo   X MISSING: UnifiedFloppyTool.pro
    set /a ERRORS+=1
) else (
    echo   OK: UnifiedFloppyTool.pro
)

if not exist "CMakeLists.txt" (
    echo   X MISSING: CMakeLists.txt
    set /a ERRORS+=1
) else (
    echo   OK: CMakeLists.txt
)

if not exist ".github\workflows\ci.yml" (
    echo   X MISSING: .github\workflows\ci.yml
    set /a ERRORS+=1
) else (
    echo   OK: .github\workflows\ci.yml
)

if not exist "src\main.cpp" (
    echo   X MISSING: src\main.cpp
    set /a ERRORS+=1
) else (
    echo   OK: src\main.cpp
)

REM ─────────────────────────────────────────────────────────────────
REM 2. Check for problematic Qt modules
REM ─────────────────────────────────────────────────────────────────
echo.
echo [2/6] Checking Qt module dependencies...

findstr /C:"core5compat" UnifiedFloppyTool.pro >nul 2>&1
if %errorlevel%==0 (
    echo   WARNING: core5compat in .pro - verify it's actually needed
    findstr /C:"QTextCodec\|QRegExp" src\*.cpp src\*.h >nul 2>&1
    if %errorlevel%==1 (
        echo   X core5compat NOT needed - remove it!
        set /a ERRORS+=1
    )
) else (
    echo   OK: No unnecessary Qt5 compat modules
)

REM ─────────────────────────────────────────────────────────────────
REM 3. Check GitHub URLs
REM ─────────────────────────────────────────────────────────────────
echo.
echo [3/6] Checking GitHub URLs...

findstr /S /C:"axelmuhr" src\*.cpp src\*.h >nul 2>&1
if %errorlevel%==0 (
    echo   X WRONG GitHub username found (axelmuhr)
    set /a ERRORS+=1
) else (
    echo   OK: GitHub URLs correct
)

REM ─────────────────────────────────────────────────────────────────
REM 4. Check for common C/C++ issues (basic)
REM ─────────────────────────────────────────────────────────────────
echo.
echo [4/6] Checking for common code issues...

REM Check for potential integer overflow patterns
findstr /S /R "<<24" src\*.c src\*.h >nul 2>&1
if %errorlevel%==0 (
    findstr /S /C:"(uint32_t)" src\*.c | findstr /C:"<<24" >nul 2>&1
    if %errorlevel%==1 (
        echo   WARNING: Potential integer overflow (<<24 without uint32_t cast)
    ) else (
        echo   OK: Bit shifts appear safe
    )
) else (
    echo   OK: No risky bit shifts found
)

REM ─────────────────────────────────────────────────────────────────
REM 5. Syntax check .pro file
REM ─────────────────────────────────────────────────────────────────
echo.
echo [5/6] Validating .pro file syntax...

findstr /C:"QT +=" UnifiedFloppyTool.pro >nul 2>&1
if %errorlevel%==0 (
    echo   OK: QT modules defined
) else (
    echo   X No QT modules in .pro file
    set /a ERRORS+=1
)

findstr /C:"SOURCES +=" UnifiedFloppyTool.pro >nul 2>&1
if %errorlevel%==0 (
    echo   OK: SOURCES defined
) else (
    echo   X No SOURCES in .pro file
    set /a ERRORS+=1
)

REM ─────────────────────────────────────────────────────────────────
REM 6. Check CI workflow syntax
REM ─────────────────────────────────────────────────────────────────
echo.
echo [6/6] Validating CI workflow...

findstr /C:"runs-on:" .github\workflows\ci.yml >nul 2>&1
if %errorlevel%==0 (
    echo   OK: CI jobs defined
) else (
    echo   X CI workflow invalid
    set /a ERRORS+=1
)

REM ─────────────────────────────────────────────────────────────────
REM Summary
REM ─────────────────────────────────────────────────────────────────
echo.
echo ══════════════════════════════════════════════════════════════

if %ERRORS%==0 (
    echo   PASSED - Safe to push!
    echo ══════════════════════════════════════════════════════════════
    echo.
    echo Run: git add -A ^&^& git commit -m "message" ^&^& git push
    exit /b 0
) else (
    echo   FAILED - %ERRORS% error(s) found. Fix before pushing!
    echo ══════════════════════════════════════════════════════════════
    exit /b 1
)
