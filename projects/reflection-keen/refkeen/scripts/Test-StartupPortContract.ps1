param(
    [string]$Cc = "gcc"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$upstream = Join-Path $root "_deps\reflectionhle-upstream\src"
$outDir = Join-Path $root "_deps\test-startup-port"
$exe = Join-Path $outDir "test_startup_port.exe"
$source = Join-Path $root "refkeen\platform\retropicoes\be_startup_port.c"
$header = Join-Path $root "refkeen\platform\retropicoes\startup_port.h"
$test = Join-Path $root "refkeen\tests\test_startup_port.c"

New-Item -ItemType Directory -Force $outDir | Out-Null

$sourceText = Get-Content -Raw $source
$headerText = Get-Content -Raw $header

if ($sourceText -match '\bexit\s*\(') {
    throw "startup port must not terminate the bare-metal process with exit()"
}
if ($sourceText -notmatch 'longjmp\s*\(') {
    throw "startup port does not escape the upstream-owned game loop"
}
if ($sourceText -notmatch 'BEL_ST_SaveConfigFiles\s*\(') {
    throw "QuickExit no longer saves the upstream configuration"
}
if ($headerText -notmatch 'set_exit_target\s*\(jmp_buf \*target\)') {
    throw "outer runtime exit-target contract is missing"
}

& $Cc -std=c11 -Wall -Wextra -Werror -Wno-unused-function `
    "-I$upstream" `
    "-I$(Join-Path $root 'refkeen\platform\retropicoes')" `
    "-I$(Join-Path $root 'refkeen\host\include')" `
    $source $test -o $exe
if ($LASTEXITCODE -ne 0) {
    throw "startup port host build failed ($LASTEXITCODE)"
}

& $exe
if ($LASTEXITCODE -ne 0) {
    throw "startup port host test failed ($LASTEXITCODE)"
}

Write-Host "REFKEEN STARTUP CONTRACT: PASS (4 source checks + 8 host assertions)"
