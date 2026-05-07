$ErrorActionPreference = "Stop"

Write-Host "=== Setting up vcpkg ===" -ForegroundColor Green

if (Test-Path "vcpkg") {
    Write-Host "vcpkg directory exists, skipping clone"
} else {
    Write-Host "Cloning vcpkg..."
    git clone https://github.com/Microsoft/vcpkg.git
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to clone vcpkg" -ForegroundColor Red
        exit 1
    }
}

if (-not (Test-Path "vcpkg\vcpkg.exe")) {
    Write-Host "Bootstrapping vcpkg..."
    cd vcpkg
    .\bootstrap-vcpkg.bat -disableMetrics
    cd ..
}

$env:VCPKG_ROOT_ABS = (Resolve-Path "vcpkg").Path
$env:VCPKG_DEFAULT_BINARY_CACHE = "$env:VCPKG_ROOT_ABS\archives"
New-Item -ItemType Directory -Force -Path "$env:VCPKG_DEFAULT_BINARY_CACHE" | Out-Null

Write-Host "VCPKG_ROOT: $env:VCPKG_ROOT_ABS"
Write-Host "VCPKG_CACHE: $env:VCPKG_DEFAULT_BINARY_CACHE"

Write-Host ""
Write-Host "=== Installing vcpkg dependencies ===" -ForegroundColor Green
.\vcpkg\vcpkg install --triplet=x64-windows lua opencv4 spdlog nlohmann-json asio curl sqlite3 protobuf

Write-Host ""
Write-Host "=== vcpkg setup complete ===" -ForegroundColor Green
