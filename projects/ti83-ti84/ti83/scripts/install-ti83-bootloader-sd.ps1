[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SdRoot,
    [string]$ApplicationUf2 = "",
    [string]$ArtworkPng = "",
    [string]$ReplaceProgramName = "",
    [switch]$IncludeSampleGames,
    [switch]$AllowDirectoryRootForTest
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ApplicationUf2)) {
    $ApplicationUf2 = Join-Path $projectRoot `
        'build-pico-retropicoes-v21-bootloader-make\ti83.uf2'
}
if ([string]::IsNullOrWhiteSpace($ArtworkPng)) {
    $ArtworkPng = Join-Path $projectRoot 'assets\loader\ti83.png'
}
$ApplicationUf2 = [IO.Path]::GetFullPath($ApplicationUf2)
$ArtworkPng = [IO.Path]::GetFullPath($ArtworkPng)
if (-not (Test-Path -LiteralPath $ApplicationUf2 -PathType Leaf)) {
    throw "Loader application UF2 is missing: $ApplicationUf2"
}
if (-not (Test-Path -LiteralPath $ArtworkPng -PathType Leaf)) {
    throw "Loader artwork is missing: $ArtworkPng"
}

& (Join-Path $PSScriptRoot 'Test-Uf2.ps1') -Path $ApplicationUf2 -Bootloader

$sd = [IO.Path]::GetFullPath($SdRoot)
if (-not (Test-Path -LiteralPath $sd -PathType Container)) {
    throw "SD root does not exist: $sd"
}
$driveRoot = [IO.Path]::GetPathRoot($sd)
if (-not $AllowDirectoryRootForTest -and
    $sd.TrimEnd('\') -ne $driveRoot.TrimEnd('\')) {
    throw "SdRoot must be a mounted drive root such as H:\, got: $sd"
}

$emuRoot = Join-Path $sd 'emu'
$configRoot = Join-Path $emuRoot '16'
$indexPath = Join-Path $emuRoot 'emulators.txt'
$targetUf2 = Join-Path $configRoot 'ti83.uf2'
$artworkRoot = Join-Path $emuRoot 'assets\themes\0'
$targetArtwork = Join-Path $artworkRoot 'ti83.png'
$romRoot = Join-Path $sd 'roms\TI83'
$saveRoot = Join-Path $sd 'SAVES\TI83'


$lines = [Collections.Generic.List[string]]::new()
if (Test-Path -LiteralPath $indexPath) {
    foreach ($line in Get-Content -LiteralPath $indexPath) { $lines.Add($line) }
} else {
    $lines.Add('# pico-bootLoader application index')
    $lines.Add('# program_name;image_key;display_name[;aux_uf2]')
}
$row = 'ti83;ti83;TI-83/84'
$found = $false
$active = 0
$duplicates = [Collections.Generic.List[int]]::new()
for ($i = 0; $i -lt $lines.Count; ++$i) {
    $trimmed = $lines[$i].Trim()
    if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
    ++$active
    $name = ($trimmed -split ';', 2)[0].Trim()
    if ($name -ieq 'ti83') {
        if (-not $found) { $lines[$i] = $row; $found = $true }
        else { $duplicates.Add($i) }
    }
}
for ($i = $duplicates.Count - 1; $i -ge 0; --$i) {
    $lines.RemoveAt($duplicates[$i]); --$active
}
if (-not $found) {
    if ($active -ge 32) {
        if ([string]::IsNullOrWhiteSpace($ReplaceProgramName)) {
            throw 'emulators.txt already has 32 entries; specify -ReplaceProgramName explicitly'
        }
        $replace = -1
        for ($i = 0; $i -lt $lines.Count; ++$i) {
            $trimmed = $lines[$i].Trim()
            if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
            if ((($trimmed -split ';', 2)[0].Trim()) -ieq $ReplaceProgramName) {
                $replace = $i; break
            }
        }
        if ($replace -lt 0) { throw "Replacement entry not found: $ReplaceProgramName" }
        $lines[$replace] = $row
    } else { $lines.Add($row) }
}

New-Item -ItemType Directory -Force `
    -Path $configRoot,$artworkRoot,$romRoot,$saveRoot | Out-Null
$indexTemp = "$indexPath.ti83.tmp"
$uf2Temp = "$targetUf2.ti83.tmp"
$artTemp = "$targetArtwork.ti83.tmp"
[IO.File]::WriteAllLines($indexTemp, $lines)
Copy-Item -LiteralPath $ApplicationUf2 -Destination $uf2Temp -Force
Copy-Item -LiteralPath $ArtworkPng -Destination $artTemp -Force
$sourceUf2Hash = (Get-FileHash -LiteralPath $ApplicationUf2 -Algorithm SHA256).Hash
$sourceArtHash = (Get-FileHash -LiteralPath $ArtworkPng -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath $uf2Temp -Algorithm SHA256).Hash -ne $sourceUf2Hash -or
    (Get-FileHash -LiteralPath $artTemp -Algorithm SHA256).Hash -ne $sourceArtHash) {
    throw 'Staged TI83 file checksum mismatch'
}
Move-Item -LiteralPath $indexTemp -Destination $indexPath -Force
Move-Item -LiteralPath $uf2Temp -Destination $targetUf2 -Force
Move-Item -LiteralPath $artTemp -Destination $targetArtwork -Force
foreach ($suffix in '.444','.555') {
    $cache = Join-Path $artworkRoot "ti83$suffix"
    if (Test-Path -LiteralPath $cache) { Remove-Item -LiteralPath $cache -Force }
}
if ((Get-FileHash -LiteralPath $targetUf2 -Algorithm SHA256).Hash -ne $sourceUf2Hash -or
    (Get-FileHash -LiteralPath $targetArtwork -Algorithm SHA256).Hash -ne $sourceArtHash) {
    throw 'Installed TI83 file checksum mismatch'
}
if ($IncludeSampleGames) {
    $bundle = Join-Path $projectRoot 'assets\games\scott-ti-games'
    foreach ($relative in @('snake\snake.8xp','2048\2048.8xp')) {
        $source = Join-Path $bundle $relative
        $target = Join-Path $romRoot (Split-Path -Leaf $relative)
        if (Test-Path -LiteralPath $target) {
            if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $source).Hash) {
                Write-Output "Preserved existing game: $target"
                continue
            }
        } else {
            $stage = "$target.ti83.tmp"
            Copy-Item -LiteralPath $source -Destination $stage
            if ((Get-FileHash -LiteralPath $stage).Hash -ne (Get-FileHash -LiteralPath $source).Hash) {
                throw "Game staging failed: $target"
            }
            Move-Item -LiteralPath $stage -Destination $target
        }
        if ((Get-FileHash -LiteralPath $target).Hash -ne (Get-FileHash -LiteralPath $source).Hash) {
            throw "Game readback failed: $target"
        }
    }
    $licenseRoot = Join-Path $sd 'licenses\TI83\scott-ti-games'
    New-Item -ItemType Directory -Path $licenseRoot -Force | Out-Null
    Copy-Item -Path (Join-Path $bundle '*') -Destination $licenseRoot -Recurse -Force
}

Write-Output 'CONTENT INSTALL: PASS'
Write-Output "Application: $targetUf2"
Write-Output "Artwork: $targetArtwork"
Write-Output "App SHA-256: $sourceUf2Hash"
Write-Output "Art SHA-256: $sourceArtHash"
& (Join-Path $PSScriptRoot 'Test-RuntimeData.ps1') -DataRoot $sd
