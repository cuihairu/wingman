@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Wingman Development Environment Setup
echo ========================================
echo.

:: Check if vcpkg directory exists
if exist "vcpkg" (
    echo [1/4] vcpkg directory already exists, skipping clone
) else (
    echo [1/4] Cloning vcpkg...
    git clone https://github.com/Microsoft/vcpkg.git
    if errorlevel 1 (
        echo ERROR: Failed to clone vcpkg
        echo Please check your internet connection and try again
        pause
        exit /b 1
    )
)

:: Bootstrap vcpkg
if exist "vcpkg\vcpkg.exe" (
    echo [2/4] vcpkg already bootstrapped
) else (
    echo [2/4] Bootstrapping vcpkg...
    cd vcpkg
    call bootstrap-vcpkg.bat -disableMetrics
    cd ..
    if errorlevel 1 (
        echo ERROR: Failed to bootstrap vcpkg
        pause
        exit /b 1
    )
)

:: Create cache directory
if not exist "vcpkg\archives" mkdir vcpkg\archives

:: Install dependencies
echo [3/4] Installing vcpkg dependencies...
echo This may take 30-60 minutes on first run...
echo.

vcpkg\vcpkg install --triplet=x64-windows ^
    lua opencv4 spdlog nlohmann-json asio curl sqlite3 protobuf

if errorlevel 1 (
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)

echo.
echo [4/4] Setting up build directories...
if not exist "build" mkdir build
if not exist "vcpkg-cache" mkdir vcpkg-cache

echo.
echo ========================================
echo Setup Complete!
echo ========================================
echo.
echo Next steps:
echo 1. Open Visual Studio 2022
echo 2. Or run: configure.bat
echo.

pause
