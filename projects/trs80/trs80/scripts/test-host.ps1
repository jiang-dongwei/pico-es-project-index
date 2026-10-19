[CmdletBinding()]
param([int]$Parallel = 8)
$ErrorActionPreference = 'Stop'
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = Join-Path $source 'build-host'
$make = (Get-Command mingw32-make.exe -ErrorAction Stop).Source
$previousPath = $env:Path
try {
    $env:Path = (Split-Path -Parent $make) + ';' + $previousPath
    & cmake -S $source -B $build -G 'MinGW Makefiles'
    if ($LASTEXITCODE -ne 0) { throw 'TRS80 host configure failed' }
    & cmake --build $build --parallel $Parallel
    if ($LASTEXITCODE -ne 0) { throw 'TRS80 host build failed' }
    & ctest --test-dir $build --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'TRS80 host tests failed' }
} finally {
    $env:Path = $previousPath
}
