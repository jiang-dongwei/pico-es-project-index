param([string]$Cc = "gcc")

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outDir = Join-Path $root "_deps\test-refkeen-game-installation"
$upstream = Join-Path $root "_deps\reflectionhle-upstream\src"
$testExe = Join-Path $outDir "test_game_installation_path.exe"

New-Item -ItemType Directory -Force $outDir | Out-Null
$includes = @(
    "-I$(Join-Path $root 'refkeen\host\include')",
    "-I$(Join-Path $root 'refkeen\platform\retropicoes')",
    "-I$(Join-Path $root 'compat')",
    "-I$upstream"
)
& $Cc '-std=c11' '-Wall' '-Wextra' '-Werror' '-Wno-unused-function' `
    '-DREFKEEN_HAS_VER_KDREAMS=1' @includes `
    (Join-Path $root 'refkeen\platform\retropicoes\be_app_paths.c') `
    (Join-Path $root 'refkeen\tests\test_game_installation_path.c') `
    -o $testExe
if ($LASTEXITCODE -ne 0) { throw "installation path build failed ($LASTEXITCODE)" }
& $testExe
if ($LASTEXITCODE -ne 0) { throw "installation path test failed ($LASTEXITCODE)" }
