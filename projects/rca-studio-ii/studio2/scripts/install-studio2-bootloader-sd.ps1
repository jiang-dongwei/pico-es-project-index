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
        'build-pico-retropicoes-v21-bootloader-make\studio2.uf2'
}
if ([string]::IsNullOrWhiteSpace($ArtworkPng)) {
    $ArtworkPng = Join-Path $projectRoot 'assets\loader\studio2.png'
}
$ApplicationUf2 = [IO.Path]::GetFullPath($ApplicationUf2)
$ArtworkPng = [IO.Path]::GetFullPath($ArtworkPng)
if (-not (Test-Path -LiteralPath $ApplicationUf2 -PathType Leaf)) {
    throw "Loader application UF2 is missing: $ApplicationUf2"
}
if (-not (Test-Path -LiteralPath $ArtworkPng -PathType Leaf)) {
    throw "Loader artwork is missing: $ArtworkPng"
}

$uf2Bytes = [IO.File]::ReadAllBytes($ApplicationUf2)
if ($uf2Bytes.Length -eq 0 -or $uf2Bytes.Length % 512 -ne 0) { throw 'Invalid UF2 block length' }
$minimum = [uint32]::MaxValue
for ($offset = 0; $offset -lt $uf2Bytes.Length; $offset += 512) {
    if ([BitConverter]::ToUInt32($uf2Bytes,$offset) -ne 0x0A324655 -or [BitConverter]::ToUInt32($uf2Bytes,$offset+508) -ne 0x0AB16F30) { throw 'Invalid UF2 magic' }
    $address = [BitConverter]::ToUInt32($uf2Bytes,$offset+12)
    $size = [BitConverter]::ToUInt32($uf2Bytes,$offset+16)
    if ($address -lt $minimum) { $minimum = $address }
    if ($address -lt 0x10080000 -or [uint64]$address + $size -gt 0x11000000) { throw 'UF2 lies outside Loader application partition' }
}
if ($minimum -ne 0x10080000) { throw 'Expected Loader application at 0x10080000' }

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
$targetUf2 = Join-Path $configRoot 'studio2.uf2'
$artworkRoot = Join-Path $emuRoot 'assets\themes\0'
$targetArtwork = Join-Path $artworkRoot 'studio2.png'
$romRoot = Join-Path $sd 'roms\STUDIO2'
$saveRoot = Join-Path $sd 'SAVES\STUDIO2'

$lines = [Collections.Generic.List[string]]::new()
if (Test-Path -LiteralPath $indexPath) {
    foreach ($line in Get-Content -LiteralPath $indexPath) { $lines.Add($line) }
} else {
    $lines.Add('# pico-bootLoader application index')
    $lines.Add('# program_name;image_key;display_name[;aux_uf2]')
}
$row = 'studio2;studio2;RCA Studio II'
$found = $false
$active = 0
$duplicates = [Collections.Generic.List[int]]::new()
for ($i = 0; $i -lt $lines.Count; ++$i) {
    $trimmed = $lines[$i].Trim()
    if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
    ++$active
    $name = ($trimmed -split ';', 2)[0].Trim()
    if ($name -ieq 'studio2') {
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
$indexTemp = "$indexPath.studio2.tmp"
$uf2Temp = "$targetUf2.studio2.tmp"
$artTemp = "$targetArtwork.studio2.tmp"
[IO.File]::WriteAllLines($indexTemp, $lines)
Copy-Item -LiteralPath $ApplicationUf2 -Destination $uf2Temp -Force
Copy-Item -LiteralPath $ArtworkPng -Destination $artTemp -Force
$sourceUf2Hash = (Get-FileHash -LiteralPath $ApplicationUf2 -Algorithm SHA256).Hash
$sourceArtHash = (Get-FileHash -LiteralPath $ArtworkPng -Algorithm SHA256).Hash
if ((Get-FileHash -LiteralPath $uf2Temp -Algorithm SHA256).Hash -ne $sourceUf2Hash -or
    (Get-FileHash -LiteralPath $artTemp -Algorithm SHA256).Hash -ne $sourceArtHash) {
    throw 'Staged STUDIO2 file checksum mismatch'
}
Move-Item -LiteralPath $indexTemp -Destination $indexPath -Force
Move-Item -LiteralPath $uf2Temp -Destination $targetUf2 -Force
Move-Item -LiteralPath $artTemp -Destination $targetArtwork -Force
foreach ($suffix in '.444','.555') {
    $cache = Join-Path $artworkRoot "studio2$suffix"
    if (Test-Path -LiteralPath $cache) { Remove-Item -LiteralPath $cache -Force }
}
if ((Get-FileHash -LiteralPath $targetUf2 -Algorithm SHA256).Hash -ne $sourceUf2Hash -or
    (Get-FileHash -LiteralPath $targetArtwork -Algorithm SHA256).Hash -ne $sourceArtHash) {
    throw 'Installed STUDIO2 file checksum mismatch'
}

$roms = @(Get-ChildItem -LiteralPath $romRoot -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in @('.st2','.bin','.rom','.s2d') })
Write-Output 'STUDIO2 SD INSTALL: CONTENT INSTALLED'
Write-Output "Application: $targetUf2"
Write-Output "Artwork:     $targetArtwork"
Write-Output "ROM folder:  $romRoot"
Write-Output "Save folder: $saveRoot"
Write-Output "App SHA-256: $sourceUf2Hash"
Write-Output "Art SHA-256: $sourceArtHash"
Write-Output 'CONTENT INSTALL: PASS'
$bios = Join-Path $sd 'bios\studio2.rom'
$biosReady = (Test-Path -LiteralPath $bios -PathType Leaf) -and (Get-Item -LiteralPath $bios).Length -eq 2048
$launchable = $false
foreach ($candidate in $roms) {
    if ($candidate.Extension -ieq '.s2d' -and $candidate.Length -eq 2048) { $launchable = $true; break }
    if (-not $biosReady) { continue }
    if ($candidate.Extension -in '.bin','.rom' -and $candidate.Length -gt 0 -and $candidate.Length -le 1024) { $launchable = $true; break }
    if ($candidate.Extension -ine '.st2' -or $candidate.Length -lt 512 -or $candidate.Length -gt 2816) { continue }
    $bytes = [IO.File]::ReadAllBytes($candidate.FullName)
    if ([Text.Encoding]::ASCII.GetString($bytes,0,4) -ne 'RCA2' -or $bytes[4] -lt 2 -or $bytes[4] -gt 11 -or $bytes.Length -ne $bytes[4]*256) { continue }
    $pages = @{}
    $valid = $true
    for ($pageIndex = 0; $pageIndex -lt $bytes[4]-1; $pageIndex++) {
        $page = [int]$bytes[64+$pageIndex]
        if (($page -lt 4 -or $page -gt 15) -or ($page -ge 8 -and $page -le 11) -or $pages.ContainsKey($page)) { $valid = $false; break }
        $pages[$page] = $true
    }
    if ($valid) { $launchable = $true; break }
}
if ($launchable) {
    Write-Output 'HARDWARE READINESS: READY'
    Write-Output 'Image presence and size checked; actual launch remains hardware acceptance.'
} else {
    Write-Output 'HARDWARE READINESS: BLOCKED'
    Write-Output 'Provide .st2/.bin/.rom plus 2048-byte bios/studio2.rom, or an original .s2d diagnostic.'
}
