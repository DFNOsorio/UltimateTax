<#
ultimateTax build script (PowerShell)

This script:
  1) Configures CMake into ./build_output
  2) Builds with the selected configuration
  3) Optionally runs tests (CTest)
  4) Optionally stages a zip-ready folder into ./install (via CMake target: utax_stage)
  5) Generates compile_commands.json for clangd/Zed using a Ninja side-config
     (Visual Studio generators do NOT emit compile_commands.json)

───────────────────────────────────────────────────────────────────────────────
USAGE EXAMPLES

1) Default (Debug, tests ON, no staging):
   .\scripts\build.ps1

2) Release build (tests ON by default), no staging:
   .\scripts\build.ps1 -Config Release

3) Release build + stage zip-ready folder to ./install:
   .\scripts\build.ps1 -Config Release -Stage

4) Release build, tests disabled, stage:
   .\scripts\build.ps1 -Config Release -NoTests -Stage

5) Clean build_output and install, then build + stage:
   .\scripts\build.ps1 -Clean -Config Release -Stage

6) Use a different Visual Studio generator and/or architecture:
   .\scripts\build.ps1 -Generator "Visual Studio 17 2022" -Arch x64 -Config Release -Stage

NOTES
- Tests are ON by default unless you pass -NoTests.
- -Stage wipes ./install first to ensure deterministic zip-ready output.
- compile_commands.json is generated in a separate build folder (build_output_clangd)
  using "Ninja Multi-Config" and copied to repo root as ./compile_commands.json
───────────────────────────────────────────────────────────────────────────────
#>

param(
  [ValidateSet("Debug","Release","RelWithDebInfo","MinSizeRel")]
  [string]$Config = "Debug",

  # Tests are ON by default; use -NoTests to disable.
  [switch]$Tests,
  [switch]$NoTests,

  # Stage means: populate ./install as zip-ready folder.
  [switch]$Stage,

  # Clean removes ./build_output, ./build_output_clangd and ./install before configuring/building.
  [switch]$Clean,

  # Default Windows generator for building with MSVC.
  [string]$Generator = "Visual Studio 18 2026",

  # Only used for Visual Studio generators.
  [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"

$root        = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildDir    = Join-Path $root "build_output"
$clangdDir   = Join-Path $root "build_output_clangd"
$stageDir    = Join-Path $root "install"
$ccDst       = Join-Path $root "compile_commands.json"

function Has-Command($name) {
  $null -ne (Get-Command $name -ErrorAction SilentlyContinue)
}

if ($Clean) {
  if (Test-Path $buildDir)  { Remove-Item -Recurse -Force $buildDir }
  if (Test-Path $clangdDir) { Remove-Item -Recurse -Force $clangdDir }
  if (Test-Path $stageDir)  { Remove-Item -Recurse -Force $stageDir }
  if (Test-Path $ccDst)     { Remove-Item -Force $ccDst }
}

# Decide tests default: ON unless explicitly disabled
$buildTests = $true
if ($Tests)   { $buildTests = $true }
if ($NoTests) { $buildTests = $false }

$testsFlag = if ($buildTests) { "ON" } else { "OFF" }

Write-Host "Generator:  $Generator"
Write-Host "Arch:       $Arch"
Write-Host "Config:     $Config"
Write-Host "Tests:      $testsFlag"
Write-Host "BuildDir:   $buildDir"
Write-Host "StageDir:   $stageDir"
Write-Host "ClangdDir:  $clangdDir"

# Configure (main build)
# Note: For VS generators, -A is required. For Ninja generators, it is ignored.
$cmakeArgs = @(
  "-S", "$root",
  "-B", "$buildDir",
  "-G", "$Generator",
  "-DUTAX_BUILD_TESTS=$testsFlag",
  "-DCMAKE_INSTALL_PREFIX=$stageDir"
)

if ($Generator -like "Visual Studio*") {
  $cmakeArgs += @("-A", "$Arch")
}

cmake @cmakeArgs

# Build (parallel)
cmake --build "$buildDir" --config "$Config" --parallel

# Run tests (if enabled)
if ($buildTests) {
  ctest --test-dir "$buildDir" -C "$Config" --output-on-failure
}

# Stage (zip-ready install folder)
if ($Stage) {
  # Ensure a clean staging folder for deterministic zip output
  if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }

  cmake --build "$buildDir" --config "$Config" --target utax_stage --parallel
  Write-Host "Staged zip-ready folder at: $stageDir"
}

# --- Generate compile_commands.json for clangd/Zed ---
# Visual Studio generators do NOT emit compile_commands.json, so we create a side
# Ninja configuration purely to generate it, then copy it to repo root.
if (-not (Has-Command "ninja")) {
  Write-Warning "Ninja not found on PATH. Skipping compile_commands.json generation."
  Write-Warning "Install Ninja or ensure it's on PATH to generate compile_commands.json."
} else {
  cmake -S "$root" -B "$clangdDir" -G "Ninja Multi-Config" `
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
    -DUTAX_BUILD_TESTS=OFF `
    -DCMAKE_INSTALL_PREFIX="$stageDir" | Out-Null

  $ccSrc = Join-Path $clangdDir "compile_commands.json"
  if (Test-Path $ccSrc) {
    Copy-Item $ccSrc $ccDst -Force
    Write-Host "Generated compile_commands.json at: $ccDst"
  } else {
    Write-Warning "Expected compile_commands.json was not found at: $ccSrc"
  }
}
