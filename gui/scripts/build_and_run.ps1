# scripts/build_and_run.ps1
[CmdletBinding()]
param(
  [ValidateSet("Debug","Release")]
  [string]$Config = "Debug",
  [switch]$Run = $true
)

$ErrorActionPreference = "Stop"

function Require-Command([string]$name, [string]$hint) {
  if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
    Write-Host "$name not found. $hint" -ForegroundColor Red
    exit 1
  }
}

Require-Command "cl.exe"   "Open 'Developer PowerShell for Visual Studio' and run again."
Require-Command "link.exe" "Open 'Developer PowerShell for Visual Studio' and run again."

$root = (Resolve-Path ".").Path

$srcDir     = Join-Path $root "src"
$headersDir = Join-Path $root "headers"
$depsDir    = Join-Path $root "dependencies"

$buildRoot = Join-Path $root "build"
$buildDir  = Join-Path $buildRoot $Config
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$outExe = Join-Path $buildDir "dashboard_clay_sokol.exe"
$outPdb = Join-Path $buildDir "dashboard_clay_sokol.pdb"

# Gather sources (all .c)
$srcFiles = @(Get-ChildItem -Path $srcDir -Recurse -Filter "*.c" | Sort-Object FullName | ForEach-Object { $_.FullName })
if ($srcFiles.Count -eq 0) { throw "No .c files found under $srcDir" }

# Includes
$includeDirs = @(
  (Join-Path $depsDir "sokol"),
  (Join-Path $depsDir "sokol\util"),
  (Join-Path $depsDir "clay"),
  (Join-Path $depsDir "fontstash"),
  (Join-Path $depsDir "stb"),
  $headersDir
)
$incArgs = $includeDirs | ForEach-Object { '/I"' + $_ + '"' }

# C flags
$commonCFlags = @("/nologo","/W4","/std:c11","/MP")

if ($Config -eq "Debug") {
  $cfgFlags = @("/Od","/Zi","/RTC1","/DDEBUG","/D_DEBUG")
} else {
  $cfgFlags = @("/O2","/Zi","/DNDEBUG")
}

# Output .obj into build dir
$compileOutFlags = @("/c", "/Fo""$buildDir\\""", "/Fd""$buildDir\cl.pdb""")

# Link libs
$libs = @(
  "user32.lib","gdi32.lib","shell32.lib","ole32.lib",
  "d3d11.lib","dxgi.lib","dxguid.lib","d3dcompiler.lib"
)

Write-Host "== Building ($Config) ==" -ForegroundColor Cyan
Write-Host "Sources: $($srcFiles.Count)" -ForegroundColor DarkCyan
Write-Host "OutDir : $buildDir" -ForegroundColor DarkCyan

# 1) Compile (parallel)
& cl.exe @commonCFlags @cfgFlags @incArgs @compileOutFlags @srcFiles
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Collect objs from build dir (only those produced from src)
$objs = @(Get-ChildItem -Path $buildDir -Filter "*.obj" | ForEach-Object { $_.FullName })
if ($objs.Count -eq 0) { throw "No .obj files produced in $buildDir" }

# 2) Link
$linkArgs = @(
  "/nologo",
  "/DEBUG",
  "/OUT:""$outExe""",
  "/PDB:""$outPdb"""
) + $objs + $libs

& link.exe @linkArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "`nBuilt: $outExe" -ForegroundColor Green

if ($Run) {
  Write-Host "Running..." -ForegroundColor Yellow
  & $outExe
}
