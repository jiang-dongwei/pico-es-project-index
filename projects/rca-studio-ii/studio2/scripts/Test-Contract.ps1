$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content (Join-Path $root 'core-manifest.json') -Raw | ConvertFrom-Json
$header = Get-Content (Join-Path $root '../common/frontend/frontend_shell.h') -Raw
if ($header -notmatch 'RETROPICOES_FRONTEND_CONTRACT_VERSION\s+2u') { throw 'Unsupported public frontend contract' }
if ($manifest.frontendContractVersion -ne 2) { throw 'Frontend contract mismatch' }
$cmake = Get-Content (Join-Path $root 'pico/CMakeLists.txt') -Raw
foreach ($token in 'rom_browser.c','settings_host.c','frontend_shell.c') {
    if (-not $cmake.Contains($token)) { throw "Missing public link: $token" }
}
$main = Get-Content (Join-Path $root 'platform/retropicoes/main.cpp') -Raw
foreach ($token in 'retropicoes_rom_browser_open','retropicoes_rom_browser_update','retropicoes_rom_browser_render','retropicoes_frontend_shell_init','retropicoes_frontend_shell_launch') {
    if (-not $main.Contains($token)) { throw "Missing public call: $token" }
}
$ui = Get-Content (Join-Path $root 'platform/retropicoes/settings_ui.cpp') -Raw
if (-not $ui.Contains('retropicoes_settings_host_run')) { throw 'Missing public Settings host' }
Write-Output 'STUDIO2 FRONTEND CONTRACT: PASS'
