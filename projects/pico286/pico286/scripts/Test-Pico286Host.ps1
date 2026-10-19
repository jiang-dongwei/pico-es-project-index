[CmdletBinding()]
param(
    [string]$Upstream = "",
    [string]$BuildDirectory = ""
)

$ErrorActionPreference = "Stop"
$expectedCommit = "519a857001220a54ed0c9224be1d741a84b362a1"

if ([string]::IsNullOrWhiteSpace($Upstream)) {
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
    $Upstream = Join-Path $repoRoot "_deps\pico-286-upstream"
    if (-not (Test-Path -LiteralPath $Upstream)) {
        $sharedRoot = Split-Path -Parent $repoRoot
        $Upstream = Join-Path $sharedRoot "pico-286-upstream"
    }
}

$Upstream = (Resolve-Path -LiteralPath $Upstream).Path
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $Upstream "build-s15-direct-test"
}

$actualCommit = (& git -C $Upstream rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "pico-286 upstream mismatch: expected $expectedCommit, got $actualCommit"
}

$compiler = Get-Command gcc -ErrorAction Stop
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
$source = Join-Path $Upstream "tests\i82_controllers_test.c"
$executable = Join-Path $BuildDirectory "pico286_i82_test.exe"

& $compiler.Source -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow `
    $source -o $executable
if ($LASTEXITCODE -ne 0) { throw "Host controller test build failed" }

& $executable
if ($LASTEXITCODE -ne 0) { throw "Host controller test failed" }

$pico286Root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$contractSource = Join-Path $pico286Root "platform\retropicoes\platform_contract.c"
$contractTest = Join-Path $pico286Root "tests\test_platform_contract.c"
$contractInclude = Join-Path $pico286Root "platform\retropicoes"
$contractExecutable = Join-Path $BuildDirectory "pico286_platform_contract.exe"

& $compiler.Source -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow `
    "-I$contractInclude" $contractSource $contractTest -o $contractExecutable
if ($LASTEXITCODE -ne 0) { throw "Platform contract test build failed" }

& $contractExecutable
if ($LASTEXITCODE -ne 0) { throw "Platform contract test failed" }

$boardSource = Join-Path $pico286Root "platform\retropicoes\board_profile.c"
$memorySource = Join-Path $pico286Root "platform\retropicoes\memory_layout.c"
$boardTest = Join-Path $pico286Root "tests\test_board_profile.c"
$boardExecutable = Join-Path $BuildDirectory "pico286_board_profile.exe"

& $compiler.Source -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow `
    "-I$contractInclude" $boardSource $memorySource $boardTest `
    -o $boardExecutable
if ($LASTEXITCODE -ne 0) { throw "Board profile test build failed" }

& $boardExecutable
if ($LASTEXITCODE -ne 0) { throw "Board profile test failed" }

$memoryTest = Join-Path $pico286Root "tests\test_memory_layout.c"
$memoryExecutable = Join-Path $BuildDirectory "pico286_memory_layout.exe"

& $compiler.Source -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow `
    "-I$contractInclude" $memorySource $memoryTest -o $memoryExecutable
if ($LASTEXITCODE -ne 0) { throw "Memory layout test build failed" }

& $memoryExecutable
if ($LASTEXITCODE -ne 0) { throw "Memory layout test failed" }

$scanlineSource = Join-Path $pico286Root "platform\retropicoes\video_scanline.c"
$scanlineTest = Join-Path $pico286Root "tests\test_video_scanline.c"
$scanlineExecutable = Join-Path $BuildDirectory "pico286_video_scanline.exe"

& $compiler.Source -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow `
    "-I$contractInclude" $scanlineSource $scanlineTest -o $scanlineExecutable
if ($LASTEXITCODE -ne 0) { throw "Video scanline test build failed" }

& $scanlineExecutable
if ($LASTEXITCODE -ne 0) { throw "Video scanline test failed" }

$eventsSource = Join-Path $pico286Root "platform\retropicoes\core_events.c"
$eventsTest = Join-Path $pico286Root "tests\test_core_events.c"
$eventsExecutable = Join-Path $BuildDirectory "pico286_core_events.exe"

& $compiler.Source -std=c17 -Wall -Wextra -Werror -Wconversion -Wshadow `
    "-I$contractInclude" $eventsSource $eventsTest -o $eventsExecutable
if ($LASTEXITCODE -ne 0) { throw "Core event test build failed" }

& $eventsExecutable
if ($LASTEXITCODE -ne 0) { throw "Core event test failed" }

Write-Host "PICO286 HOST BASELINE: PASS commit=$actualCommit"
