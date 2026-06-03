#!/usr/bin/env pwsh
# Wingman 一键构建脚本

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Wingman 构建脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. 构建 Dashboard
Write-Host "[1/3] 构建 Dashboard..." -ForegroundColor Yellow
Push-Location dashboard
try {
    & pnpm install
    & pnpm run build
    if ($LASTEXITCODE -ne 0) {
        throw "Dashboard 构建失败"
    }
    Write-Host "✓ Dashboard 构建成功" -ForegroundColor Green
} finally {
    Pop-Location
}

# 2. 复制 Dashboard 到构建目录
Write-Host ""
Write-Host "[2/3] 复制静态资源..." -ForegroundColor Yellow
$distDir = "dashboard/dist"
$buildDistDir = "build/dist"

if (Test-Path $buildDistDir) {
    Remove-Item -Recurse -Force $buildDistDir
}
New-Item -ItemType Directory -Path $buildDistDir -Force | Out-Null
Copy-Item -Recurse -Path "$distDir/*" -Destination $buildDistDir

Write-Host "✓ 静态资源已复制到 build/dist" -ForegroundColor Green

# 3. 构建 C++ 项目
Write-Host ""
Write-Host "[3/3] 构建 C++ 项目..." -ForegroundColor Yellow
if (-not (Test-Path build)) {
    New-Item -ItemType Directory -Path build -Force | Out-Null
}
Push-Location build
try {
    & cmake .. -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        throw "CMake 配置失败"
    }
    & cmake --build . --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "CMake 构建失败"
    }
    Write-Host "✓ C++ 项目构建成功" -ForegroundColor Green
} finally {
    Pop-Location
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  构建完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "运行程序:" -ForegroundColor White
Write-Host "  .\build-msvc-ninja-vcpkg\apps\runtime\wingman-runtime.exe" -ForegroundColor Gray
Write-Host ""
