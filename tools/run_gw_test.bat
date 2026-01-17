@echo off
REM Compile and run Greaseweazle connection test
REM This uses the MinGW compiler that comes with Qt

echo === Greaseweazle Connection Test ===
echo.

REM Try to find gcc
where gcc >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: gcc not found in PATH
    echo Please run this from Qt MinGW command prompt or add MinGW to PATH
    pause
    exit /b 1
)

echo Compiling gw_test.c...
gcc -o gw_test.exe gw_test.c -Wall
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo.
echo Compilation successful!
echo.

if "%1"=="" (
    echo Usage: run_gw_test.bat COMx
    echo Example: run_gw_test.bat COM4
    echo.
    set /p PORT="Enter COM port (e.g., COM4): "
) else (
    set PORT=%1
)

echo.
echo Testing connection to %PORT%...
echo.
gw_test.exe %PORT%

echo.
pause
