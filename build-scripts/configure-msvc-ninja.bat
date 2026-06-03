@echo off
setlocal

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
set "VCPKG_TOOLCHAIN=C:\Users\admin\vcpkg\scripts\buildsystems\vcpkg.cmake"
set "BUILD_DIR=build-msvc-ninja-vcpkg"

if not exist "%VCVARS%" (
    echo [ERROR] vcvars64.bat not found: %VCVARS%
    exit /b 1
)

if not exist "%VCPKG_TOOLCHAIN%" (
    echo [ERROR] vcpkg toolchain not found: %VCPKG_TOOLCHAIN%
    exit /b 1
)

call "%VCVARS%"
if errorlevel 1 exit /b %errorlevel%

cmake -S . -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

exit /b %errorlevel%
