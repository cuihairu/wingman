@echo off
setlocal enabledelayedexpansion

echo [LEGACY] This script is kept for compatibility. Use build-scripts\build-runtime-msvc-ninja.bat for the primary Windows build path.
echo.

echo ========================================
echo Wingman CMake Configuration
echo ========================================
echo.

:: Check if vcpkg exists
if not exist "vcpkg\vcpkg.exe" (
    echo ERROR: vcpkg not found!
    echo Please run setup.bat first to install dependencies.
    pause
        exit /b 1
)

:: Set vcpkg environment
set VCPKG_ROOT=%CD%\vcpkg
set VCPKG_INSTALLED_DIR=%VCPKG_ROOT%\installed

echo VCPKG_ROOT: %VCPKG_ROOT%
echo VCPKG_INSTALLED_DIR: %VCPKG_INSTALLED_DIR%
echo.

:: Configure CMake
echo Configuring CMake (Debug)...
cmake -B build -S . ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DWINGMAN_BUILD_AGENT=ON ^
    -DWINGMAN_BUILD_TESTS=OFF

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    echo Possible solutions:
    echo 1. Make sure Visual Studio 2022 is installed
    echo 2. Run setup.bat to install dependencies
    echo 3. Check that vcpkg dependencies are installed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Configuration Complete!
echo ========================================
echo.
echo To build the project:
echo   build-scripts\build-runtime-msvc-ninja.bat
echo.

pause
