param(
    [string]$BuildDirectory = (Join-Path $PSScriptRoot "..\build-host")
)

$ErrorActionPreference = "Stop"
$moduleRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$repoRoot = (Resolve-Path (Join-Path $moduleRoot "..")).Path
$vendorRoot = Join-Path $moduleRoot "third_party\superzazu8080"

$expected = [ordered]@{
    "LICENSE" = "C924D3588BDEC43ADB9BE0732E37D393F1C054CDEBEA3E0496C7D237E307680A"
    "i8080.c" = "ABA87F0E380E607B1971943323C96114305B889C1F76C5DB303F5262DA73DB36"
    "i8080.h" = "F78D7461AE649166B189E869C7CFDE08347219BD87FAC87FF3C6DA6C2F0866F6"
}

function Get-CanonicalTextSha256([string]$Path) {
    $text = [System.IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([System.BitConverter]::ToString(
            $sha.ComputeHash($bytes))).Replace("-", "")
    }
    finally {
        $sha.Dispose()
    }
}

foreach ($entry in $expected.GetEnumerator()) {
    $path = Join-Path $vendorRoot $entry.Key
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing pinned upstream file: $path"
    }
    $actual = Get-CanonicalTextSha256 $path
    if ($actual -ne $entry.Value) {
        throw "Pinned upstream file was modified: $($entry.Key)"
    }
}

$normalizedBuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$moduleRelative = Split-Path -Leaf $moduleRoot
$candidateFiles = git -C $repoRoot ls-files --cached --others `
    --exclude-standard -- $moduleRelative
if ($LASTEXITCODE -ne 0) { throw "Unable to enumerate Git source files" }
$candidatePaths = $candidateFiles | ForEach-Object {
    Join-Path $repoRoot $_
} | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }

$forbiddenExtensions = @(".bin", ".rom", ".zip", ".7z", ".wav", ".state")
$forbidden = $candidatePaths | Where-Object {
    $forbiddenExtensions -contains [System.IO.Path]::GetExtension(
        $_).ToLowerInvariant()
}
if ($forbidden) {
    throw "Game/test data must not be committed: $($forbidden.FullName -join ', ')"
}

$sourceFiles = $candidatePaths | Where-Object {
    [System.IO.Path]::GetExtension($_) -in @(".c", ".h")
}
$platformIncludes = Select-String -Path $sourceFiles `
    -Pattern '#\s*include\s*[<"](?:SDL|pico/|hardware/)' -CaseSensitive:$false
if ($platformIncludes) {
    throw "S12-A core must remain host/platform independent: $($platformIncludes.Path -join ', ')"
}

cmake -S $moduleRoot -B $normalizedBuildDirectory -G Ninja `
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
cmake --build $normalizedBuildDirectory
if ($LASTEXITCODE -ne 0) { throw "Host build failed" }
ctest --test-dir $normalizedBuildDirectory --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Host tests failed" }

Write-Host "S12-A SOURCE BASELINE: PASS"
Write-Host "repo=$repoRoot"
Write-Host "upstream=superzazu/8080@274ffd700b81baabea99b0963bc1260b67132185"
Write-Host "rom_data=absent"
