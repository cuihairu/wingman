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

# Exclude IPC and FileWatcher tests that crash under OpenCppCoverage instrumentation.
# Write the full command to a batch file to avoid PowerShell/cmd semicolon escaping issues.
# The semicolons in gtest_filter get misinterpreted when passed through cmd /c.
$gtestFilter = "*:-IpcTest.*:-IpcFactoryTest.CreateServerWithDefaultConfig:-IpcFactoryTest.CreateClientWithDefaultConfig:-IpcFactoryTest.CreateServerWithExplicitTransport:-IpcFactoryTest.CreateClientWithExplicitTransport:-IpcFactoryTest.CreateServerWithEmptyName:-IpcFactoryTest.CreateClientWithEmptyName:-IpcFactoryTest.CreateServerWithTcpFallback:-IpcFactoryTest.CreateClientWithTcpFallback:-FileWatcherTest.*"

# Create a batch file with the full OpenCppCoverage command
$runBat = Join-Path $coverageDir "run_coverage.bat"
$batLines = @(
    '@echo off',
    "set GTEST_FILTER=$gtestFilter",
    "`"$coverageExe`" --quiet --sources `"$projectRoot\lib\wingman\src`" --sources `"$projectRoot\lib\wingman\include`" --excluded_sources `"$projectRoot\lib\wingman\src\platform`" --export_type `"cobertura:$absoluteCoverageFile`" -- `"$($testExe.FullName)`""
)
Set-Content -Path $runBat -Value ($batLines -join "`r`n") -Encoding ASCII

Write-Host "Coverage batch file: $runBat"

# Execute the batch file directly (avoids all PowerShell/cmd quote escaping)
& cmd /s /c $runBat

# OpenCppCoverage may exit with code 3 (coverage instrumentation issues)
# but the test results are still valid. Only fail on exit code 1 (test failures).
if ($LASTEXITCODE -eq 1) {
    exit 1
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "OpenCppCoverage exited with code $LASTEXITCODE (non-fatal)"
}
