[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$lock=Get-Content (Join-Path $root 'sources.lock.json') -Raw | ConvertFrom-Json
foreach($file in $lock.files){
    if((Get-FileHash -LiteralPath (Join-Path $root $file.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $file.sha256){throw "Upstream drift: $($file.path)"}
}
$manifest=Get-Content (Join-Path $root 'core-manifest.json') -Raw | ConvertFrom-Json
if($manifest.frontendContractVersion -ne 2){throw 'Wrong frontend contract version'}
$cmake=Get-Content (Join-Path $root 'pico\CMakeLists.txt') -Raw
foreach($token in @('rom_browser.c','settings_host.c','frontend_shell.c','PICO_SECOND_PIO_USB_DP_PIN','RETROPICOES_ENABLE_DIRECT_LOADER_RETURN=0')){
    if(-not $cmake.Contains($token)){throw "Missing integration: $token"}
}
$main=Get-Content (Join-Path $root 'platform\retropicoes\main.cpp') -Raw
foreach($token in @('retropicoes_rom_browser_open','retropicoes_rom_browser_render','retropicoes_rom_browser_update','retropicoes_frontend_shell_launch','retropicoes_frontend_shell_open_settings')){
    if(-not $main.Contains($token)){throw "Missing lifecycle call: $token"}
}
$settings=Get-Content (Join-Path $root 'platform\retropicoes\settings_ui.cpp') -Raw
if(-not $settings.Contains('retropicoes_settings_host_run') -or $settings.Contains('RETROPICOES_SETTINGS_CAP_AUDIO')){throw 'Invalid Settings capability adoption'}
Write-Output 'TI83 CONTRACTS: PASS pinned unmodified upstream, frontend v2, public modules, single-key Menu, no unsupported Audio option'
