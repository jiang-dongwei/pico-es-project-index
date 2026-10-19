param([string]$Cc = "gcc")

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outDir = Join-Path $root "_deps\test-refkeen-stdio-bridge"
$testExe = Join-Path $outDir "test_stdio_compat.exe"
New-Item -ItemType Directory -Force $outDir | Out-Null
& $Cc '-std=c11' '-Wall' '-Wextra' '-Werror' `
    "-I$(Join-Path $root 'compat')" `
    "-I$(Join-Path $root 'refkeen\platform\retropicoes')" `
    (Join-Path $root 'refkeen\platform\retropicoes\stdio_compat.c') `
    (Join-Path $root 'refkeen\tests\test_stdio_compat.c') `
    -o $testExe
if ($LASTEXITCODE -ne 0) { throw "stdio bridge build failed ($LASTEXITCODE)" }
& $testExe
if ($LASTEXITCODE -ne 0) { throw "stdio bridge test failed ($LASTEXITCODE)" }
