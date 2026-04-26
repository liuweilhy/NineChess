param(
    [Parameter(Mandatory = $true)]
    [ValidateRange(0, 3)]
    [int]$RuleIndex,

    [Parameter(Mandatory = $true)]
    [string]$RuleName
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-MSBuildPath {
    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "MSBuild.exe was not found."
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectPath = Join-Path $scriptRoot "RuleHarness.vcxproj"
$exePath = Join-Path $scriptRoot "bin\x64\Debug\RuleHarness.exe"
$msbuildPath = Get-MSBuildPath

Write-Host ("== Build RuleHarness ({0}) ==" -f $RuleName)
& $msbuildPath $projectPath /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path $exePath)) {
    throw ("RuleHarness executable was not found: {0}" -f $exePath)
}

Write-Host ("== Run rule test: {0} ==" -f $RuleName)
& $exePath $RuleIndex
exit $LASTEXITCODE
