[CmdletBinding()]
param(
    [string]$Upstream = "",
    [string]$BuildDirectory = ""
)

$ErrorActionPreference = "Stop"
$expectedCommit = "519a857001220a54ed0c9224be1d741a84b362a1"

if ([string]::IsNullOrWhiteSpace($Upstream)) {
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
    $Upstream = Join-Path (Split-Path -Parent $repoRoot) "pico-286-upstream"
}
$Upstream = (Resolve-Path -LiteralPath $Upstream).Path

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $Upstream "build-rp2350-s15-hw16board"
}
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path

$actualCommit = (& git -C $Upstream rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "pico-286 upstream mismatch: expected $expectedCommit, got $actualCommit"
}

$buildNinja = Join-Path $BuildDirectory "build.ninja"
$definitionLine = Select-String -LiteralPath $buildNinja `
    -Pattern "DEFINES = .*RETROPICOES_HW16" | Select-Object -First 1
if ($null -eq $definitionLine) {
    throw "Missing RETROPICOES_HW16 compile definitions"
}

$requiredDefinitions = @(
    "-DCPU_FREQ_MHZ=252",
    "-DRETROPICOES_HW16=1",
    "-DRETROPICOES_AUDIO=1",
    "-DPICO286_PSRAM_CS_PIN=47",
    "-DPICO_DEFAULT_UART_TX_PIN=2",
    "-DPICO_DEFAULT_UART_RX_PIN=3",
    "-DSDCARD_SPI_BUS=spi1",
    "-DSDCARD_PIN_SPI0_CS=43",
    "-DSDCARD_PIN_SPI0_SCK=30",
    "-DSDCARD_PIN_SPI0_MOSI=31",
    "-DSDCARD_PIN_SPI0_MISO=40",
    "-DNES_GPIO_CLK=6",
    "-DNES_GPIO_DATA=8",
    "-DNES_GPIO_LAT=7"
)
foreach ($definition in $requiredDefinitions) {
    if (-not $definitionLine.Line.Contains($definition)) {
        throw "Missing HW16 definition: $definition"
    }
}

foreach ($forbidden in @("-DPWM_BEEPER=", "-DPWM_LEFT_CHANNEL=", "-DPSRAM_PIN_CS=18")) {
    if ($definitionLine.Line.Contains($forbidden)) {
        throw "Upstream board definition leaked into HW16 build: $forbidden"
    }
}

$buildGraph = Get-Content -LiteralPath $buildNinja -Raw
$requiredPortSource = "pico286/platform/retropicoes/core_events.c"
if (-not $buildGraph.Contains($requiredPortSource)) {
    throw "HW16 build graph does not include $requiredPortSource"
}

$mainSourcePath = Join-Path $Upstream "src\pico-main.c"
$mainSource = Get-Content -LiteralPath $mainSourcePath -Raw
foreach ($requiredFragment in @(
    "static void pico286_realtime_init(void)",
    "static inline void pico286_realtime_step(void)",
    "pico286_realtime_init();",
    "pico286_realtime_step();"
)) {
    if (-not $mainSource.Contains($requiredFragment)) {
        throw "Missing bounded realtime-loop fragment: $requiredFragment"
    }
}

$objects = @(
    "CMakeFiles\286.dir\src\pico-main.c.obj",
    "CMakeFiles\286.dir\src\emulator\i8253.c.obj",
    "CMakeFiles\286.dir\src\emulator\ports.c.obj",
    "CMakeFiles\286.dir\drivers\sdcard\sdcard.c.obj"
)
foreach ($object in $objects) {
    if (-not (Test-Path -LiteralPath (Join-Path $BuildDirectory $object))) {
        throw "Missing directly compiled HW16 object: $object"
    }
}

Write-Host "PICO286 HW16 CONFIG: PASS clock=252MHz psram_cs=47 sd=spi1 objects=$($objects.Count) core_events=present realtime_step=bounded"
