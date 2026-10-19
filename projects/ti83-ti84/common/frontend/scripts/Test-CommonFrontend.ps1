[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$frontendRoot = Split-Path -Parent $PSScriptRoot
$commonRoot = Split-Path -Parent $frontendRoot
$menuRoot = Join-Path $commonRoot 'menu'
$repoRoot = Split-Path -Parent $commonRoot
$outputDir = Join-Path $repoRoot '_build\common-frontend-host'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$warnings = @('-Wall', '-Wextra', '-Wpedantic', '-Werror')
$includes = @("-I$menuRoot", "-I$frontendRoot")
$sources = @(
    (Join-Path $menuRoot 'menu_contract.c'),
    (Join-Path $menuRoot 'menu_input.c'),
    (Join-Path $menuRoot 'menu_state.c'),
    (Join-Path $menuRoot 'menu_render.c'),
    (Join-Path $menuRoot 'menu_runtime.c'),
    (Join-Path $menuRoot 'settings_menu.c'),
    (Join-Path $frontendRoot 'rom_browser.c'),
    (Join-Path $frontendRoot 'frontend_shell.c'),
    (Join-Path $frontendRoot 'settings_host.c')
)

$browserTest = Join-Path $outputDir 'rom-browser-tests.exe'
& $gcc '-std=c11' @warnings @includes @sources `
    (Join-Path $frontendRoot 'tests\test_rom_browser.c') '-o' $browserTest
if ($LASTEXITCODE -ne 0) { throw 'Common ROM browser build failed' }

$shellTest = Join-Path $outputDir 'frontend-shell-tests.exe'
& $gcc '-std=c11' @warnings @includes @sources `
    (Join-Path $frontendRoot 'tests\test_frontend_shell.c') '-o' $shellTest
if ($LASTEXITCODE -ne 0) { throw 'Common frontend shell build failed' }

& $browserTest
if ($LASTEXITCODE -ne 0) { throw 'Common ROM browser tests failed' }
& $shellTest
if ($LASTEXITCODE -ne 0) { throw 'Common frontend shell tests failed' }

$settingsTest = Join-Path $outputDir 'settings-host-tests.exe'
& $gcc '-std=c11' @warnings @includes @sources `
    (Join-Path $frontendRoot 'tests\test_settings_host.c') '-o' $settingsTest
if ($LASTEXITCODE -ne 0) { throw 'Common Settings host build failed' }
& $settingsTest
if ($LASTEXITCODE -ne 0) { throw 'Common Settings host tests failed' }

& (Join-Path $PSScriptRoot 'Test-FrontendAdoption.ps1')

Write-Output 'S19 common frontend host tests: PASS'
