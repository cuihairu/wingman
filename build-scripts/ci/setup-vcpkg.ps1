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

# Pin vcpkg to a specific commit for reproducibility and supply chain security
# Use the builtin-baseline from vcpkg.json (can be overridden via VCPKG_COMMIT env var)
$vcpkgCommit = if ($env:VCPKG_COMMIT) { $env:VCPKG_COMMIT } else { "00d899c410b31467733472fc3a83a25729046b13" }

Write-Host "Cloning vcpkg and checking out commit $vcpkgCommit"

git clone https://github.com/Microsoft/vcpkg.git $VcpkgRoot
if ($LASTEXITCODE -ne 0) {
    throw "Failed to clone vcpkg"
}

git -C $VcpkgRoot checkout $vcpkgCommit
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to checkout vcpkg at commit $vcpkgCommit, using current HEAD"
}

& (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat")
