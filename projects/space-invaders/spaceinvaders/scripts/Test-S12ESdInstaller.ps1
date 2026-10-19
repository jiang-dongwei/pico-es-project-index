[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$installer = Join-Path $PSScriptRoot 'install-spaceinvaders-bootloader-sd.ps1'
$uf2 = Join-Path $projectRoot `
    'build-pico-retropicoes-v21-bootloader-make\spaceinvaders.uf2'
$art = Join-Path $projectRoot 'assets\loader\spaceinvaders.png'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ("retropicoes-s12e-" + [Guid]::NewGuid().ToString('N'))

function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "S12-E test failed: $Message" }
}

try {
    New-Item -ItemType Directory -Force -Path $testRoot | Out-Null
    $emu = Join-Path $testRoot 'emu'
    $rom = Join-Path $testRoot 'roms\SPACEINVADERS'
    New-Item -ItemType Directory -Force -Path $emu,$rom | Out-Null
    $index = Join-Path $emu 'emulators.txt'
    [IO.File]::WriteAllLines($index, @(
        '# preserved comment',
        'existing;existing;Existing App'
    ))
    $marker = Join-Path $testRoot 'do-not-touch.dat'
    [IO.File]::WriteAllText($marker, 'preserve me')
    foreach ($name in @('invaders.h','invaders.g','invaders.f','invaders.e')) {
        [IO.File]::WriteAllBytes((Join-Path $rom $name), [byte[]]::new(2048))
    }
    $beforeMarker = (Get-FileHash -LiteralPath $marker -Algorithm SHA256).Hash

    $first = & $installer -SdRoot $testRoot -ApplicationUf2 $uf2 `
        -ArtworkPng $art -AllowDirectoryRootForTest
    $second = & $installer -SdRoot $testRoot -ApplicationUf2 $uf2 `
        -ArtworkPng $art -AllowDirectoryRootForTest
    Assert (($first -join "`n") -match 'HARDWARE READINESS: READY') `
        'complete four-ROM set was not READY'
    Assert (($second -join "`n") -match 'HARDWARE READINESS: READY') `
        'second install was not READY'
    $rows = @(Get-Content -LiteralPath $index | Where-Object {
        $_.Trim() -match '^spaceinvaders;'
    })
    Assert ($rows.Count -eq 1) 'menu entry is not idempotently unique'
    Assert ((Get-FileHash -LiteralPath (Join-Path $testRoot `
        'emu\16\spaceinvaders.uf2') -Algorithm SHA256).Hash -eq
        (Get-FileHash -LiteralPath $uf2 -Algorithm SHA256).Hash) `
        'installed UF2 hash mismatch'
    Assert ((Get-FileHash -LiteralPath (Join-Path $testRoot `
        'emu\assets\themes\0\spaceinvaders.png') -Algorithm SHA256).Hash -eq
        (Get-FileHash -LiteralPath $art -Algorithm SHA256).Hash) `
        'installed artwork hash mismatch'
    Assert ((Get-FileHash -LiteralPath $marker -Algorithm SHA256).Hash -eq
        $beforeMarker) 'unrelated file was modified'
    Assert (@(Get-ChildItem -LiteralPath $testRoot -Recurse -File |
        Where-Object { $_.Name -like '*.s12.tmp' }).Count -eq 0) `
        'temporary files remain after install'

    Remove-Item -LiteralPath (Join-Path $rom 'invaders.e') -Force
    $blocked = & $installer -SdRoot $testRoot -ApplicationUf2 $uf2 `
        -ArtworkPng $art -AllowDirectoryRootForTest
    Assert (($blocked -join "`n") -match 'HARDWARE READINESS: BLOCKED') `
        'incomplete ROM set was not BLOCKED'

    $fullRoot = Join-Path $testRoot 'full-menu'
    $fullEmu = Join-Path $fullRoot 'emu'
    New-Item -ItemType Directory -Force -Path $fullEmu | Out-Null
    $fullIndex = Join-Path $fullEmu 'emulators.txt'
    $fullRows = 1..16 | ForEach-Object { "app$_;art$_;Application $_" }
    [IO.File]::WriteAllLines($fullIndex, $fullRows)
    $capacityRejected = $false
    try {
        & $installer -SdRoot $fullRoot -ApplicationUf2 $uf2 `
            -ArtworkPng $art -AllowDirectoryRootForTest | Out-Null
    } catch {
        $capacityRejected = $_.Exception.Message -match '16 entries'
    }
    Assert $capacityRejected 'full menu did not require explicit replacement'
    & $installer -SdRoot $fullRoot -ApplicationUf2 $uf2 `
        -ArtworkPng $art -ReplaceProgramName 'app16' `
        -AllowDirectoryRootForTest | Out-Null
    $installedFullRows = @(Get-Content -LiteralPath $fullIndex)
    Assert ($installedFullRows.Count -eq 16) 'full menu entry count changed'
    Assert (@($installedFullRows | Where-Object {
        $_ -match '^spaceinvaders;'
    }).Count -eq 1) 'explicit full-menu replacement failed'
    Assert (@($installedFullRows | Where-Object {
        $_ -match '^app16;'
    }).Count -eq 0) 'replacement target remains in full menu'
    Write-Output 'S12-E SD INSTALLER TEST: PASS'
} finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
