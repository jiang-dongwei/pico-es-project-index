[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build-host'
$env:Path="$(Split-Path (Get-Command mingw32-make.exe).Source);$env:Path"
& cmake -S $root -B $build -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Debug
if($LASTEXITCODE){throw 'Host configure failed'}
& cmake --build $build --parallel 8
if($LASTEXITCODE){throw 'Host build failed'}
& ctest --test-dir $build --output-on-failure
if($LASTEXITCODE){throw 'Host tests failed'}
