@echo off
REM Run Lua Tests with Busted

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set LUAROCKS_DIR=%SCRIPT_DIR%luarocks
set PROJECT_ROOT=%SCRIPT_DIR%..

echo ========================================
echo  Wingman - Lua Test Runner
echo ========================================
echo.

REM Check if LuaRocks is installed
if not exist "%LUAROCKS_DIR%\luarocks.exe" (
    echo [ERROR] LuaRocks not found!
    echo.
    echo Please install LuaRocks first:
    echo   scripts\install-luarocks.cmd
    echo.
    echo Then install Busted:
    echo   scripts\install-busted.cmd
    echo.
    pause
    exit /b 1
)

REM Check if Busted is installed
luarocks list --porcelain busted >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Busted not found!
    echo.
    echo Please install Busted first:
    echo   scripts\install-busted.cmd
    echo.
    pause
    exit /b 1
)

REM Add LuaRocks to PATH
set PATH=%LUAROCKS_DIR%;%PATH%

REM Check if tests directory exists
if not exist "%PROJECT_ROOT%\tests" (
    echo [INFO] No tests directory found, creating it...
    mkdir "%PROJECT_ROOT%\tests"
    echo Created: %PROJECT_ROOT%\tests
)

echo [INFO] Running Lua tests...
echo.

cd /d "%PROJECT_ROOT%"
busted tests -o utfTerminal

endlocal
