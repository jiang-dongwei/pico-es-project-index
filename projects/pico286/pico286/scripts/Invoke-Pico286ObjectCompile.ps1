[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDirectory,

    [string]$SourceFileName = "pico-main.c"
)

$ErrorActionPreference = "Stop"
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$databasePath = Join-Path $BuildDirectory "compile_commands.json"
$database = Get-Content -LiteralPath $databasePath -Raw | ConvertFrom-Json
$entry = $database | Where-Object {
    [System.IO.Path]::GetFileName($_.file) -eq $SourceFileName
} | Select-Object -First 1
if ($null -eq $entry) {
    throw "Compile command not found for $SourceFileName"
}

if ($entry.command -notmatch '^(?:"([^"]+)"|(\S+))\s+([\s\S]+)$') {
    throw "Cannot parse compiler command for $SourceFileName"
}
$compiler = if ($matches[1]) { $matches[1] } else { $matches[2] }
$arguments = $matches[3]

# GCC response files treat backslashes as escapes. Preserve escaped quotes in
# definitions, but use forward slashes for Windows paths and object names.
$quoteMarker = "__PICO286_ESCAPED_QUOTE__"
$arguments = $arguments.Replace('\"', $quoteMarker)
$arguments = $arguments.Replace('\', '/')
$arguments = $arguments.Replace($quoteMarker, '\"')

$responsePath = Join-Path $BuildDirectory ".pico286-$SourceFileName.rsp"
[System.IO.File]::WriteAllText(
    $responsePath,
    $arguments,
    [System.Text.UTF8Encoding]::new($false))

Push-Location $entry.directory
try {
    & $compiler "@$responsePath"
    if ($LASTEXITCODE -ne 0) {
        throw "Object compile failed for $SourceFileName"
    }
} finally {
    Pop-Location
    Remove-Item -LiteralPath $responsePath -Force -ErrorAction SilentlyContinue
}

Write-Host "PICO286 ARM OBJECT: PASS source=$SourceFileName"
