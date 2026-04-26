$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectPath = "D:\My program\QT\NineChess\NineChessConsole\NineChessConsole.vcxproj"
$exePath = "D:\My program\QT\NineChess\NineChessConsole\x64\Debug\NineChessConsole.exe"
$tempDir = Join-Path $scriptRoot "tmp_console_blackbox"

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

function U {
    param([Parameter(Mandatory = $true)][string]$Base64)

    return [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Base64))
}

function Quote-CmdPath {
    param([Parameter(Mandatory = $true)][string]$Text)

    return '"' + $Text + '"'
}

function Build-NineChessConsole {
    $msbuildPath = Get-MSBuildPath

    Write-Host "== Build NineChessConsole =="
    & $msbuildPath $projectPath /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo
    if ($LASTEXITCODE -ne 0) {
        throw ("NineChessConsole build failed with exit code {0}." -f $LASTEXITCODE)
    }

    if (-not (Test-Path $exePath)) {
        throw ("NineChessConsole executable was not found: {0}" -f $exePath)
    }
}

function Invoke-ConsoleReplay {
    param(
        [Parameter(Mandatory = $true)][string[]]$Commands,
        [string[]]$Arguments = @()
    )

    if (-not (Test-Path $tempDir)) {
        New-Item -ItemType Directory -Path $tempDir | Out-Null
    }

    $id = [Guid]::NewGuid().ToString("N")
    $inputPath = Join-Path $tempDir ($id + ".in.txt")
    $outputPath = Join-Path $tempDir ($id + ".out.txt")
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)

    try {
        $inputText = ($Commands -join "`r`n") + "`r`n"
        [System.IO.File]::WriteAllText($inputPath, $inputText, $utf8NoBom)

        $argText = ""
        if ($Arguments.Count -gt 0) {
            $argText = " " + ($Arguments -join " ")
        }

        $cmdline = (Quote-CmdPath -Text $exePath) + $argText +
            " < " + (Quote-CmdPath -Text $inputPath) +
            " > " + (Quote-CmdPath -Text $outputPath)

        cmd /c $cmdline
        $exitCode = $LASTEXITCODE
        $outputText = [System.Text.Encoding]::UTF8.GetString(
            [System.IO.File]::ReadAllBytes($outputPath))

        return @{
            ExitCode = $exitCode
            Output = $outputText
        }
    }
    finally {
        Remove-Item -LiteralPath $inputPath -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $outputPath -ErrorAction SilentlyContinue
    }
}

function New-Case {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Commands,
        [string[]]$Arguments = @(),
        [string[]]$Contains = @(),
        [string[]]$NotContains = @()
    )

    return @{
        Name = $Name
        Commands = $Commands
        Arguments = $Arguments
        Contains = $Contains
        NotContains = $NotContains
    }
}

function Test-Case {
    param([Parameter(Mandatory = $true)][hashtable]$Case)

    Write-Host ""
    Write-Host ("== {0} ==" -f $Case.Name)

    $result = Invoke-ConsoleReplay -Commands $Case.Commands -Arguments $Case.Arguments
    $checks = 0
    $failed = 0

    ++$checks
    if ($result.ExitCode -eq 0) {
        Write-Host "  [PASS] process exits with code 0"
    }
    else {
        ++$failed
        Write-Host ("  [FAIL] process exit code is {0}" -f $result.ExitCode)
    }

    foreach ($text in $Case.Contains) {
        ++$checks
        if ($result.Output.Contains($text)) {
            Write-Host ("  [PASS] contains: {0}" -f $text)
        }
        else {
            ++$failed
            Write-Host ("  [FAIL] missing: {0}" -f $text)
        }
    }

    foreach ($text in $Case.NotContains) {
        ++$checks
        if (-not $result.Output.Contains($text)) {
            Write-Host ("  [PASS] not contains: {0}" -f $text)
        }
        else {
            ++$failed
            Write-Host ("  [FAIL] unexpected text: {0}" -f $text)
        }
    }

    if ($failed -eq 0) {
        Write-Host "  CASE PASS"
    }
    else {
        Write-Host ("  CASE FAIL ({0}/{1} failed)" -f $failed, $checks)
        Write-Host "  ---- Captured Output Begin ----"
        Write-Host $result.Output
        Write-Host "  ---- Captured Output End ----"
    }

    return @{
        Checks = $checks
        Failed = $failed
    }
}

Build-NineChessConsole

$cases = @()

