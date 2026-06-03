#!/usr/bin/env pwsh
# Wingman Bootstrap Script
# 自动安装 vcpkg 和项目依赖

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Wingman 依赖安装脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查是否在项目根目录
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "错误: 请在项目根目录运行此脚本" -ForegroundColor Red
    exit 1
}

# 检查 vcpkg
Write-Host "[1/4] 检查 vcpkg..." -ForegroundColor Yellow
if (-not (Test-Path "vcpkg")) {
    Write-Host "vcpkg 未安装，开始克隆..." -ForegroundColor Cyan
    git clone https://github.com/Microsoft/vcpkg.git
    if ($LASTEXITCODE -ne 0) {
        Write-Host "错误: 克隆 vcpkg 失败" -ForegroundColor Red
        exit 1
    }
    Push-Location vcpkg
    .\bootstrap-vcpkg.bat
    if ($LASTEXITCODE -ne 0) {
        Write-Host "错误: vcpkg bootstrap 失败" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    Pop-Location
    Write-Host "vcpkg 安装完成" -ForegroundColor Green
} else {
    Write-Host "vcpkg 已安装" -ForegroundColor Green
}

# 设置环境变量
$VCPKG_ROOT = Resolve-Path "vcpkg"
$env:VCPKG_ROOT = $VCPKG_ROOT
Write-Host "VCPKG_ROOT = $VCPKG_ROOT" -ForegroundColor Cyan

# 检查 CMake
Write-Host ""
Write-Host "[2/4] 检查 CMake..." -ForegroundColor Yellow
$cmakeVersion = cmake --version 2>$null
if (-not $cmakeVersion) {
    Write-Host "错误: 未找到 CMake，请先安装 CMake 3.20+" -ForegroundColor Red
    Write-Host "下载: https://cmake.org/download/" -ForegroundColor Cyan
    exit 1
}
Write-Host $cmakeVersion.Split('\n')[0] -ForegroundColor Green

# 检查 Visual Studio
Write-Host ""
Write-Host "[3/4] 检查 Visual Studio..." -ForegroundColor Yellow
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    Write-Host "错误: 未找到 Visual Studio 2022" -ForegroundColor Red
    exit 1
}
$vsPath = & $vsWhere -latest -property installationPath
if (-not $vsPath) {
    Write-Host "错误: 未找到 Visual Studio 2022" -ForegroundColor Red
    exit 1
}
Write-Host "Visual Studio 2022 已安装" -ForegroundColor Green

# 安装依赖
Write-Host ""
Write-Host "[4/4] 安装 vcpkg 依赖..." -ForegroundColor Yellow
Write-Host "这可能需要 10-30 分钟，请耐心等待..." -ForegroundColor Cyan

Push-Location vcpkg
.\vcpkg install --triplet x64-windows lua opencv4 spdlog nlohmann-json asio curl tesseract onnxruntime
if ($LASTEXITCODE -ne 0) {
    Write-Host "错误: 依赖安装失败" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  依赖安装完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "接下来运行:" -ForegroundColor Cyan
Write-Host "  build-scripts\build-runtime-msvc-ninja.bat" -ForegroundColor White
Write-Host ""
