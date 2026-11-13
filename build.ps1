# ============================================================================
#  build.ps1 - build VolcanoX and the five tutorial stages
# ----------------------------------------------------------------------------
#  Usage (from the project folder):
#      powershell -ExecutionPolicy Bypass -File build.ps1
#      powershell -ExecutionPolicy Bypass -File build.ps1 -Target all
#      powershell -ExecutionPolicy Bypass -File build.ps1 -Target final -Run
#      powershell -ExecutionPolicy Bypass -File build.ps1 -Target stage2
#      powershell -ExecutionPolicy Bypass -File build.ps1 -Target final -Console
#
#  -Console omits -mwindows, so printf() output is visible in the terminal.
#  The script finds g++ automatically: PATH first, then the common MinGW
#  installation folders (including the WinGet WinLibs package used in SETUP).
# ============================================================================
param(
    [ValidateSet('final', 'stage1', 'stage2', 'stage3', 'stage4', 'stage5', 'all')]
    [string]$Target = 'final',
    [switch]$Console,
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

function Find-Gpp {
    $cmd = Get-Command g++ -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $patterns = @(
        "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_*\mingw64\bin\g++.exe",
        "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.MCF.UCRT_*\mingw64\bin\g++.exe",
        "C:\msys64\mingw64\bin\g++.exe",
        "C:\mingw64\bin\g++.exe",
        "C:\Program Files\mingw64\bin\g++.exe",
        "C:\tools\mingw64\bin\g++.exe"
    )
    foreach ($pattern in $patterns) {
        $file = Get-ChildItem $pattern -ErrorAction SilentlyContinue |
                Select-Object -First 1
        if ($file) { return $file.FullName }
    }
    throw "g++ was not found. Install MinGW-w64 first (see docs/SETUP.md)."
}

if (-not (Test-Path 'include\graphics.h')) {
    throw "include\graphics.h is missing. Run this script from the project folder."
}
if (-not (Test-Path 'lib\libbgi.a')) {
    throw "lib\libbgi.a is missing. Build WinBGIm first (see docs/SETUP.md)."
}

$Gpp = Find-Gpp
$env:PATH = "$(Split-Path $Gpp);$env:PATH"
Write-Host "Compiler : $Gpp" -ForegroundColor DarkGray

$Flags = @('-O2', '-std=c++17', '-Wall', '-Wextra')
if (-not $Console) { $Flags += '-mwindows' }

$Libs = @(
    '-I', 'include',
    '-L', 'lib',
    '-lbgi', '-lgdiplus', '-lcomdlg32', '-lgdi32',
    '-luuid', '-loleaut32', '-lole32', '-lwinmm',
    '-static'
)

function Build-One([string]$Source, [string]$Exe) {
    $dir = Split-Path -Parent $Exe
    if ($dir -and -not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    Write-Host "Building $Source -> $Exe" -ForegroundColor Cyan
    & $Gpp @Flags $Source -o $Exe @Libs
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $Source"
    }
}

$jobs = @()
switch ($Target) {
    'final'  { $jobs += @('src\volcano.cpp', 'volcanox.exe') }
    'stage1' { $jobs += @('stages\stage1_window.cpp',   'stages\build\stage1.exe') }
    'stage2' { $jobs += @('stages\stage2_volcano.cpp',  'stages\build\stage2.exe') }
    'stage3' { $jobs += @('stages\stage3_lava.cpp',     'stages\build\stage3.exe') }
    'stage4' { $jobs += @('stages\stage4_particles.cpp','stages\build\stage4.exe') }
    'stage5' { $jobs += @('stages\stage5_controls.cpp', 'stages\build\stage5.exe') }
    'all' {
        $jobs += @('src\volcano.cpp',               'volcanox.exe')
        $jobs += @('stages\stage1_window.cpp',      'stages\build\stage1.exe')
        $jobs += @('stages\stage2_volcano.cpp',     'stages\build\stage2.exe')
        $jobs += @('stages\stage3_lava.cpp',        'stages\build\stage3.exe')
        $jobs += @('stages\stage4_particles.cpp',   'stages\build\stage4.exe')
        $jobs += @('stages\stage5_controls.cpp',    'stages\build\stage5.exe')
    }
}

for ($i = 0; $i -lt $jobs.Count; $i += 2) {
    Build-One $jobs[$i] $jobs[$i + 1]
}

$exe = if ($Target -eq 'all') { 'volcanox.exe' }
       elseif ($Target -eq 'final') { 'volcanox.exe' }
       else { "stages\build\$Target.exe" }

Write-Host "Build OK: $exe" -ForegroundColor Green

if ($Run) {
    Write-Host "Starting $exe ..." -ForegroundColor Yellow
    Start-Process -FilePath (Join-Path $Root $exe)
}
