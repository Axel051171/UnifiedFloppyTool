@echo off
REM ═══════════════════════════════════════════════════════════════════════════════
REM UFT End-to-End Build & Test Script for Windows
REM Version: 4.1.0
REM ═══════════════════════════════════════════════════════════════════════════════

setlocal enabledelayedexpansion

echo.
echo ╔═══════════════════════════════════════════════════════════════╗
echo ║     UFT End-to-End Test Suite v4.1.0 (Windows)               ║
echo ╚═══════════════════════════════════════════════════════════════╝
echo.

REM Configuration
set VERSION=4.1.0
set PROJECT_DIR=%~dp0..
set BUILD_DIR=%PROJECT_DIR%\build_e2e
set RESULTS_DIR=%PROJECT_DIR%\test_results
set LOG_FILE=%RESULTS_DIR%\e2e_test_%date:~-4%%date:~3,2%%date:~0,2%_%time:~0,2%%time:~3,2%.log

REM Counters
set TESTS_RUN=0
set TESTS_PASSED=0
set TESTS_FAILED=0
set TESTS_SKIPPED=0

REM Create directories
if not exist "%RESULTS_DIR%" mkdir "%RESULTS_DIR%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo UFT E2E Test Results - %date% %time% > "%LOG_FILE%"
echo. >> "%LOG_FILE%"

REM ═══════════════════════════════════════════════════════════════════════════════
REM Phase 1: Environment Checks
REM ═══════════════════════════════════════════════════════════════════════════════

echo.
echo ═══════════════════════════════════════════════════════════════
echo  Phase 1: Environment Validation
echo ═══════════════════════════════════════════════════════════════
echo.

REM Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo   [PASS] CMake installed
    set /a TESTS_PASSED+=1
) else (
    echo   [FAIL] CMake not found
    set /a TESTS_FAILED+=1
)
set /a TESTS_RUN+=1

REM Check for Visual Studio
where cl >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo   [PASS] MSVC compiler found
    set /a TESTS_PASSED+=1
) else (
    echo   [WARN] MSVC not found - run from Developer Command Prompt
    set /a TESTS_SKIPPED+=1
)
set /a TESTS_RUN+=1

REM Check for Ninja
where ninja >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo   [PASS] Ninja installed
    set CMAKE_GEN=-G Ninja
    set /a TESTS_PASSED+=1
) else (
    echo   [INFO] Ninja not found - using Visual Studio generator
    set CMAKE_GEN=-G "Visual Studio 17 2022" -A x64
    set /a TESTS_SKIPPED+=1
)
set /a TESTS_RUN+=1

REM ═══════════════════════════════════════════════════════════════════════════════
REM Phase 2: Build Tests
REM ═══════════════════════════════════════════════════════════════════════════════

echo.
echo ═══════════════════════════════════════════════════════════════
echo  Phase 2: Build System Tests
echo ═══════════════════════════════════════════════════════════════
echo.

cd /d "%PROJECT_DIR%"

REM CMake configure
echo   Testing: CMake configure...
cmake -B "%BUILD_DIR%" %CMAKE_GEN% -DCMAKE_BUILD_TYPE=Debug -DUFT_BUILD_TESTS=ON >> "%LOG_FILE%" 2>&1
if %ERRORLEVEL% equ 0 (
    echo   [PASS] CMake configure
    set /a TESTS_PASSED+=1
) else (
    echo   [FAIL] CMake configure
    set /a TESTS_FAILED+=1
)
set /a TESTS_RUN+=1

REM Build
echo   Testing: Build project...
cmake --build "%BUILD_DIR%" --config Debug >> "%LOG_FILE%" 2>&1
if %ERRORLEVEL% equ 0 (
    echo   [PASS] Build project
    set /a TESTS_PASSED+=1
) else (
    echo   [FAIL] Build project
    set /a TESTS_FAILED+=1
)
set /a TESTS_RUN+=1

REM Check library exists
if exist "%BUILD_DIR%\Debug\*.lib" (
    echo   [PASS] Library built
    set /a TESTS_PASSED+=1
) else if exist "%BUILD_DIR%\*.lib" (
    echo   [PASS] Library built
    set /a TESTS_PASSED+=1
) else (
    echo   [WARN] Library not found
    set /a TESTS_SKIPPED+=1
)
set /a TESTS_RUN+=1

REM ═══════════════════════════════════════════════════════════════════════════════
REM Phase 3: Unit Tests
REM ═══════════════════════════════════════════════════════════════════════════════

echo.
echo ═══════════════════════════════════════════════════════════════
echo  Phase 3: Unit Tests
echo ═══════════════════════════════════════════════════════════════
echo.

cd /d "%BUILD_DIR%"

REM Run CTest
if exist "CTestTestfile.cmake" (
    echo   Testing: CTest...
    ctest --output-on-failure -C Debug >> "%LOG_FILE%" 2>&1
    if %ERRORLEVEL% equ 0 (
        echo   [PASS] CTest
        set /a TESTS_PASSED+=1
    ) else (
        echo   [FAIL] CTest
        set /a TESTS_FAILED+=1
    )
) else (
    echo   [SKIP] CTest not configured
    set /a TESTS_SKIPPED+=1
)
set /a TESTS_RUN+=1

REM ═══════════════════════════════════════════════════════════════════════════════
REM Phase 4: Documentation Check
REM ═══════════════════════════════════════════════════════════════════════════════

echo.
echo ═══════════════════════════════════════════════════════════════
echo  Phase 4: Documentation Validation
echo ═══════════════════════════════════════════════════════════════
echo.

cd /d "%PROJECT_DIR%"

if exist "README.md" (
    echo   [PASS] README.md exists
    set /a TESTS_PASSED+=1
) else (
    echo   [FAIL] README.md missing
    set /a TESTS_FAILED+=1
)
set /a TESTS_RUN+=1

if exist "docs\FORMAT_SPECIFICATIONS.md" (
    echo   [PASS] Format specs exist
    set /a TESTS_PASSED+=1
) else (
    echo   [FAIL] Format specs missing
    set /a TESTS_FAILED+=1
)
set /a TESTS_RUN+=1

if exist "docs\PARSER_WRITING_GUIDE.md" (
    echo   [PASS] Parser guide exists
    set /a TESTS_PASSED+=1
) else (
    echo   [FAIL] Parser guide missing
    set /a TESTS_FAILED+=1
)
set /a TESTS_RUN+=1

REM ═══════════════════════════════════════════════════════════════════════════════
REM Summary
REM ═══════════════════════════════════════════════════════════════════════════════

echo.
echo ═══════════════════════════════════════════════════════════════
echo  Test Summary
echo ═══════════════════════════════════════════════════════════════
echo.
echo   Platform:  Windows x64
echo   Date:      %date% %time%
echo.
echo   Passed:    %TESTS_PASSED%
echo   Failed:    %TESTS_FAILED%
echo   Skipped:   %TESTS_SKIPPED%
echo   Total:     %TESTS_RUN%
echo.

if %TESTS_FAILED% gtr 0 (
    echo   Result:    FAILED
    echo.
    exit /b 1
) else (
    echo   Result:    PASSED
    echo.
    exit /b 0
)

endlocal
