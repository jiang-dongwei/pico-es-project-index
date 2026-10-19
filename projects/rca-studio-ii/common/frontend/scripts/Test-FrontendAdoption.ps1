[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))

function Read-Required([string]$relative) {
    $path = Join-Path $workspace $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing frontend contract source: $relative"
    }
    return Get-Content -LiteralPath $path -Raw
}

function Require-Token([string]$text, [string]$token, [string]$source) {
    if (-not $text.Contains($token)) {
        throw "$source must include $token"
    }
}

$cmakeContracts = @(
    @('atari2600/pico/CMakeLists.txt', 'rom_browser.c', 'frontend_shell.c'),
    @('chip8/pico/CMakeLists.txt', 'rom_browser.c', 'frontend_shell.c', 'settings_host.c'),
    @('lynx/CMakeLists.txt', 'rom_browser.c', 'frontend_shell.c', 'settings_host.c'),
    @('digger/CMakeLists.txt', 'native_game_settings.c', 'frontend_shell.c', 'settings_host.c'),
    @('xrick/CMakeLists.txt', 'native_game_settings.c', 'frontend_shell.c', 'settings_host.c'),
    @('new-core-template/template/CMakeLists.txt.tmpl',
      'rom_browser.c', 'frontend_shell.c', 'settings_host.c')
)
foreach ($contract in $cmakeContracts) {
    $body = Read-Required $contract[0]
    foreach ($token in $contract[1..($contract.Count - 1)]) {
        Require-Token $body $token $contract[0]
    }
}

# PCE/O2EM apply pinned patches to external trees. Inspect the actual patched
# build inputs, not an unpatched upstream checkout or an unused source copy.
foreach ($contract in @(
    @('_deps/pico-pcePlus-upstream/CMakeLists.txt', 'rom_browser.c', 'frontend_shell.c', 'settings_host.c'),
    @('_deps/pico-pacPlus-upstream/CMakeLists.txt', 'rom_browser.c', 'frontend_shell.c', 'settings_host.c')
)) {
    if (-not (Test-Path -LiteralPath (Join-Path $workspace $contract[0]) -PathType Leaf)) {
        Write-Output "External core not bootstrapped; adoption check skipped: $($contract[0])"
        continue
    }
    $body = Read-Required $contract[0]
    foreach ($token in $contract[1..($contract.Count - 1)]) {
        Require-Token $body $token $contract[0]
    }
}

$templateContract = Read-Required 'new-core-template/template/platform/retropicoes/contract.c.tmpl'
Require-Token $templateContract 'retropicoes_frontend_shell_init' 'new-core-template contract'
Require-Token $templateContract 'retropicoes_rom_browser_open' 'new-core-template contract'
$templateGuide = Read-Required 'new-core-template/template/PORTING.md.tmpl'
Require-Token $templateGuide 'RETROPICOES_UI_MENU' 'new-core-template guide'
$frontendContract = Read-Required 'docs/contracts/FRONTEND_CONTRACT.md'
Require-Token $frontendContract 'RETROPICOES_FRONTEND_CONTRACT_VERSION = 2' 'frontend contract'
$frontendHeader = Read-Required 'common/frontend/frontend_shell.h'
Require-Token $frontendHeader '#define RETROPICOES_FRONTEND_CONTRACT_VERSION 2u' 'frontend header'
$manifestTemplate = Read-Required 'new-core-template/template/core-manifest.json.tmpl'
Require-Token $manifestTemplate '"frontendContractVersion": 2' 'new-core-template manifest'
foreach ($relative in @('common/menu/menu_input.h', 'common/menu/menu_input.c')) {
    $body = Read-Required $relative
    if ($body.Contains('retropicoes_menu_gesture_') -or
        $body.Contains('RETROPICOES_MENU_CHORD_HOLD_MS')) {
        throw "Obsolete Select+Start/long-Start gesture API returned: $relative"
    }
}

$activeAdapters = @(
    'atari2600/platform/retropicoes', 'chip8/platform/retropicoes',
    'lynx/platform/retropicoes', 'digger/port/src', 'xrick/port/src',
    'new-core-template/template/platform/retropicoes'
)
foreach ($relative in $activeAdapters) {
    $root = Join-Path $workspace $relative
    foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse |
             Where-Object { $_.Name -match '\.(c|cpp|h|tmpl)$' }) {
        $body = Get-Content -LiteralPath $file.FullName -Raw
        if ($body -match '(?i)select\s*\+\s*start|start\s*\+\s*select|long.?press.{0,20}start|hold.{0,20}start') {
            throw "Legacy menu gesture in active adapter: $($file.FullName)"
        }
        if ($body -match '\b(draw_rom_menu|scan_rom_menu)\s*\(') {
            throw "Private ROM browser in active adapter: $($file.FullName)"
        }
    }
}

# The native-game builds use a purpose-built Settings page. Their retained
# pico_shared/menu.cpp copies are NOT linked; reject accidental re-linking.
foreach ($project in @('digger', 'xrick')) {
    $body = Read-Required "$project/CMakeLists.txt"
    if ($body.Contains('pico_shared/menu.cpp')) {
        throw "$project re-linked the unused legacy pico_shared menu"
    }
}

Write-Output 'S19 FRONTEND ADOPTION GATE: PASS'
