@echo off
echo ============================================
echo Greaseweazle Minimal Test Compiler
echo ============================================
echo.

REM Check for MinGW gcc
where gcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: gcc not found!
    echo Please install MinGW or add it to PATH
    pause
    exit /b 1
)

echo Compiling gw_minimal_test.c...
gcc -o gw_minimal_test.exe gw_minimal_test.c -Wall

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Compilation FAILED!
    pause
    exit /b 1
)

echo Compilation OK!
echo.
echo ============================================
echo Running test on COM4...
echo ============================================
echo.

gw_minimal_test.exe COM4

echo.
pause
