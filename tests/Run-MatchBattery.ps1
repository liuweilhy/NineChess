# Run-MatchBattery.ps1 - Long-running match validation battery
#
# Runs multiple vs-match configurations against the Release NineChessConsole
# and prints a comparison table (opening book on/off, depth handicap, pv...).
#
# Usage: powershell -ExecutionPolicy Bypass -File ".\tests\Run-MatchBattery.ps1"
# Optional: -Games 100 -SkipBuild
#
# Note: keep this script ASCII-only. Chinese literals are embedded via Base64
# (the console writes UTF-8; Windows PowerShell 5.1 reads .ps1 as ANSI).

param(
    [int]$Games = 30,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectPath = "D:\My program\QT\NineChess\NineChessConsole\NineChessConsole.vcxproj"
$exePath = "D:\My program\QT\NineChess\NineChessConsole\x64\Release\NineChessConsole.exe"
$tempDir = Join-Path $scriptRoot "tmp_match_battery"

function U {
    param([Parameter(Mandatory = $true)][string]$Base64)
    return [System.Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Base64))
}

# Console summary fragments (UTF-8 -> Base64, see U above).
$patEngine1 = U "5byV5pOOMSjlhYjmiYsp6IOc"   # "引擎1(先手)胜"
$patEngine2 = U "5byV5pOOMijlkI7miYsp6IOc"   # "引擎2(后手)胜"
$patDraw    = U "5bmz5bGA"                    # "平局"
$patBook    = U "5Lmm5YaF6LWw5rOV"            # "书内走法"
$patReject  = U "5Lmm5YaF6KKr5ouS"            # "书内被拒"

function Get-MSBuildPath {
    $candidates = @(
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    throw "MSBuild.exe was not found."
}

if (-not $SkipBuild) {
    Write-Host "== Build NineChessConsole (Release) =="
    & (Get-MSBuildPath) $projectPath /t:Build /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "NineChessConsole build failed."
    }
}

# Each config: Name + commands (bookon/bookoff then a vs line).
$configs = @(
    @{ Name = "base 6v6 no-book";   Commands = @("bookoff", "vs $Games 6 1 16 6 1 16 7 1 40") },
    @{ Name = "6v6 with-book";      Commands = @("bookon",  "vs $Games 6 1 16 6 1 16 7 1 40") },
    @{ Name = "handicap 5v7 no-bk"; Commands = @("bookoff", "vs $Games 5 1 16 7 1 16 7 1 40") },
    @{ Name = "handicap 5v7 book";  Commands = @("bookon",  "vs $Games 5 1 16 7 1 16 7 1 40") },
    @{ Name = "6v6 book pv16v0";    Commands = @("bookon",  "vs $Games 6 1 16 6 1 0 7 1 40") }
)

if (-not (Test-Path $tempDir)) {
    New-Item -ItemType Directory -Path $tempDir | Out-Null
}

$results = @()

foreach ($config in $configs) {
    Write-Host ""
    Write-Host ("==== {0} ====" -f $config.Name)

    $commands = $config.Commands + @("quit")
    $inputPath = Join-Path $tempDir ("battery_" + [Guid]::NewGuid().ToString("N") + ".in.txt")
    $outputPath = Join-Path $tempDir ("battery_" + [Guid]::NewGuid().ToString("N") + ".out.txt")
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)

    [System.IO.File]::WriteAllText($inputPath, ($commands -join "`r`n") + "`r`n", $utf8NoBom)
    & cmd /c ('"' + $exePath + '" < "' + $inputPath + '" > "' + $outputPath + '"')
    $outputText = [System.Text.Encoding]::UTF8.GetString(
        [System.IO.File]::ReadAllBytes($outputPath))

    $wins1 = 0; $wins2 = 0; $draws = 0; $bookUses = 0; $bookRejects = 0
    $summaryMatch = [regex]::Match($outputText,
        [regex]::Escape($patEngine1) + ":\s*(\d+)\s+" +
        [regex]::Escape($patEngine2) + ":\s*(\d+)\s+" +
        [regex]::Escape($patDraw) + ":\s*(\d+)")
    if ($summaryMatch.Success) {
        $wins1 = [int]$summaryMatch.Groups[1].Value
        $wins2 = [int]$summaryMatch.Groups[2].Value
        $draws = [int]$summaryMatch.Groups[3].Value
    }
    $bookMatch = [regex]::Match($outputText,
        [regex]::Escape($patBook) + ":\s*(\d+)\s+" +
        [regex]::Escape($patReject) + ":\s*(\d+)")
    if ($bookMatch.Success) {
        $bookUses = [int]$bookMatch.Groups[1].Value
        $bookRejects = [int]$bookMatch.Groups[2].Value
    }

    $results += [pscustomobject]@{
        Name = $config.Name
        Wins1 = $wins1
        Wins2 = $wins2
        Draws = $draws
        BookUses = $bookUses
        BookRejects = $bookRejects
    }

    Write-Host ("  engine1 wins: {0}  engine2 wins: {1}  draws: {2}  book uses: {3}  rejects: {4}" -f `
        $wins1, $wins2, $draws, $bookUses, $bookRejects)

    Remove-Item -LiteralPath $inputPath -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $outputPath -ErrorAction SilentlyContinue
}

if (Test-Path $tempDir) {
    Remove-Item -LiteralPath $tempDir -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "==== Battery Summary ===="
Write-Host ("{0,-22} {1,8} {2,8} {3,8} {4,8} {5,8}" -f "config", "wins1", "wins2", "draws", "book", "rej")
foreach ($result in $results) {
    Write-Host ("{0,-22} {1,8} {2,8} {3,8} {4,8} {5,8}" -f `
        $result.Name, $result.Wins1, $result.Wins2, $result.Draws, $result.BookUses, $result.BookRejects)
}
