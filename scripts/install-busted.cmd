@echo off
REM Busted (Lua Testing Framework) Installer for Wingman Developers

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set LUAROCKS_DIR=%SCRIPT_DIR%luarocks

echo ========================================
echo  Wingman - Busted Installer
echo ========================================
echo.
echo Busted is a unit testing framework for Lua.
echo It requires LuaRocks to be installed first.
echo.

REM Check if LuaRocks is installed
if not exist "%LUAROCKS_DIR%\luarocks.exe" (
    echo [ERROR] LuaRocks not found!
    echo.
    echo Please install LuaRocks first:
    echo   scripts\install-luarocks.cmd
    echo.
    pause
    exit /b 1
)

echo [INFO] LuaRocks found at: %LUAROCKS_DIR%
echo.

REM Add LuaRocks to PATH for this session
set PATH=%LUAROCKS_DIR%;%PATH%

REM Check if Busted is already installed
luarocks list --porcelain busted >nul 2>&1
if errorlevel 0 (
    echo [INFO] Busted is already installed.
    set /p REINSTALL="Do you want to reinstall? (y/N): "
    if /i not "!REINSTALL!"=="y" (
        echo [INFO] Skipping installation.
        goto :done
    )
    echo [INFO] Removing existing Busted installation...
    luarocks remove busted --force
)

echo [1/1] Installing Busted via LuaRocks...
luarocks install busted
if errorlevel 1 (
    echo [ERROR] Failed to install Busted
    pause
    exit /b 1
)

:done
echo.
echo ========================================
echo  Installation Complete!
echo ========================================
echo.
echo To run tests:
echo   busted tests -o utfTerminal
echo.
echo For more options:
echo   busted --help
echo.

endlocal
