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

git clone https://github.com/Microsoft/vcpkg.git $VcpkgRoot
& (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat")
