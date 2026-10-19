[CmdletBinding()]
param(
    [string]$Destination = ""
)

$ErrorActionPreference = "Stop"
$repository = "https://github.com/xrip/pico-286.git"
$commit = "519a857001220a54ed0c9224be1d741a84b362a1"
$patchRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\patches")).Path

if ([string]::IsNullOrWhiteSpace($Destination)) {
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
    $sharedRoot = Split-Path -Parent $repoRoot
    $Destination = Join-Path $sharedRoot "pico-286-upstream"
}

if (-not (Test-Path -LiteralPath $Destination)) {
    git clone --filter=blob:none $repository $Destination
    if ($LASTEXITCODE -ne 0) { throw "Failed to clone $repository" }
}

$Destination = (Resolve-Path -LiteralPath $Destination).Path
$origin = (& git -C $Destination remote get-url origin).Trim()
if ($LASTEXITCODE -ne 0 -or $origin -notmatch "(^|/)xrip/pico-286(?:\.git)?$") {
    throw "Unexpected pico-286 origin: $origin"
}

git -C $Destination cat-file -e "$commit`^{commit}"
if ($LASTEXITCODE -ne 0) {
    git -C $Destination fetch --filter=blob:none origin $commit
    if ($LASTEXITCODE -ne 0) { throw "Failed to fetch pinned pico-286 commit" }
}

git -C $Destination checkout --detach $commit
if ($LASTEXITCODE -ne 0) { throw "Failed to checkout pinned pico-286 commit" }

# git apply matches patch context byte-for-byte. Windows checkouts may use
# CRLF while patches use Git's canonical LF, and sequential patches can then
# leave a file with mixed line endings. Normalize only patch-owned text files.
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$patchOwnedFiles = @(
    "CMakeLists.txt",
    "src\pico-main.c",
    "src\emulator\i8253.h",
    "src\emulator\ports.c"
)
function Convert-PatchOwnedFilesToLf {
    foreach ($relativePath in $patchOwnedFiles) {
        $path = Join-Path $Destination $relativePath
        $content = [System.IO.File]::ReadAllText($path)
        $content = $content.Replace("`r`n", "`n")
        [System.IO.File]::WriteAllText($path, $content, $utf8NoBom)
    }
}

$patches = Get-ChildItem -LiteralPath $patchRoot -Filter "*.patch" | Sort-Object Name
$reversedPatches = [System.Collections.Generic.List[object]]::new()
$completeStackPresent = $true
foreach ($patch in ($patches | Sort-Object Name -Descending)) {
    Convert-PatchOwnedFilesToLf
    & git -C $Destination apply --reverse --check $patch.FullName 2>$null
    if ($LASTEXITCODE -ne 0) {
        $completeStackPresent = $false
        break
    }
    & git -C $Destination apply --reverse $patch.FullName
    if ($LASTEXITCODE -ne 0) { throw "Failed to peel patch: $($patch.Name)" }
    $reversedPatches.Add($patch)
}

if (-not $completeStackPresent -and $reversedPatches.Count -ne 0) {
    foreach ($patch in ($reversedPatches | Sort-Object Name)) {
        Convert-PatchOwnedFilesToLf
        & git -C $Destination apply $patch.FullName
        if ($LASTEXITCODE -ne 0) { throw "Failed to restore patch: $($patch.Name)" }
    }
    throw "Incomplete or damaged pico-286 patch stack"
}

foreach ($patch in $patches) {
    Convert-PatchOwnedFilesToLf
    if (-not $completeStackPresent) {
        & git -C $Destination apply --reverse --check $patch.FullName 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "PICO286 PATCH: PRESENT $($patch.Name)"
            continue
        }
    }
    & git -C $Destination apply --check $patch.FullName
    if ($LASTEXITCODE -ne 0) { throw "Patch does not apply cleanly: $($patch.Name)" }
    & git -C $Destination apply $patch.FullName
    if ($LASTEXITCODE -ne 0) { throw "Failed to apply patch: $($patch.Name)" }
    $state = $completeStackPresent ? "PRESENT" : "APPLIED"
    Write-Host "PICO286 PATCH: $state $($patch.Name)"
}

$actual = (& git -C $Destination rev-parse HEAD).Trim()
if ($actual -ne $commit) {
    throw "Pinned checkout validation failed: commit=$actual"
}

Convert-PatchOwnedFilesToLf
$lastPatch = $patches | Select-Object -Last 1
& git -C $Destination apply --reverse --check $lastPatch.FullName
if ($LASTEXITCODE -ne 0) { throw "Applied patch validation failed: $($lastPatch.Name)" }

Write-Host "PICO286 UPSTREAM: PASS path=$Destination commit=$actual patches=$($patches.Count)"
