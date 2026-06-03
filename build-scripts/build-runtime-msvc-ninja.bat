@echo off
setlocal

call "%~dp0configure-msvc-ninja.bat"
if errorlevel 1 exit /b %errorlevel%

call "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%

cmake --build build-msvc-ninja-vcpkg --target wingman-runtime --config Debug

exit /b %errorlevel%
