# script/build_portfolio.ps1
# Usage:
#   .\script\build_portfolio.ps1
#   .\script\build_portfolio.ps1 -Tests
#   .\script\build_portfolio.ps1 -VerboseTests
#
# Also accepts bash-style:
#   .\script\build_portfolio.ps1 --tests
#   .\script\build_portfolio.ps1 --verbose-tests

[CmdletBinding()]
param(
    [switch]$Tests,
    [switch]$VerboseTests
)

$ErrorActionPreference = "Stop"

# --- Accept bash-style flags too (optional convenience) ---
if ($args -contains "--tests") { $Tests = $true }
if ($args -contains "--verbose-tests") { $Tests = $true; $VerboseTests = $true }

# --- Relaunch under PowerShell 7 (pwsh) if we're in Windows PowerShell 5.1 ---
# NOTE: This must come AFTER param() to keep PS 5.1 happy.
if ($PSVersionTable.PSEdition -ne "Core") {
    $pwshCmd = Get-Command pwsh -ErrorAction SilentlyContinue
    if ($pwshCmd) {
        & $pwshCmd.Source -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath @args
        exit $LASTEXITCODE
    }
}

# If -VerboseTests is set, ensure tests run.
if ($VerboseTests) { $Tests = $true }

# Resolve repo root as: script_dir\..
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path (Join-Path $ScriptDir "..")).Path

$ZigPortfolioDir = Join-Path $RootDir "zigPortfolio"
$OutputDir = Join-Path $RootDir "build_output"

Write-Host "Building zigPortfolio..."
Write-Host ("Root directory:       {0}" -f $RootDir)
Write-Host ("zigPortfolio folder:  {0}" -f $ZigPortfolioDir)
Write-Host ("Output directory:     {0}" -f $OutputDir)
Write-Host ""

if (-not (Test-Path $ZigPortfolioDir)) {
    throw "zigPortfolio folder not found: $ZigPortfolioDir"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Push-Location $ZigPortfolioDir
try {
    zig build

    if ($Tests) {
        Write-Host ""
        Write-Host "Running unit tests..."
        if ($VerboseTests) {
            zig build test-verbose
        } else {
            zig build test
        }
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Copying library and header to output folder..."

$ZigOutDir = Join-Path $ZigPortfolioDir "zig-out"
$BinDir    = Join-Path $ZigOutDir "bin"
$LibDir    = Join-Path $ZigOutDir "lib"

if (-not (Test-Path $ZigOutDir)) {
    throw "Could not find zig-out folder: $ZigOutDir (did zig build succeed?)"
}

$Artifacts = @()

if (Test-Path $BinDir) {
    $Artifacts += Get-ChildItem -Path $BinDir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in ".dll", ".pdb" }
}

if (Test-Path $LibDir) {
    $Artifacts += Get-ChildItem -Path $LibDir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in ".lib", ".a" }
}

$Artifacts = $Artifacts | Sort-Object LastWriteTime -Descending

if (-not $Artifacts -or $Artifacts.Count -eq 0) {
    throw "No artifacts found under: $ZigOutDir"
}

Write-Host "Found artifacts (newest first):"
$Artifacts | Select-Object -First 15 | ForEach-Object {
    Write-Host (" - {0}" -f $_.FullName)
}

# Prefer DLL if present (shared lib), otherwise fall back to static/import lib.
$Dll = $Artifacts | Where-Object { $_.Extension -eq ".dll" } | Select-Object -First 1
$Lib = $Artifacts | Where-Object { $_.Extension -in ".lib", ".a" } | Select-Object -First 1
$Pdb = $Artifacts | Where-Object { $_.Extension -eq ".pdb" } | Select-Object -First 1

# Header
$HdrSrc = Join-Path $ZigPortfolioDir "zig-out\zigPortfolio.h"
if (-not (Test-Path $HdrSrc)) {
    throw "Could not find header at: $HdrSrc"
}

$HdrDst = Join-Path $OutputDir "zigPortfolio.h"
Copy-Item -Force $HdrSrc $HdrDst

# Copy DLL if present
if ($Dll) {
    $DllDst = Join-Path $OutputDir "zigPortfolio.dll"
    Copy-Item -Force $Dll.FullName $DllDst
    Write-Host ("DLL copied to:        {0}" -f $DllDst)
}

# Copy .lib/.a if present
if ($Lib) {
    $LibDst = Join-Path $OutputDir $Lib.Name
    Copy-Item -Force $Lib.FullName $LibDst
    Write-Host ("Library copied to:    {0}" -f $LibDst)
} else {
    throw "No .lib/.a found to link against."
}

# Copy PDB if present
if ($Pdb) {
    $PdbDst = Join-Path $OutputDir $Pdb.Name
    Copy-Item -Force $Pdb.FullName $PdbDst
    Write-Host ("PDB copied to:        {0}" -f $PdbDst)
}

Write-Host "Header copied to:     $HdrDst"
Write-Host ""
Write-Host "Done."
