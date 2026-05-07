@echo off
REM LuaRocks Installer for Wingman Developers
REM This is an optional developer tool for managing Lua packages

setlocal enabledelayedexpansion

set LUAROCKS_VERSION=3.11.1
set LUAROCKS_DIR=%~dp0luarocks

echo ========================================
echo  Wingman - LuaRocks Installer
echo ========================================
echo.
echo This will install LuaRocks %LUAROCKS_VERSION% to:
echo %LUAROCKS_DIR%
echo.
echo LuaRocks is a package manager for Lua modules.
echo It's only needed for development, not for running Wingman.
echo.
pause

REM Check if already installed
if exist "%LUAROCKS_DIR%\luarocks.exe" (
    echo [INFO] LuaRocks already installed at:
    echo   %LUAROCKS_DIR%
    echo.
    set /p REINSTALL="Do you want to reinstall? (y/N): "
    if /i not "!REINSTALL!"=="y" (
        echo [INFO] Skipping installation.
        goto :add_path
    )
    echo [INFO] Removing existing installation...
    rmdir /s /q "%LUAROCKS_DIR%"
)

REM Download LuaRocks
set LUAROCKS_URL=https://luarocks.github.io/luarocks/releases/luarocks-%LUAROCKS_VERSION%-windows-64.zip
set TEMP_FILE=%TEMP%\luarocks.zip

echo [1/3] Downloading LuaRocks...
echo   From: %LUAROCKS_URL%
powershell -Command "Invoke-WebRequest -Uri '%LUAROCKS_URL%' -OutFile '%TEMP_FILE%'"
if errorlevel 1 (
    echo [ERROR] Failed to download LuaRocks
    goto :cleanup
)

REM Extract
echo [2/3] Extracting...
powershell -Command "Expand-Archive -Path '%TEMP_FILE%' -DestinationPath '%TEMP%\luarocks-temp' -Force"
if errorlevel 1 (
    echo [ERROR] Failed to extract
    goto :cleanup
)

REM Move to final location
echo [3/3] Installing to: %LUAROCKS_DIR%
move "%TEMP%\luarocks-temp\luarocks-%LUAROCKS_VERSION%-windows-64" "%LUAROCKS_DIR%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to move files
    goto :cleanup
)

:add_path
echo.
echo ========================================
echo  Installation Complete!
echo ========================================
echo.
echo LuaRocks installed to: %LUAROCKS_DIR%
echo.
echo To use LuaRocks, add it to your PATH:
echo   set PATH=%LUAROCKS_DIR%;%%PATH%%
echo.
echo Or run this command to add it temporarily:
echo   set PATH=%LUAROCKS_DIR%;%%PATH%%
echo.
echo Common commands:
echo   luarocks install ^<package^>    - Install a Lua package
echo   luarocks list                   - List installed packages
echo   luarocks remove ^<package^>     - Remove a package
echo.

:cleanup
if exist "%TEMP_FILE%" del "%TEMP_FILE%"
if exist "%TEMP%\luarocks-temp" rmdir /s /q "%TEMP%\luarocks-temp"

endlocal