$cases += New-Case `
    -Name "rules_and_rule_switch" `
    -Commands @("rules", "rule 3", "quit") `
    -Contains @(
        (U "5Y+v6YCJ6KeE5YiZOg=="),
        (U "6KeE5YiZIDAgOiDmiJDkuInmo4s="),
        (U "6KeE5YiZIDMgOiDojqvph4zmlq/kuZ3lrZDmo4s="),
        (U "5bey5YiH5o2i5Yiw6KeE5YiZIDMgOiDojqvph4zmlq/kuZ3lrZDmo4s="),
        (U "6KeE5YiZOiDojqvph4zmlq/kuZ3lrZDmo4s=")
    )

$cases += New-Case `
    -Name "invalid_command_keeps_state" `
    -Commands @("(0,7)", "-(0,0)", "quit") `
    -Contains @(
        (U "5ZG95Luk5peg5pWI77yM5oiW6K+l5ZG95Luk5Zyo5b2T5YmN5bGA6Z2i5LiL5LiN5ZCI5rOV44CC"),
        (U "5pyA5ZCO5ZG95LukOiAoMCw3KQ=="),
        (U "5omL54mMOiDlhYjmiYs9OCAg5ZCO5omLPTk=")
    ) `
    -NotContains @(
        (U "5pyA5ZCO5ZG95LukOiAtKDAsMCk=")
    )

$cases += New-Case `
    -Name "history_and_undo" `
    -Commands @("(0,7)", "history", "undo", "quit") `
    -Contains @(
        (U "5ZG95Luk5Y6G5Y+yICgxKTo="),
        "  1. (0,7)",
        (U "5bey5pKk6ZSA5LiA5q2l44CC"),
        (U "5omL54mMOiDlhYjmiYs9OSAg5ZCO5omLPTk="),
        (U "5pyA5ZCO5ZG95LukOiDml6A=")
    )

$cases += New-Case `
    -Name "giveup_command" `
    -Commands @("-0", "quit") `
    -Contains @(
        (U "5b2T5YmN6IOc6ICFOiDlkI7miYs="),
        (U "5o+Q56S6OiDnjqnlrrYx6K6k6LSf77yM5oGt5Zac546p5a62MuiOt+iDnO+8gQ=="),
        (U "5pyA5ZCO5ZG95LukOiAtMA=="),
        (U "5a+55bGA5bey57uT5p2f44CC6L6T5YWlIG5ldyDph43mlrDlvIDlsYDvvIzmiJbovpPlhaUgcnVsZSBOIOWIh+aNouinhOWImeOAgg==")
    )

$cases += New-Case `
    -Name "draw_command" `
    -Commands @("==", "quit") `
    -Contains @(
        (U "5b2T5YmN6IOc6ICFOiDlubPlsYA="),
        (U "5o+Q56S6OiDlubPlsYDjgII="),
        (U "5pyA5ZCO5ZG95LukOiA9PQ=="),
        (U "5a+55bGA5bey57uT5p2f44CC6L6T5YWlIG5ldyDph43mlrDlvIDlsYDvvIzmiJbovpPlhaUgcnVsZSBOIOWIh+aNouinhOWImeOAgg==")
    )

$cases += New-Case `
    -Name "startup_rule_argument" `
    -Arguments @("--rule", "1") `
    -Commands @("quit") `
    -Contains @(
        (U "5ZCv5Yqo6KeE5YiZOiAxIDog5omT5LiJ5qOLKDEy6L+e5qOLKQ=="),
        (U "6KeE5YiZOiDmiZPkuInmo4soMTLov57mo4sp"),
        (U "6Zi25q61OiDlvIDlsYAgIOWKqOS9nDog6JC95a2QICDlvZPliY3ova7mrKE6IOWFiOaJiw==")
    )

$casePassed = 0
$caseFailed = 0
$checks = 0
$failedChecks = 0

foreach ($case in $cases) {
    $summary = Test-Case -Case $case
    $checks += $summary.Checks
    $failedChecks += $summary.Failed

    if ($summary.Failed -eq 0) {
        ++$casePassed
    }
    else {
        ++$caseFailed
    }
}

if (Test-Path $tempDir) {
    Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "==== Summary ===="
Write-Host ("Cases:  {0}/{1} passed" -f $casePassed, ($casePassed + $caseFailed))
Write-Host ("Checks: {0}/{1} passed" -f ($checks - $failedChecks), $checks)

if ($caseFailed -eq 0) {
    Write-Host "Overall result: PASS"
    exit 0
}

Write-Host "Overall result: FAIL"
exit 1
