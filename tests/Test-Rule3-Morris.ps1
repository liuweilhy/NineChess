$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $scriptRoot "Invoke-RuleHarness.ps1") -RuleIndex 3 -RuleName "Rule 3 - Morris"
exit $LASTEXITCODE
