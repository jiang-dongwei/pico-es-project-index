[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SdRoot,
    [string]$ApplicationUf2 = "",
    [string]$ArtworkPng = "",
    [string]$ReplaceProgramName = "",
    [switch]$AllowDirectoryRootForTest
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ApplicationUf2)) {
    $ApplicationUf2 = Join-Path $projectRoot `
        'build-pico-hw16-loader\trs80.uf2'
}
if ([string]::IsNullOrWhiteSpace($ArtworkPng)) {
    $ArtworkPng = Join-Path $projectRoot 'assets\loader\trs80.png'
}
$ApplicationUf2 = [IO.Path]::GetFullPath($ApplicationUf2)
$ArtworkPng = [IO.Path]::GetFullPath($ArtworkPng)
if (-not (Test-Path -LiteralPath $ApplicationUf2 -PathType Leaf)) {
    throw "Loader application UF2 is missing: $ApplicationUf2"
}
if (-not (Test-Path -LiteralPath $ArtworkPng -PathType Leaf)) {
    throw "Loader artwork is missing: $ArtworkPng"
}
$imageBytes = [IO.File]::ReadAllBytes($ApplicationUf2)
if ($imageBytes.Length -eq 0 -or $imageBytes.Length % 512 -ne 0) {
    throw 'Invalid UF2 length'
}
$applicationBlocks = 0
for ($offset = 0; $offset -lt $imageBytes.Length; $offset += 512) {
    if ([BitConverter]::ToUInt32($imageBytes,$offset) -ne 0x0A324655 -or
        [BitConverter]::ToUInt32($imageBytes,$offset+4) -ne 0x9E5D5157L -or
        [BitConverter]::ToUInt32($imageBytes,$offset+508) -ne 0x0AB16F30) {
        throw 'Invalid UF2 block magic'
    }
    $flags = [BitConverter]::ToUInt32($imageBytes,$offset+8)
    if ($flags -band 0x8000) { continue }
    $address = [BitConverter]::ToUInt32($imageBytes,$offset+12)
    $length = [BitConverter]::ToUInt32($imageBytes,$offset+16)
    $family = [BitConverter]::ToUInt32($imageBytes,$offset+28)
    if (-not ($flags -band 0x2000) -or $family -ne 0xE48BFF59L -or
        $length -eq 0 -or $length -gt 476 -or $address -lt 0x10080000 -or
        ([uint64]$address + $length) -gt 0x11000000) {
        throw 'UF2 is not an RP2350 ARM-S Loader application at 0x10080000'
    }
    ++$applicationBlocks
}
if ($applicationBlocks -eq 0) { throw 'UF2 has no application blocks' }

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
$targetUf2 = Join-Path $configRoot 'trs80.uf2'
$artworkRoot = Join-Path $emuRoot 'assets\themes\0'
$targetArtwork = Join-Path $artworkRoot 'trs80.png'
$romRoot = Join-Path $sd 'roms\TRS80'
$saveRoot = Join-Path $sd 'SAVES\TRS80'
$biosRoot = Join-Path $sd 'data\trs80\bios'

$lines = [Collections.Generic.List[string]]::new()
if (Test-Path -LiteralPath $indexPath) {
    foreach ($line in Get-Content -LiteralPath $indexPath) { $lines.Add($line) }
} else {
    $lines.Add('# pico-bootLoader application index')
    $lines.Add('# program_name;image_key;display_name[;aux_uf2]')
}
$row = 'trs80;trs80;TRS-80 Model I/III/4'
$found = $false
$active = 0
$duplicates = [Collections.Generic.List[int]]::new()
for ($i = 0; $i -lt $lines.Count; ++$i) {
    $trimmed = $lines[$i].Trim()
    if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
    ++$active
    $name = ($trimmed -split ';', 2)[0].Trim()
    if ($name -ieq 'trs80') {
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
    -Path $configRoot,$artworkRoot,$romRoot,$saveRoot,$biosRoot | Out-Null
$indexTemp = "$indexPath.trs80.tmp"
$uf2Temp = "$targetUf2.trs80.tmp"
$artTemp = "$targetArtwork.trs80.tmp"
[IO.File]::WriteAllLines($indexTemp, $lines)
if (Test-Path -LiteralPath $indexPath -PathType Leaf) {
    $indexBackup = "$indexPath.trs80.bak"
    if (-not (Test-Path -LiteralPath $indexBackup)) {
        Copy-Item -LiteralPath $indexPath -Destination $indexBackup
    }
}
Copy-Item -LiteralPath $ApplicationUf2 -Destination $uf2Temp -Force
Copy-Item -LiteralPath $ArtworkPng -Destination $artTemp -Force
$sourceUf2Hash = (Get-FileHash -LiteralPath $ApplicationUf2 -Algorithm SHA256).Hash
$sourceArtHash = (Get-FileHash -LiteralPath $ArtworkPng -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath $uf2Temp -Algorithm SHA256).Hash -ne $sourceUf2Hash -or
    (Get-FileHash -LiteralPath $artTemp -Algorithm SHA256).Hash -ne $sourceArtHash) {
    throw 'Staged TRS80 file checksum mismatch'
}
Move-Item -LiteralPath $indexTemp -Destination $indexPath -Force
Move-Item -LiteralPath $uf2Temp -Destination $targetUf2 -Force
Move-Item -LiteralPath $artTemp -Destination $targetArtwork -Force
foreach ($suffix in '.444','.555') {
    $cache = Join-Path $artworkRoot "trs80$suffix"
    if (Test-Path -LiteralPath $cache) { Remove-Item -LiteralPath $cache -Force }
}
if ((Get-FileHash -LiteralPath $targetUf2 -Algorithm SHA256).Hash -ne $sourceUf2Hash -or
    (Get-FileHash -LiteralPath $targetArtwork -Algorithm SHA256).Hash -ne $sourceArtHash) {
    throw 'Installed TRS80 file checksum mismatch'
}

$roms = @(Get-ChildItem -LiteralPath $romRoot -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @('.cmd','.jv1','.jv3') })
Write-Output 'TRS80 SD INSTALL: CONTENT INSTALLED'
Write-Output "Application: $targetUf2"
Write-Output "Artwork:     $targetArtwork"
Write-Output "ROM folder:  $romRoot"
Write-Output "Save folder: $saveRoot"
Write-Output "App SHA-256: $sourceUf2Hash"
Write-Output "Art SHA-256: $sourceArtHash"
Write-Output "Compatible media files: $($roms.Count)"
Write-Output 'Hardware acceptance: PENDING (installation is not runtime validation)'
