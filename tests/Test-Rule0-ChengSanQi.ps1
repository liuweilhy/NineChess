$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $scriptRoot "Invoke-RuleHarness.ps1") -RuleIndex 0 -RuleName "Rule 0 - ChengSanQi"
exit $LASTEXITCODE
