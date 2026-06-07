param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw "VCPKG_ROOT is not set."
}

$vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"

if ((Test-Path $VcpkgRoot) -and (Test-Path $vcpkgExe)) {
    Write-Host "vcpkg already installed at $VcpkgRoot, skipping clone/bootstrap."
    exit 0
}

if (Test-Path $VcpkgRoot) {
    Remove-Item -Recurse -Force $VcpkgRoot
}

# Pin vcpkg to a specific version for reproducibility and supply chain security
# Matches the builtin-baseline in vcpkg.json (can be overridden via VCPKG_VERSION env var)
$vcpkgVersion = if ($env:VCPKG_VERSION) { $env:VCPKG_VERSION } else { "2024.06.15" }

Write-Host "Cloning vcpkg version $vcpkgVersion"

git clone --depth 1 --branch $vcpkgVersion https://github.com/Microsoft/vcpkg.git $VcpkgRoot
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to clone vcpkg at version $vcpkgVersion, falling back to latest"
    git clone https://github.com/Microsoft/vcpkg.git $VcpkgRoot
}

& (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat")
