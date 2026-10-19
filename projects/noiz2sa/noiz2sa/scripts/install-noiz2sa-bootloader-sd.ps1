[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SdRoot,
    [string]$ApplicationUf2 = "",
    [string]$ArtworkPng = "",
    [switch]$AllowDirectoryRootForTest
)

# Installs only this port's Loader artifacts.  User-supplied game data and
# preferences are deliberately neither read nor copied by this script.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($ApplicationUf2)) {
    $ApplicationUf2 = Join-Path $projectRoot 'build-pico-retropicoes-v21-bootloader-make\noiz2sa.uf2'
}
if ([string]::IsNullOrWhiteSpace($ArtworkPng)) {
    $ArtworkPng = Join-Path $projectRoot 'assets\loader\noiz2sa.png'
}
$ApplicationUf2 = [IO.Path]::GetFullPath($ApplicationUf2)
$ArtworkPng = [IO.Path]::GetFullPath($ArtworkPng)
foreach ($source in @($ApplicationUf2, $ArtworkPng)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required delivery artifact is missing: $source"
    }
}

$sd = [IO.Path]::GetFullPath($SdRoot)
if (-not (Test-Path -LiteralPath $sd -PathType Container)) { throw "SD root does not exist: $sd" }
$driveRoot = [IO.Path]::GetPathRoot($sd)
if (-not $AllowDirectoryRootForTest -and $sd.TrimEnd('\') -ne $driveRoot.TrimEnd('\')) {
    throw "SdRoot must be a mounted drive root such as H:\, got: $sd"
}

$emu = Join-Path $sd 'emu'
$appRoot = Join-Path $emu '16'
$artRoot = Join-Path $emu 'assets\themes\0'
$index = Join-Path $emu 'emulators.txt'
$targetUf2 = Join-Path $appRoot 'noiz2sa.uf2'
$targetArt = Join-Path $artRoot 'noiz2sa.png'

$lines = [Collections.Generic.List[string]]::new()
if (Test-Path -LiteralPath $index) { foreach ($line in Get-Content -LiteralPath $index) { $lines.Add($line) } }
else {
    $lines.Add('# pico-bootLoader application index')
    $lines.Add('# program_name;image_key;display_name[;aux_uf2]')
}
$row = 'noiz2sa;noiz2sa;Noiz2sa'
$found = $false; $active = 0; $duplicates = [Collections.Generic.List[int]]::new()
for ($i = 0; $i -lt $lines.Count; ++$i) {
    $trimmed = $lines[$i].Trim()
    if ($trimmed.Length -eq 0 -or $trimmed.StartsWith('#')) { continue }
    ++$active
    if ((($trimmed -split ';', 2)[0].Trim()) -ieq 'noiz2sa') {
        if (-not $found) { $lines[$i] = $row; $found = $true } else { $duplicates.Add($i) }
    }
}
for ($i = $duplicates.Count - 1; $i -ge 0; --$i) { $lines.RemoveAt($duplicates[$i]); --$active }
if (-not $found) {
    if ($active -ge 32) { throw 'emulators.txt already has the maximum 32 entries' }
    $lines.Add($row)
}

$uf2Hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ApplicationUf2).Hash
$artHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ArtworkPng).Hash
New-Item -ItemType Directory -Force -Path $appRoot, $artRoot | Out-Null
$indexTmp = "$index.s17.tmp"; $uf2Tmp = "$targetUf2.s17.tmp"; $artTmp = "$targetArt.s17.tmp"
[IO.File]::WriteAllLines($indexTmp, $lines)
Copy-Item -LiteralPath $ApplicationUf2 -Destination $uf2Tmp -Force
Copy-Item -LiteralPath $ArtworkPng -Destination $artTmp -Force
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $uf2Tmp).Hash -ne $uf2Hash -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $artTmp).Hash -ne $artHash) {
    throw 'Staged noiz2sa delivery file checksum mismatch'
}
Move-Item -LiteralPath $indexTmp -Destination $index -Force
Move-Item -LiteralPath $uf2Tmp -Destination $targetUf2 -Force
Move-Item -LiteralPath $artTmp -Destination $targetArt -Force
foreach ($suffix in @('.444', '.555')) {
    $cache = Join-Path $artRoot "noiz2sa$suffix"
    if (Test-Path -LiteralPath $cache -PathType Leaf) { Remove-Item -LiteralPath $cache -Force }
}
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $targetUf2).Hash -ne $uf2Hash -or
    (Get-FileHash -Algorithm SHA256 -LiteralPath $targetArt).Hash -ne $artHash) {
    throw 'Installed noiz2sa delivery file checksum mismatch'
}

$data = Join-Path $sd 'data\noiz2sa'
$required = @('images\title_n.bmp','images\title_o.bmp','images\title_i.bmp','images\title_z.bmp',
              'images\title_2.bmp','images\title_s.bmp','images\title_a.bmp',
              'sounds\stg0.ogg','sounds\stg00.ogg','sounds\stg1.ogg','sounds\stg2.ogg',
              'sounds\stg3.ogg','sounds\stg4.ogg','sounds\stg5.ogg','sounds\shot.wav',
              'sounds\hit.wav','sounds\foedst.wav','sounds\bossdst.wav','sounds\shipdst.wav',
              'sounds\bonus.wav','sounds\extend.wav')
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $data $_) -PathType Leaf) })
$xmlCounts = @{ zako = 18; middle = 23; boss = 32 }
foreach ($name in $xmlCounts.Keys) {
    $count = @(Get-ChildItem -LiteralPath (Join-Path $data $name) -Filter '*.xml' -File -ErrorAction SilentlyContinue).Count
    if ($count -lt $xmlCounts[$name]) { $missing += "$name/*.xml (need $($xmlCounts[$name]), found $count)" }
}
Write-Output 'S17 CONTENT INSTALL: PASS'
Write-Output "Application: $targetUf2"
Write-Output "Artwork:     $targetArt"
Write-Output "Index:       $index"
if ($missing.Count) {
    Write-Output 'S17 HARDWARE READINESS: BLOCKED'
    Write-Output ('Reason: /data/noiz2sa is incomplete: ' + ($missing -join ', '))
} else { Write-Output 'S17 HARDWARE READINESS: READY' }
