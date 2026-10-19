[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$menuRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent (Split-Path -Parent $menuRoot)
$outputDir = Join-Path $repoRoot '_build\common-menu-host'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$gcc = (Get-Command gcc -ErrorAction Stop).Source
$gxx = (Get-Command g++ -ErrorAction Stop).Source
$warnings = @('-Wall', '-Wextra', '-Wpedantic', '-Werror')
$include = "-I$menuRoot"
$contract = Join-Path $menuRoot 'menu_contract.c'
$inputSource = Join-Path $menuRoot 'menu_input.c'
$stateSource = Join-Path $menuRoot 'menu_state.c'
$renderSource = Join-Path $menuRoot 'menu_render.c'
$runtimeSource = Join-Path $menuRoot 'menu_runtime.c'
$settingsSource = Join-Path $menuRoot 'settings_menu.c'
$commonSources = @(
    $contract,
    $inputSource,
    $stateSource,
    $renderSource,
    $runtimeSource,
    $settingsSource
)

$contractTest = Join-Path $outputDir 'menu-contract-tests.exe'
& $gcc '-std=c11' @warnings $include @commonSources `
    (Join-Path $menuRoot 'tests\test_menu_contract.c') '-o' $contractTest
if ($LASTEXITCODE -ne 0) { throw 'C menu contract build failed' }

$inputTest = Join-Path $outputDir 'menu-input-tests.exe'
& $gcc '-std=c11' @warnings $include @commonSources `
    (Join-Path $menuRoot 'tests\test_menu_input.c') '-o' $inputTest
if ($LASTEXITCODE -ne 0) { throw 'C menu input build failed' }

$stateTest = Join-Path $outputDir 'menu-state-tests.exe'
& $gcc '-std=c11' @warnings $include @commonSources `
    (Join-Path $menuRoot 'tests\test_menu_state.c') '-o' $stateTest
if ($LASTEXITCODE -ne 0) { throw 'C menu state build failed' }

$renderTest = Join-Path $outputDir 'menu-render-tests.exe'
& $gcc '-std=c11' @warnings $include @commonSources `
    (Join-Path $menuRoot 'tests\test_menu_render.c') '-o' $renderTest
if ($LASTEXITCODE -ne 0) { throw 'C menu render build failed' }

$runtimeTest = Join-Path $outputDir 'menu-runtime-tests.exe'
& $gcc '-std=c11' @warnings $include @commonSources `
    (Join-Path $menuRoot 'tests\test_menu_runtime.c') '-o' $runtimeTest
if ($LASTEXITCODE -ne 0) { throw 'C menu runtime build failed' }

$settingsTest = Join-Path $outputDir 'settings-menu-tests.exe'
& $gcc '-std=c11' @warnings $include @commonSources `
    (Join-Path $menuRoot 'tests\test_settings_menu.c') '-o' $settingsTest
if ($LASTEXITCODE -ne 0) { throw 'C Settings menu build failed' }

$contractObject = Join-Path $outputDir 'menu_contract.o'
$inputObject = Join-Path $outputDir 'menu_input.o'
& $gcc '-std=c11' @warnings $include '-c' $contract '-o' $contractObject
if ($LASTEXITCODE -ne 0) { throw 'C contract object build failed' }
& $gcc '-std=c11' @warnings $include '-c' $inputSource '-o' $inputObject
if ($LASTEXITCODE -ne 0) { throw 'C input object build failed' }

$cppTest = Join-Path $outputDir 'menu-cpp-contract-tests.exe'
& $gxx '-std=c++17' @warnings $include `
    (Join-Path $menuRoot 'tests\test_cpp_contract.cpp') `
    $contractObject $inputObject '-o' $cppTest
if ($LASTEXITCODE -ne 0) { throw 'C++ contract build failed' }

& $contractTest
if ($LASTEXITCODE -ne 0) { throw 'C menu contract tests failed' }
& $inputTest
if ($LASTEXITCODE -ne 0) { throw 'C menu input tests failed' }
& $stateTest
if ($LASTEXITCODE -ne 0) { throw 'C menu state tests failed' }
& $renderTest
if ($LASTEXITCODE -ne 0) { throw 'C menu render tests failed' }
& $runtimeTest
if ($LASTEXITCODE -ne 0) { throw 'C menu runtime tests failed' }
& $settingsTest
if ($LASTEXITCODE -ne 0) { throw 'C Settings menu tests failed' }
& $cppTest
if ($LASTEXITCODE -ne 0) { throw 'C++ menu contract tests failed' }

Write-Output 'S14-A/B/C/D common menu host tests: PASS'
