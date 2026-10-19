#Requires -Version 5.1
<#
.SYNOPSIS
    SDL3 shim-surface gate for the S20 ReflectionHLE (Keen Dreams) port.

.DESCRIPTION
    Companion to Test-PlatformBoundary.ps1, asking the opposite question.

        Test-PlatformBoundary.ps1 -C99Check
            Compiles the translation units WITHOUT the shim, and reports what
            upstream still needs from the port. That is the work list.

        Test-ShimSurface.ps1  (this script)
            Compiles the same units WITH the shim, and reports whether the shim
            is sufficient. That is the measure of done.

    Both must hold for BUILD_AS_CPP=OFF to be viable: the first tells us what
    is missing, the second tells us when nothing is.

    The shim is refkeen/host/shim -- a hand-written stand-in for the SDL3
    headers. It works because upstream reaches SDL through *angle-bracket*
    includes even from its own platform-dispatch headers:

        src/backend/threading/be_mutex_sdl.h   ->  #include <SDL3/SDL_mutex.h>
        src/backend/threading/be_atomic_sdl.h  ->  #include <SDL3/SDL_atomic.h>
        src/be_st_sdl_private.h                ->  #include <SDL3/SDL.h>

    Quoted includes with an explicit relative path cannot be shadowed from the
    command line; angle-bracket ones can. Putting refkeen/host/shim first on
    the include path therefore redirects every SDL include in the tree, and
    upstream stays byte-identical (verified: `git -C _deps/reflectionhle-
    upstream status --short` is empty at tag release-20260606).

    WHAT THIS GATE DOES NOT CHECK: semantics. It proves the shim compiles the
    tree, not that its timing or atomic behaviour matches SDL on the target.
    That is an S20-D / on-hardware acceptance question.

    To prove this gate can actually fail, use Test-ShimSurfaceSelfTest.ps1.

    Nothing in the upstream tree is modified.

.NOTES
    Two things about the compile flags are load-bearing and easy to get wrong:

      * REFKEEN_PLATFORM_PICO must be defined. Without it refkeen_config.h
        leaves the desktop feature block (upstream: !ANDROID && !EMSCRIPTEN)
        switched on, and the resulting failures describe a build the port never
        performs -- wrong names, right compile errors.

      * -include string.h is required because BUILD_AS_CPP=ON has been masking
        missing includes upstream. be_video_ui.c uses memcmp/strlen/memcpy with
        no <string.h>; under C++ that is silently satisfied, under C99 it is an
        error. This was found by this gate, not assumed.

.PARAMETER UpstreamRoot
    Path to the pinned ReflectionHLE checkout. Defaults to
    _deps/reflectionhle-upstream relative to the workspace root.

.PARAMETER Gcc
    Path to gcc. Defaults to resolving gcc from PATH.

.PARAMETER ReportPath
    Optional path for a plain-text copy of the report.

