$ErrorActionPreference = "Stop"

Write-Host "[LEGACY] Use build-scripts\build-runtime-msvc-ninja.bat instead." -ForegroundColor Yellow

# 设置 Visual Studio 环境
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

# 配置 CMake
cmake -B build -S . -DWINGMAN_BUILD_AGENT=ON -DWINGMAN_BUILD_TESTS=OFF -G "Ninja"
