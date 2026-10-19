[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SdRoot,
    [string]$ApplicationUf2 = "E:\rk20-build\refkeen.uf2",
    [string]$ArtworkPng = "",
    [switch]$AllowDirectoryRootForTest
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ArtworkPng)) {
    $ArtworkPng = Join-Path $projectRoot "assets\loader\refkeen.png"
}
$ApplicationUf2 = [IO.Path]::GetFullPath($ApplicationUf2)
$ArtworkPng = [IO.Path]::GetFullPath($ArtworkPng)
foreach ($source in @($ApplicationUf2, $ArtworkPng)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required install source is missing: $source"
    }
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

$emuRoot = Join-Path $sd "emu"
$configRoot = Join-Path $emuRoot "16"
$indexPath = Join-Path $emuRoot "emulators.txt"
$targetUf2 = Join-Path $configRoot "refkeen.uf2"
$artworkRoot = Join-Path $emuRoot "assets\themes\0"
$targetArtwork = Join-Path $artworkRoot "refkeen.png"

$lines = [Collections.Generic.List[string]]::new()
if (Test-Path -LiteralPath $indexPath) {
    foreach ($line in Get-Content -LiteralPath $indexPath) { $lines.Add($line) }
} else {
    $lines.Add("# pico-bootLoader application index")
    $lines.Add("# program_name;image_key;display_name[;aux_uf2]")
}

$row = "refkeen;refkeen;Reflection Keen"
$found = $false
$activeRows = 0
$duplicates = [Collections.Generic.List[int]]::new()
for ($i = 0; $i -lt $lines.Count; ++$i) {
    $trimmed = $lines[$i].Trim()
    if ($trimmed.Length -eq 0 -or $trimmed.StartsWith("#")) { continue }
    ++$activeRows
    if ((($trimmed -split ';', 2)[0].Trim()) -ieq "refkeen") {
        if (-not $found) {
            $lines[$i] = $row
            $found = $true
        } else {
            $duplicates.Add($i)
        }
    }
}
for ($i = $duplicates.Count - 1; $i -ge 0; --$i) {
    $lines.RemoveAt($duplicates[$i])
    --$activeRows
}
if (-not $found) {
    if ($activeRows -ge 32) { throw "emulators.txt already has the maximum 32 entries" }
    $lines.Add($row)
}

New-Item -ItemType Directory -Force -Path $configRoot, $artworkRoot | Out-Null
$indexTemp = "$indexPath.s20.tmp"
$uf2Temp = "$targetUf2.s20.tmp"
$artTemp = "$targetArtwork.s20.tmp"
[IO.File]::WriteAllLines($indexTemp, $lines)
Copy-Item -LiteralPath $ApplicationUf2 -Destination $uf2Temp -Force
Copy-Item -LiteralPath $ArtworkPng -Destination $artTemp -Force

$sourceUf2Hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ApplicationUf2).Hash
$sourceArtHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ArtworkPng).Hash
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $uf2Temp).Hash -ne $sourceUf2Hash -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $artTemp).Hash -ne $sourceArtHash) {
    Remove-Item -LiteralPath $indexTemp, $uf2Temp, $artTemp -Force -ErrorAction SilentlyContinue
    throw "UF2 or artwork verification failed while staging the SD update"
}

Move-Item -LiteralPath $indexTemp -Destination $indexPath -Force
Move-Item -LiteralPath $uf2Temp -Destination $targetUf2 -Force
Move-Item -LiteralPath $artTemp -Destination $targetArtwork -Force
foreach ($extension in ".444", ".555") {
    $cache = Join-Path $artworkRoot "refkeen$extension"
    if (Test-Path -LiteralPath $cache -PathType Leaf) {
        Remove-Item -LiteralPath $cache -Force
    }
}

$targetUf2Hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetUf2).Hash
$targetArtHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetArtwork).Hash
if ($targetUf2Hash -ne $sourceUf2Hash -or $targetArtHash -ne $sourceArtHash) {
    throw "UF2 or artwork verification failed after copying to SD"
}

Write-Output "S20 SD INSTALL: PASS"
Write-Output "Application: $targetUf2"
Write-Output "Artwork:     $targetArtwork"
Write-Output "Index:       $indexPath"
Write-Output "App SHA-256: $targetUf2Hash"
Write-Output "Art SHA-256: $targetArtHash"
