@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Wingman Minimal Build Configuration
echo ========================================
echo.

:: Set Scoop paths
set USER_SCOOP=%USERPROFILE%\scoop
set LUA_DIR=%USER_SCOOP%\apps\lua\5.4.8-1

echo Looking for dependencies in Scoop...
echo LUA_DIR: %LUA_DIR%
echo.

:: Configure CMake with minimal dependencies
echo Configuring CMake (Minimal)...
cmake -B build-minimal -S . ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DLUA_INCLUDE_DIR=%LUA_DIR%\include ^
    -DLUA_LIBRARY=%LUA_DIR%\lib\liblua.dll.a ^
    -DWINGMAN_BUILD_AGENT=ON ^
    -DWINGMAN_BUILD_TESTS=OFF ^
    -DOPENCV_FOUND=OFF ^
    -DSPDLOG_FOUND=OFF ^
    -DPROTOBUF_FOUND=OFF

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    echo For full dependency support, run setup.bat
    pause
    exit /b 1
)

echo.
echo ========================================
echo Minimal Configuration Complete!
echo ========================================
echo.
echo Note: This is a minimal build with limited features.
echo For full functionality, install vcpkg dependencies.
echo.
echo To build:
echo   cmake --build build-minimal --config Debug
echo.

pause
