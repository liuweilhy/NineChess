$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$scripts = @(
    "Run-All-RuleTests.ps1",
    "Run-ConsoleBlackBoxTests.ps1"
)

$passed = 0
$failed = 0

foreach ($scriptName in $scripts) {
    Write-Host ""
    Write-Host ("==== {0} ====" -f $scriptName)

    & powershell -ExecutionPolicy Bypass -File (Join-Path $scriptRoot $scriptName)
    if ($LASTEXITCODE -eq 0) {
        ++$passed
    }
    else {
        ++$failed
    }
}

Write-Host ""
Write-Host "==== Overall Summary ===="
Write-Host ("Scripts passed: {0}" -f $passed)
Write-Host ("Scripts failed: {0}" -f $failed)

if ($failed -eq 0) {
    Write-Host "Overall result: PASS"
    exit 0
}

Write-Host "Overall result: FAIL"
exit 1
