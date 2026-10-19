[CmdletBinding()]
param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
)

$ErrorActionPreference = 'Stop'

$expected = [ordered]@{
    'compat.h' = 'E6DD36193B0DB726CDBF6C8615F7FDA2BE8782716D00F6732B57AAB072BC93FD'
    'audio.h'  = '41CC24FE4EB99DD76E6CCA739DECD4F967A81C5FF8C6284FE78D63992B84F5C6'
    'audio.c'  = '99A073CFC634E7F2761CC03B17117AADA05C1DABD7C1552F8E6244542B8C3E1C'
    'video.h'  = '82919FDA9849AEAE83EDDDA20CBCE88D7CE204B6B4DF02D9C9F2AD92F8CFD5DF'
    'video.c'  = 'F507F69A21BC0E74951A4A8F45C65DB1AB3C1B431F3E4B744DDAD0CC997C4709'
}

function Get-NormalizedSha256([string]$Path) {
    $text = Get-Content -LiteralPath $Path -Raw
    $normalized = $text -replace "`r`n?", "`n"
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($normalized)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return [Convert]::ToHexString($sha.ComputeHash($bytes))
    }
    finally {
        $sha.Dispose()
    }
}

$copies = [ordered]@{
    'canonical' = Join-Path $WorkspaceRoot 'compat'
    'SDLPoP'     = Join-Path $WorkspaceRoot 'sdlpop\compat'
    'xrick'      = Join-Path $WorkspaceRoot 'xrick\compat'
    'Digger'     = Join-Path $WorkspaceRoot 'digger\compat'
}

foreach ($copy in $copies.GetEnumerator()) {
    foreach ($file in $expected.GetEnumerator()) {
        $path = Join-Path $copy.Value $file.Key
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "compat v1 missing: $($copy.Key)/$($file.Key)"
        }
        $actual = Get-NormalizedSha256 $path
        if ($actual -ne $file.Value) {
            throw "compat v1 drift: $($copy.Key)/$($file.Key) expected=$($file.Value) actual=$actual"
        }
    }
    Write-Host "PASS $($copy.Key)"
}

Write-Host 'PASS compat v1.0.0: all adopters match the canonical manifest'
