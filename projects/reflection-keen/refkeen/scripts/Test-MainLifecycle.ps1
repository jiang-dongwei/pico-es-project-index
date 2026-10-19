param([string]$Cc = "gcc")

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outDir = Join-Path $root "_deps\test-refkeen-main"
$upstream = Join-Path $root "_deps\reflectionhle-upstream\src"
$mainObject = Join-Path $outDir "main.o"
$testExe = Join-Path $outDir "test_main_lifecycle.exe"

New-Item -ItemType Directory -Force $outDir | Out-Null
$includes = @(
    "-I$(Join-Path $root 'refkeen\host\include')",
    "-I$(Join-Path $root 'refkeen\platform\retropicoes')",
    "-I$(Join-Path $root 'compat')",
    "-I$(Join-Path $root 'common\frontend')",
    "-I$(Join-Path $root 'common\menu')",
    "-I$upstream",
    "-I$(Join-Path $upstream 'kdreams')",
    "-I$(Join-Path $upstream 'kdreams\lscr')"
)
$flags = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function')

& $Cc @flags @includes '-Dmain=refkeen_port_main' -c `
    (Join-Path $root 'refkeen\platform\retropicoes\main.c') -o $mainObject
if ($LASTEXITCODE -ne 0) { throw "port main build failed ($LASTEXITCODE)" }

& $Cc @flags @includes `
    (Join-Path $root 'refkeen\tests\test_main_lifecycle.c') `
    $mainObject -o $testExe
if ($LASTEXITCODE -ne 0) { throw "main lifecycle test build failed ($LASTEXITCODE)" }

& $testExe
if ($LASTEXITCODE -ne 0) { throw "main lifecycle test failed ($LASTEXITCODE)" }
