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

& $coverageExe `
    --quiet `
    --cover_children `
    --sources $projectRoot `
    --export_type "cobertura:$absoluteCoverageFile" `
    -- `
    ctest --test-dir $BuildDir -C $Config --output-on-failure --timeout $TimeoutSeconds

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
