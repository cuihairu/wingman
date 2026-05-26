param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$CoverageFile = "build/coverage/coverage.cobertura.xml",
    [int]$TimeoutSeconds = 120
)

$coverageDir = Split-Path -Parent $CoverageFile
New-Item -ItemType Directory -Force -Path $coverageDir | Out-Null

$coverageExe = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
if (-not (Test-Path $coverageExe)) {
    throw "OpenCppCoverage is not installed at $coverageExe"
}

$projectRoot = (Get-Item ".").FullName
$absoluteCoverageFile = Join-Path $projectRoot $CoverageFile

# Find test executable
$testExe = Get-ChildItem -Path "$BuildDir\$Config\core_tests.exe" -ErrorAction SilentlyContinue
if (-not $testExe) {
    $testExe = Get-ChildItem -Path "$BuildDir\lib\wingman\tests\$Config\core_tests.exe" -ErrorAction SilentlyContinue
}
if (-not $testExe) {
    $testExe = Get-ChildItem -Path "$BuildDir" -Recurse -Filter "core_tests.exe" | Select-Object -First 1
}
if (-not $testExe) {
    throw "Could not find core_tests.exe"
}

Write-Host "Found test executable: $($testExe.FullName)"
Write-Host "Running with OpenCppCoverage..."

# Run coverage directly on test executable (much faster than --cover_children with ctest)
& $coverageExe `
    --quiet `
    --sources $projectRoot `
    --export_type "cobertura:$absoluteCoverageFile" `
    -- `
    $testExe.FullName "--gtest_filter=*"

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
