[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$SdRoot,
    [string]$ApplicationUf2 = "",
    [string]$ArtworkPng = "",
    [string]$ReplaceProgramName = "",
    [switch]$AllowDirectoryRootForTest
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ApplicationUf2)) {
    $ApplicationUf2 = Join-Path $projectRoot `
        'build-pico-retropicoes-v21-bootloader-make\spaceinvaders.uf2'
}
if ([string]::IsNullOrWhiteSpace($ArtworkPng)) {
    $ArtworkPng = Join-Path $projectRoot `
        'assets\loader\spaceinvaders.png'
}
$ApplicationUf2 = [IO.Path]::GetFullPath($ApplicationUf2)
$ArtworkPng = [IO.Path]::GetFullPath($ArtworkPng)
foreach ($source in @($ApplicationUf2, $ArtworkPng)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required S12 deliverable is missing: $source"
    }
}
$png = [IO.File]::ReadAllBytes($ArtworkPng)
if ($png.Length -lt 24 -or $png[0] -ne 0x89 -or $png[1] -ne 0x50 -or
    $png[2] -ne 0x4e -or $png[3] -ne 0x47) {
    throw "Loader artwork is not a PNG: $ArtworkPng"
}
$pngWidth = ([int]$png[16] -shl 24) -bor ([int]$png[17] -shl 16) -bor
    ([int]$png[18] -shl 8) -bor [int]$png[19]
$pngHeight = ([int]$png[20] -shl 24) -bor ([int]$png[21] -shl 16) -bor
    ([int]$png[22] -shl 8) -bor [int]$png[23]
if ($pngWidth -ne 320 -or $pngHeight -ne 240) {
    throw "Loader artwork must be exactly 320x240, got ${pngWidth}x${pngHeight}"
}

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
$artworkRoot = Join-Path $emuRoot 'assets\themes\0'
$indexPath = Join-Path $emuRoot 'emulators.txt'
$targetUf2 = Join-Path $configRoot 'spaceinvaders.uf2'
$targetArtwork = Join-Path $artworkRoot 'spaceinvaders.png'
$romRoot = Join-Path $sd 'roms\SPACEINVADERS'
$saveRoot = Join-Path $sd 'SAVES\spaceinvaders'

$lines = [Collections.Generic.List[string]]::new()
if (Test-Path -LiteralPath $indexPath) {
    foreach ($line in Get-Content -LiteralPath $indexPath) { $lines.Add($line) }
} else {
    $lines.Add('# pico-bootLoader application index')
    $lines.Add('# program_name;image_key;display_name[;aux_uf2]')
}
$row = 'spaceinvaders;spaceinvaders;Space Invaders'
$found = $false
$active = 0
$duplicates = [Collections.Generic.List[int]]::new()
for ($i = 0; $i -lt $lines.Count; ++$i) {
    $trimmed = $lines[$i].Trim()
    if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
    ++$active
    $name = ($trimmed -split ';', 2)[0].Trim()
    if ($name -ieq 'spaceinvaders') {
        if (-not $found) { $lines[$i] = $row; $found = $true }
        else { $duplicates.Add($i) }
    }
}
for ($i = $duplicates.Count - 1; $i -ge 0; --$i) {
    $lines.RemoveAt($duplicates[$i])
    --$active
}
if (-not $found) {
    if ($active -ge 16) {
        if ([string]::IsNullOrWhiteSpace($ReplaceProgramName)) {
            throw 'emulators.txt already has 16 entries; specify -ReplaceProgramName explicitly'
        }
        $replace = -1
        for ($i = 0; $i -lt $lines.Count; ++$i) {
            $trimmed = $lines[$i].Trim()
            if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
            if ((($trimmed -split ';', 2)[0].Trim()) -ieq
                $ReplaceProgramName) { $replace = $i; break }
        }
        if ($replace -lt 0) {
            throw "Replacement entry not found: $ReplaceProgramName"
        }
        $lines[$replace] = $row
    } else {
        $lines.Add($row)
    }
}

New-Item -ItemType Directory -Force `
    -Path $configRoot,$artworkRoot,$romRoot,$saveRoot | Out-Null
$indexTemp = "$indexPath.s12.tmp"
$uf2Temp = "$targetUf2.s12.tmp"
$artTemp = "$targetArtwork.s12.tmp"
try {
    [IO.File]::WriteAllLines($indexTemp, $lines)
    Copy-Item -LiteralPath $ApplicationUf2 -Destination $uf2Temp -Force
    Copy-Item -LiteralPath $ArtworkPng -Destination $artTemp -Force
    $sourceUf2Hash = (Get-FileHash -LiteralPath $ApplicationUf2 `
        -Algorithm SHA256).Hash
    $sourceArtHash = (Get-FileHash -LiteralPath $ArtworkPng `
        -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath $uf2Temp -Algorithm SHA256).Hash -ne
            $sourceUf2Hash -or
        (Get-FileHash -LiteralPath $artTemp -Algorithm SHA256).Hash -ne
            $sourceArtHash) {
        throw 'Staged S12 file checksum mismatch'
    }
    Move-Item -LiteralPath $indexTemp -Destination $indexPath -Force
    Move-Item -LiteralPath $uf2Temp -Destination $targetUf2 -Force
    Move-Item -LiteralPath $artTemp -Destination $targetArtwork -Force
} finally {
    Remove-Item -LiteralPath $indexTemp,$uf2Temp,$artTemp -Force `
        -ErrorAction SilentlyContinue
}
foreach ($suffix in '.444','.555') {
    $cache = Join-Path $artworkRoot "spaceinvaders$suffix"
    if (Test-Path -LiteralPath $cache) {
        Remove-Item -LiteralPath $cache -Force
    }
}
if ((Get-FileHash -LiteralPath $targetUf2 -Algorithm SHA256).Hash -ne
        $sourceUf2Hash -or
    (Get-FileHash -LiteralPath $targetArtwork -Algorithm SHA256).Hash -ne
        $sourceArtHash) {
    throw 'Installed S12 file checksum mismatch'
}

$requiredRoms = @('invaders.h','invaders.g','invaders.f','invaders.e')
$problems = [Collections.Generic.List[string]]::new()
foreach ($name in $requiredRoms) {
    $path = Join-Path $romRoot $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $problems.Add("missing $name")
    } elseif ((Get-Item -LiteralPath $path).Length -ne 2048) {
        $problems.Add("$name must be exactly 2048 bytes")
    }
}

Write-Output 'S12 CONTENT INSTALL: PASS'
Write-Output "Application: $targetUf2"
Write-Output "Artwork:     $targetArtwork"
Write-Output "ROM folder:  $romRoot"
Write-Output "Save folder: $saveRoot"
Write-Output "App SHA-256: $sourceUf2Hash"
Write-Output "Art SHA-256: $sourceArtHash"
if ($problems.Count -eq 0) {
    Write-Output 'S12 HARDWARE READINESS: READY'
    Write-Output 'ROM set: invaders.h/g/f/e, 2048 bytes each'
} else {
    Write-Output 'S12 HARDWARE READINESS: BLOCKED'
    Write-Output "Reason: $($problems -join '; ')"
}
