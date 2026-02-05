# build_and_run.ps1
$ErrorActionPreference = "Stop"

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
  Write-Host "cl.exe not found. Open 'Developer PowerShell for Visual Studio' and run again." -ForegroundColor Red
  exit 1
}

$src = "src/main.c"

$buildDir = "build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$outExe = Join-Path $buildDir "dashboard_clay_sokol.exe"
$outPdb = Join-Path $buildDir "dashboard_clay_sokol.pdb"

$inc = @(
  "/I", "dependencies\sokol",
  "/I", "dependencies\sokol\util",
  "/I", "dependencies\clay",
  "/I", "dependencies\fontstash",
  "/I", "dependencies\stb"
)

# D3D11 backend libs needed by Sokol on Windows
$libs = @(
  "user32.lib","gdi32.lib","shell32.lib","ole32.lib",
  "d3d11.lib","dxgi.lib","dxguid.lib","d3dcompiler.lib"
)

# Compile + link:
#  - /Fo: .obj goes to build\
#  - /Fd: compiler .pdb (if emitted) goes to build\
#  - /Fe: .exe goes to build\
#  - /PDB: linker .pdb goes to build\
cl.exe /nologo /W4 /Od /Zi /RTC1 /std:c11 @inc $src `
  /Fo"$buildDir\" /Fd"$buildDir\" /Fe"$outExe" `
  /link /DEBUG /PDB:"$outPdb" @libs

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Built $outExe" -ForegroundColor Green
& $outExe
