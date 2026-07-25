param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [switch]$EnableTests,
    [switch]$EnablePython,
    [string]$VersionSuffix = ""
)

$vcpkgRoot = $env:VCPKG_ROOT
if ([string]::IsNullOrWhiteSpace($vcpkgRoot)) {
    throw "VCPKG_ROOT is not set."
}

$overlayPorts = Join-Path (Get-Location).Path "vcpkg-ports"

$args = @(
    "-S", ".",
    "-B", $BuildDir,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake",
    "-DVCPKG_OVERLAY_PORTS=$overlayPorts",
    "-DVCPKG_TARGET_TRIPLET=x64-windows-static"
)

if ($Config) {
    $args += "-DCMAKE_BUILD_TYPE=$Config"
}

# vcpkg manifest feature 累积到一个数组，避免多个开关各自写入
# VCPKG_MANIFEST_FEATURES 造成相互覆盖（如 -EnableTests -EnablePython 同时启用）。
$manifestFeatures = @()

if ($EnableTests) {
    $manifestFeatures += "tests"
    $args += @(
        "-DWINGMAN_BUILD_TESTS=ON",
        "-DBUILD_CORE_TESTS=ON",
        "-DBUILD_TRANSPORT_TESTS=ON",
        "-DBUILD_PROTO_TESTS=ON",
        "-DBUILD_DEBUG_TESTS=ON"
    )
}

if ($EnablePython) {
    $manifestFeatures += "python"
    $args += "-DWINGMAN_ENABLE_PYTHON=ON"
    # 当设置了 Python_ROOT_DIR 时，提示 CMake FindPython3 使用系统 Python，
    # 而非 vcpkg 安装的版本（vcpkg 的 python3 不支持 x64-windows-static）。
    if (-not [string]::IsNullOrWhiteSpace($env:Python_ROOT_DIR)) {
        $args += "-DPython_ROOT_DIR=$($env:Python_ROOT_DIR)"
    }
}

if ($manifestFeatures.Count -gt 0) {
    $args += "-DVCPKG_MANIFEST_FEATURES=$($manifestFeatures -join ';')"
}

if (-not [string]::IsNullOrWhiteSpace($VersionSuffix)) {
    $args += "-DWINGMAN_VERSION_SUFFIX=$VersionSuffix"
}

& cmake @args
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
