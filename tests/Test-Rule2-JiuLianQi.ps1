$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $scriptRoot "Invoke-RuleHarness.ps1") -RuleIndex 2 -RuleName "Rule 2 - JiuLianQi"
exit $LASTEXITCODE
