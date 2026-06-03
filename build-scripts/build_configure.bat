@echo off
echo [LEGACY] Use build-scripts\build-runtime-msvc-ninja.bat instead.
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
cmake -B build -S . -DWINGMAN_BUILD_AGENT=ON -DWINGMAN_BUILD_TESTS=OFF -G "Ninja"
