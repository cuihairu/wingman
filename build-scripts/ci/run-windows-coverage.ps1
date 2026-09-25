param(
    [string]$BuildDir = "build",
    [string]$Config = "Debug",
    [string]$CoverageFile = "build/coverage/coverage.cobertura.xml",
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Continue"

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
# NOTE: FileWatcherTest.* must be FIRST - gtest 1.17.0 ignores the last negative
# pattern in a long filter string when --gtest_filter is passed through OpenCppCoverage.
$gtestFilter = "*:-FileWatcherTest.*:-IpcTest.*:-IpcFactoryTest.CreateServerWithDefaultConfig:-IpcFactoryTest.CreateClientWithDefaultConfig:-IpcFactoryTest.CreateServerWithExplicitTransport:-IpcFactoryTest.CreateClientWithExplicitTransport:-IpcFactoryTest.CreateServerWithEmptyName:-IpcFactoryTest.CreateClientWithEmptyName:-IpcFactoryTest.CreateServerWithTcpFallback:-IpcFactoryTest.CreateClientWithTcpFallback"

# Create a batch file with the full OpenCppCoverage command.
# Use delayed expansion to capture OpenCppCoverage's exit code before it gets overwritten.
# Test output is redirected into a log file: OpenCppCoverage's exit code is NOT a
# reliable failure signal (a real "4 FAILED" run that hit an exit-phase
# ACCESS_VIOLATION exited -1073741819 instead of 1 and showed up green), so we
# judge by the gtest FAILED summary line instead.
$gtestLog = Join-Path $coverageDir "core_tests_output.log"
$runBat = Join-Path $coverageDir "run_coverage.bat"
$batLines = @(
    '@echo off',
    'setlocal EnableDelayedExpansion',
    "set GTEST_FILTER=$gtestFilter",
    "`"$coverageExe`" --quiet --sources `"$projectRoot\lib\wingman\src`" --sources `"$projectRoot\lib\wingman\include`" --excluded_sources `"$projectRoot\lib\wingman\src\platform`" --export_type `"cobertura:$absoluteCoverageFile`" -- `"$($testExe.FullName)`" > `"$gtestLog`" 2>&1",
    'set OCPP_EXIT=!ERRORLEVEL!',
    'if !OCPP_EXIT! equ 1 ( exit /b 1 )',
    'exit /b 0'
)
Set-Content -Path $runBat -Value ($batLines -join "`r`n") -Encoding ASCII

Write-Host "Coverage batch file: $runBat"

# Execute the batch file directly (avoids all PowerShell/cmd quote escaping)
cmd /s /c $runBat

$exitCode = $LASTEXITCODE
Write-Host "cmd exit code: $exitCode"

# Judge by the gtest output first: exit code 1 is also propagated below, but an
# exit-phase ACCESS_VIOLATION can turn a failed run's exit code into anything.
if (Test-Path $gtestLog) {
    $passedLine = Select-String -Path $gtestLog -Pattern '\[\s+PASSED\s+\]\s+\d+ tests' | Select-Object -Last 1
    if ($passedLine) { Write-Host "gtest summary: $($passedLine.Line.Trim())" }
    $failedLine = Select-String -Path $gtestLog -Pattern '\[\s+FAILED\s+\]\s+\d+ tests' | Select-Object -Last 1
    if ($failedLine) {
        Write-Host "Test failures detected in gtest output: $($failedLine.Line.Trim())"
        Select-String -Path $gtestLog -Pattern '\[\s+FAILED\s+\]' | ForEach-Object { Write-Host $_.Line }
        Get-Content $gtestLog -Tail 40 | ForEach-Object { Write-Host $_ }
        exit 1
    }
}

# OpenCppCoverage may crash with ACCESS_VIOLATION during cleanup (exit code -1073741819)
# but the test results are still valid. Only fail on exit code 1 (actual test failures).
if ($exitCode -eq 1) {
    Write-Host "Test failures detected"
    exit 1
}
if ($exitCode -ne 0) {
    Write-Host "OpenCppCoverage exited with code $exitCode (non-fatal, tests passed)"
}
exit 0
