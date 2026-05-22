param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT
)

if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    throw "VCPKG_ROOT is not set."
}

if (Test-Path $VcpkgRoot) {
    Remove-Item -Recurse -Force $VcpkgRoot
}

git clone --depth 1 https://github.com/Microsoft/vcpkg.git $VcpkgRoot
& (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat")