.EXAMPLE
    & refkeen/scripts/Test-ShimSurface.ps1 -Gcc C:\mingw64\bin\gcc.exe `
        -ReportPath .emv2\logs\s20-shim-surface.txt
#>
[CmdletBinding()]
param(
    [string]$UpstreamRoot,
    [string]$Gcc,
    [string]$ReportPath
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$script:Report = New-Object 'System.Collections.Generic.List[string]'
$script:Problems = 0

function Emit {
    param([string]$Line)
    $script:Report.Add($Line) | Out-Null
    Write-Output $Line
}

function Fail {
    param([string]$Line)
    $script:Problems++
    Emit ("FAIL  " + $Line)
}

function Flush-Report {
    if (-not $ReportPath) { return }
    $dir = Split-Path -Parent $ReportPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    Set-Content -LiteralPath $ReportPath -Value $script:Report -Encoding UTF8
}

# Invoke gcc without letting its native stderr become a terminating error.
# With $ErrorActionPreference = 'Stop', PowerShell 5.1 escalates native stderr
# to a terminating error, which would abort this script on the first TU that is
# merely *expected* to fail.
function Invoke-Gcc {
    param([string[]]$Arguments)
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # -Width matters: Out-String defaults to the console width (often 120)
        # and FOLDS long lines, which cuts compiler diagnostics mid-path.
        $out = (& $Gcc @Arguments 2>&1 | Out-String -Width 4096)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Rc = $rc; Out = $out }
}

# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (-not $UpstreamRoot) { $UpstreamRoot = Join-Path $workspace '_deps\reflectionhle-upstream' }

$cfgDir  = Join-Path $workspace 'refkeen\host\include'
$shimDir = Join-Path $workspace 'refkeen\host\shim'
$shimSrc = Join-Path $workspace 'refkeen\host\src\sdl_shim.c'

Emit "S20 SDL3 SHIM SURFACE GATE"
Emit ("Upstream : {0}" -f $UpstreamRoot)
Emit ("Shim     : {0}" -f $shimDir)
Emit ""

# ---------------------------------------------------------------------------
# Preconditions: the port's own files must all be present before anything is
# measured, otherwise a missing shim header would masquerade as an uncovered
# SDL symbol and send us chasing the wrong problem.
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath (Join-Path $UpstreamRoot 'src') -PathType Container)) {
    Emit "NOT BOOTSTRAPPED: upstream checkout not present."
    Emit "Bootstrap _deps/reflectionhle-upstream first."
    Emit ""
    Emit "SHIM SURFACE: SKIPPED"
    Flush-Report
    exit 1
}

$shimHeaders = @('SDL.h', 'SDL_mutex.h', 'SDL_atomic.h', 'SDL_gamepad.h', 'SDL_touch.h')
foreach ($h in $shimHeaders) {
    $p = Join-Path $shimDir ('SDL3\' + $h)
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
        Fail ("shim header missing: {0}" -f $p)
    }
}
$cfgHeader = Join-Path $cfgDir 'refkeen_config.h'
if (-not (Test-Path -LiteralPath $cfgHeader -PathType Leaf)) {
    Fail ("port configuration header missing: {0}" -f $cfgHeader)
}
if (-not (Test-Path -LiteralPath $shimSrc -PathType Leaf)) {
    Fail ("shim implementation missing: {0}" -f $shimSrc)
}
if ($script:Problems -gt 0) {
    Emit ("SHIM SURFACE: FAILED ({0} problem(s))" -f $script:Problems)
    Flush-Report
    exit 1
}

# ---------------------------------------------------------------------------
# gcc
# ---------------------------------------------------------------------------
if (-not $Gcc) {
    $cmd = Get-Command gcc -ErrorAction SilentlyContinue
    if ($cmd) { $Gcc = $cmd.Source }
}
if (-not $Gcc -or -not (Test-Path -LiteralPath $Gcc)) {
    Emit "SKIPPED: gcc not found (pass -Gcc)."
    Emit ""
    Emit "SHIM SURFACE: SKIPPED"
    Flush-Report
    exit 1
}

$facts = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1')

# ---------------------------------------------------------------------------
# Source list, derived from upstream's CMakeLists so it tracks upstream
# instead of a stored copy.
# ---------------------------------------------------------------------------
$beList = @()
$beCmake = Join-Path $UpstreamRoot 'src\CMakeLists.txt'
if (Test-Path -LiteralPath $beCmake) {
    $t = [System.IO.File]::ReadAllText($beCmake)
    $head = ($t -split '(?m)^if\s*\(')[0]      # only the unconditional part
    $beList = @([regex]::Matches($head, '(?m)^\s*(src/[A-Za-z0-9_/]+\.c)\s*$') |
                ForEach-Object { $_.Groups[1].Value })
}
$kdList = @()
$kdCmake = Join-Path $UpstreamRoot 'src\kdreams\CMakeLists.txt'
if (Test-Path -LiteralPath $kdCmake) {
    $t = [System.IO.File]::ReadAllText($kdCmake)
    $kdList = @([regex]::Matches($t, '(?m)^\s*(src/kdreams/(?:lscr/)?[A-Za-z0-9_]+\.c)\s*$') |
                ForEach-Object { $_.Groups[1].Value })
}
if (($beList.Count -eq 0) -or ($kdList.Count -eq 0)) {
    Emit "SKIPPED: could not parse the upstream CMake source lists."
    Emit ""
    Emit "SHIM SURFACE: SKIPPED"
    Flush-Report
    exit 1
}

$allSources = @($beList + $kdList)
$candidates = @($allSources | Where-Object {
        ($facts.PlatformTus -notcontains $_) -and
        ($_ -notlike ($facts.LauncherPrefix + '*')) -and
        ($_ -cne 'src/actual_main.c')
    } | Sort-Object)

$knownUncovered = $facts.ShimKnownUncovered

Emit "Source list"
Emit ("  {0} TUs in upstream's CMake lists" -f $allSources.Count)
Emit ("  {0} replaced platform TUs excluded, launcher excluded, entry point excluded" -f $facts.PlatformTus.Count)
Emit ("  {0} TUs to compile against the shim" -f $candidates.Count)
Emit ("  {0} of them are known-not-covered (see the reasons below, and BoundaryFacts.psd1)" -f $knownUncovered.Count)
Emit ""

# Port build switches, in one place. Each is load-bearing; read the .NOTES
# section before changing any of them.
$defs = @(
    '-DREFKEEN_VER_KDREAMS=1'
    '-DREFKEEN_HAS_VER_KDREAMS=1'
    # Target configuration: desktop feature block off, exactly as on the board.
    '-DREFKEEN_PLATFORM_PICO=1'
    # Swap only the shim's own platform implementation for the host's.
    '-DREFKEEN_SDL_SHIM_HOST=1'
    # Compensate for upstream includes that BUILD_AS_CPP=ON was masking.
    '-include'
    'string.h'
)

# refkeen/host/shim first: that is the whole mechanism. refkeen/host/include
# carries the port's refkeen_config.h.
$incs = @('-I', $shimDir,
          '-I', $cfgDir,
          '-I', "$UpstreamRoot\src",
          '-I', "$UpstreamRoot\src\crc32",
          '-I', "$UpstreamRoot\src\depklite",
          '-I', "$UpstreamRoot\src\unlzexe",
          '-I', "$UpstreamRoot\src\kdreams",
          '-I', "$UpstreamRoot\src\kdreams\lscr")

$okCount = 0
$missing = New-Object 'System.Collections.Generic.List[string]'
$failures = New-Object 'System.Collections.Generic.List[object]'

foreach ($rel in $candidates) {
    $f = Join-Path $UpstreamRoot ($rel -replace '/', '\')
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) {
        # Upstream's CMake list mentions a file that is not in the tree: report
        # it rather than skipping silently, because a silent skip here would
        # quietly shrink the measured surface.
        $missing.Add($rel)
        continue
    }
    $o = Join-Path $env:TEMP ('shim-' + (($rel -replace '[/\\]', '_') -replace '\.c$', '.o'))
    $r = Invoke-Gcc -Arguments (@($incs) + @($defs) + @('-c', '-std=c99', '-fno-common', '-O0', '-fmessage-length=0', '-o', $o, $f))
    if ($r.Rc -eq 0) { $okCount++; continue }
    $errs = @(($r.Out -split "`r?`n") | Where-Object { $_ -match ':\d+:\d+: (error|fatal error)' } | Select-Object -First 3)
    $failures.Add([pscustomobject]@{ Rel = $rel; First = $errs })
}

# The shim's own implementation must build under the same flags.
$shimObj = Join-Path $env:TEMP 'shim-sdl_shim.o'
$shimRes = Invoke-Gcc -Arguments (@($incs) + @($defs) + @('-c', '-std=c99', '-fno-common', '-O0', '-fmessage-length=0', '-o', $shimObj, $shimSrc))

Emit "Results"
Emit ("  ok    {0} TU(s) compile against the shim" -f $okCount)
if ($shimRes.Rc -eq 0) {
    Emit "  ok    the shim implementation itself compiles"
} else {
    Fail "the shim implementation does not compile"
    foreach ($e in @(($shimRes.Out -split "`r?`n") | Where-Object { $_ -match ':\d+:\d+: (error|fatal error)' } | Select-Object -First 3)) {
        Emit ("        " + $e.Trim())
    }
}
Emit ""

foreach ($m in $missing) {
    Fail ("listed by upstream's CMake but not present in the tree: {0}" -f $m)
}

# Split the failures in two: the ones the shim is not meant to cover (counted
# and explained, not failures) and the ones it is (failures).
$notCoveredByDesign = @($failures | Where-Object { $knownUncovered.ContainsKey($_.Rel) })
$regressions = @($failures | Where-Object { -not $knownUncovered.ContainsKey($_.Rel) })

if ($notCoveredByDesign.Count -gt 0) {
    Emit ("  note  {0} TU(s) not covered by design (not shim defects):" -f $notCoveredByDesign.Count)
    foreach ($x in $notCoveredByDesign) {
        Emit ("        {0}" -f $x.Rel)
        Emit ("          {0}" -f $knownUncovered[$x.Rel])
    }
    Emit ""
}

foreach ($x in $regressions) {
    Fail ("not covered by the shim: {0}" -f $x.Rel)
    foreach ($e in $x.First) { Emit ("        " + $e.Trim()) }
}

$total = $candidates.Count
$expectedOk = $total - $knownUncovered.Count
if ($script:Problems -eq 0) {
    Emit ("  shim coverage {0}/{1}: every TU the shim is meant to cover compiles" -f $okCount, $expectedOk)
}

Emit ""
Emit "---------------------------------------------------------------------------"
if ($script:Problems -eq 0) {
    Emit ("SHIM SURFACE: PASS ({0}/{1} covered, {2} not covered by the shim)" -f `
          $okCount, $expectedOk, $notCoveredByDesign.Count)
    Flush-Report
    exit 0
}
Emit ("SHIM SURFACE: FAILED ({0} problem(s), {1}/{2} TUs compiled)" -f `
      $script:Problems, $okCount, $total)
Flush-Report
exit 1
