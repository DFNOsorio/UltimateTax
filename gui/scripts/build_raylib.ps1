# build_raylib.ps1
# Builds all C files in ..\src using raylib in ..\dependencies\raylib (MSVC).
# Outputs:
#   ..\build\<OutName>.exe
#   ..\build\raylib.dll
# Intermediates:
#   ..\build\obj\*.obj
#   ..\build\obj\<OutName>.pdb (Debug)

param(
    [ValidateSet("Debug","Release")]
    [string]$Config = "Debug",

    [string]$OutName = "app",

    [switch]$Run
)

$ErrorActionPreference = "Stop"

# Resolve project paths relative to this script (gui\scripts)
$GuiRoot   = Split-Path -Parent $PSScriptRoot
$SrcDir    = Join-Path $GuiRoot "src"
$HdrDir    = Join-Path $GuiRoot "headers"
$DepDir    = Join-Path $GuiRoot "dependencies\raylib"
$IncDir    = Join-Path $DepDir  "include"
$LibDir    = Join-Path $DepDir  "lib"
$BuildDir  = Join-Path $GuiRoot "build"
$ObjDir    = Join-Path $BuildDir "obj"

# Inputs (FORCE ARRAY even if only one .c file)
$cFiles = @(
    Get-ChildItem -Path $SrcDir -Filter *.c -File |
    ForEach-Object { $_.FullName }
)

if ($cFiles.Count -eq 0) {
    throw "No .c files found in: $SrcDir"
}

# Ensure output dirs exist
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir   | Out-Null

# Find MSVC compiler
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) {
    Write-Host "cl.exe not found in PATH." -ForegroundColor Yellow
    Write-Host "Run this from 'Developer PowerShell for VS' (or after VsDevCmd.bat)." -ForegroundColor Yellow
    exit 1
}

$ExePath = Join-Path $BuildDir ($OutName + ".exe")

# Flags
if ($Config -eq "Debug") {
    $cflags = @("/nologo","/W3","/Zi","/Od","/MDd","/D_CRT_SECURE_NO_WARNINGS")
    $lflags = @("/DEBUG")
} else {
    $cflags = @("/nologo","/W3","/O2","/MD","/D_CRT_SECURE_NO_WARNINGS")
    $lflags = @()
}

# Raylib (DLL import lib) + Win32 deps
$raylibImportLib = Join-Path $LibDir "raylibdll.lib"
if (-not (Test-Path $raylibImportLib)) {
    throw "Missing import lib: $raylibImportLib"
}

$winLibs = @(
    "opengl32.lib",
    "gdi32.lib",
    "winmm.lib",
    "user32.lib",
    "shell32.lib"
)

Write-Host "Building ($Config) -> $ExePath"
Write-Host "Sources:"
$cFiles | ForEach-Object { Write-Host "  - $_" }

# Put .obj files in build\obj\
$fo = "/Fo$ObjDir\"

# Put .pdb in build\obj\ (Debug)
$pdbPath = Join-Path $ObjDir ($OutName + ".pdb")
$fd = "/Fd:$pdbPath"

# Build args
$args = @()
$args += $cflags
$args += @("/I", $IncDir)
$args += @("/I", $HdrDir)   # <-- THIS FIXES trade_table.h not found
$args += $fo
if ($Config -eq "Debug") { $args += $fd }
$args += $cFiles
$args += "/Fe:$ExePath"
$args += @("/link", "/LIBPATH:$LibDir", $raylibImportLib)
$args += $winLibs
$args += $lflags

& $cl.Source @args
if ($LASTEXITCODE -ne 0) {
    throw "cl.exe failed with exit code $LASTEXITCODE"
}

# Copy raylib.dll next to the exe so it runs
$raylibDll = Join-Path $LibDir "raylib.dll"
if (-not (Test-Path $raylibDll)) {
    throw "Missing DLL: $raylibDll"
}
Copy-Item -Force $raylibDll $BuildDir

Write-Host "Done."
Write-Host "Output: $ExePath"
Write-Host "Intermediates: $ObjDir"
Write-Host "DLL copied: $(Join-Path $BuildDir "raylib.dll")"

if ($Run) {
    Write-Host "Running..."
    & $ExePath
}
