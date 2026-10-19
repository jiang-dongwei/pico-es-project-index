[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SystemRom)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$rom=[IO.Path]::GetFullPath($SystemRom)
if(-not (Test-Path -LiteralPath $rom) -or (Get-Item -LiteralPath $rom).Length -ne 1048576){throw 'Need full 1 MiB TI84 Plus system'}
$build=Join-Path $root 'build-workload'
$env:Path="$(Split-Path (Get-Command mingw32-make.exe).Source);$env:Path"
& cmake -S $root -B $build -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DTI83_CPU_PROFILE=ON -DTI83_TIMING_PROFILE=OFF
if($LASTEXITCODE){throw 'Workload profile configure failed'}
& cmake --build $build --target game_smoke --parallel 8
if($LASTEXITCODE){throw 'Workload profile build failed'}
& (Join-Path $build 'game_smoke.exe') $rom (Join-Path $root 'assets/games/scott-ti-games/2048/2048.8xp') (Join-Path $build 'unused.ppm') --workload
if($LASTEXITCODE){throw 'Workload profile failed'}
