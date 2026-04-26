$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $scriptRoot "Invoke-RuleHarness.ps1") -RuleIndex 1 -RuleName "Rule 1 - DaSanQi"
exit $LASTEXITCODE
