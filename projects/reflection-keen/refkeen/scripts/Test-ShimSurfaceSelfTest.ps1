#Requires -Version 5.1
<#
.SYNOPSIS
    Negative-path self-test for Test-ShimSurface.ps1.

.DESCRIPTION
    A gate that cannot fail is worse than no gate, because it makes people
    believe something was checked. This script constructs deliberately broken
    upstream trees in a scratch directory and confirms the shim-surface gate
    goes red on each one -- while staying green on the cases it should ignore.

    It never touches the real upstream checkout or the real port files: the
    synthetic workspace is built by *copying* them into a scratch directory, so
    the same shim, config header and gate logic under test are the real ones.
    Each case gets its own directory (never reused, never deleted) because the
    sandbox redirects recursive deletion to the recycle bin and fails closed.

    Cases:
      C0  a clean synthetic tree must PASS (the release path)
      C1  a TU using an SDL symbol the shim does not cover must FAIL
      C2  a missing shim header must FAIL before anything is measured
      C3  a TU listed as known-not-yet-covered must NOT be reported as a shim
          defect -- this is what keeps the known list from hiding real rot

.PARAMETER Workspace
    Workspace root holding the real shim and gate. Defaults to two levels above
    this script.

.PARAMETER RunId
    Tag for this run's scratch directories. Use a fresh value to re-run without
    deleting anything.

.PARAMETER Gcc
    Path to gcc. Defaults to resolving gcc from PATH.

.EXAMPLE
    & refkeen/scripts/Test-ShimSurfaceSelfTest.ps1 -RunId r1
#>
[CmdletBinding()]
param(
    [string]$Workspace,
    [string]$RunId,
    [string]$Gcc
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if (-not $Workspace) {
    $Workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
}
if (-not $RunId) { $RunId = Get-Date -Format 'yyyyMMdd-HHmmss' }

$gatePath = Join-Path $Workspace 'refkeen\scripts\Test-ShimSurface.ps1'
$factsPath = Join-Path $Workspace 'refkeen\scripts\BoundaryFacts.psd1'
$realShim = Join-Path $Workspace 'refkeen\host\shim'
$realCfg = Join-Path $Workspace 'refkeen\host\include'
$realSrc = Join-Path $Workspace 'refkeen\host\src'
$base = Join-Path $Workspace ('_deps\_probe\shimself\' + $RunId)

foreach ($p in @($gatePath, $factsPath)) {
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
        Write-Output ("PRECONDITION FAILED: missing {0}" -f $p)
        exit 1
    }
}
foreach ($d in @($realShim, $realCfg, $realSrc)) {
    if (-not (Test-Path -LiteralPath $d -PathType Container)) {
        Write-Output ("PRECONDITION FAILED: missing {0}" -f $d)
        exit 1
    }
}

# The gate reads gcc from PATH unless told otherwise; resolve it once so every
# case uses the same compiler.
if (-not $Gcc) {
    $cmd = Get-Command gcc -ErrorAction SilentlyContinue
    if ($cmd) { $Gcc = $cmd.Source }
}
if (-not $Gcc -or -not (Test-Path -LiteralPath $Gcc)) {
    Write-Output "PRECONDITION FAILED: gcc not found (pass -Gcc)."
    exit 1
}

$script:CaseCount = 0
$script:BadCases = 0
$script:Log = New-Object 'System.Collections.Generic.List[string]'

function Say {
    param([string]$Line)
    $script:Log.Add($Line) | Out-Null
    Write-Output $Line
}

function Write-File {
    param([string]$Root, [string]$Rel, [string]$Text)
    $p = Join-Path $Root ($Rel -replace '/', '\')
    $d = Split-Path -Parent $p
    if ($d -and -not (Test-Path -LiteralPath $d)) {
        New-Item -ItemType Directory -Force -Path $d | Out-Null
    }
    Set-Content -LiteralPath $p -Value $Text -Encoding ASCII
}

# Build a synthetic workspace: the real port files copied in, plus a minimal
# upstream tree whose CMakeLists lists only what the case needs.
function New-Synth {
    param(
        [string]$Tag,
        [string]$BackendSource = "#include <SDL3/SDL.h>`nint ok_stub(void) { return (int)SDL_GetTicks(); }`n",
        [string]$BackendTuName = 'src/ok.c',
        [switch]$OmitAtomicHeader
    )
    $root = Join-Path $base $Tag
    if (Test-Path -LiteralPath $root) {
        throw ("case directory already exists: " + $root + " -- pick a new -RunId")
    }
    New-Item -ItemType Directory -Force -Path $root | Out-Null

    # Port files under test, copied verbatim.
    $dstShim = Join-Path $root 'refkeen\host\shim'
    New-Item -ItemType Directory -Force -Path $dstShim | Out-Null
    Copy-Item -Path (Join-Path $realShim '*') -Destination $dstShim -Recurse -Force
    if ($OmitAtomicHeader) {
        # Simulate the header never having been written, without deleting
        # anything: this case's workspace simply never receives it.
        $atomic = Join-Path $dstShim 'SDL3\SDL_atomic.h'
        if (Test-Path -LiteralPath $atomic) {
            Move-Item -LiteralPath $atomic -Destination (Join-Path $root 'SDL_atomic.h.omitted')
        }
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $root 'refkeen\host\include') | Out-Null
    Copy-Item -LiteralPath (Join-Path $realCfg 'refkeen_config.h') `
              -Destination (Join-Path $root 'refkeen\host\include\refkeen_config.h') -Force
    $dstSrc = Join-Path $root 'refkeen\host\src'
    New-Item -ItemType Directory -Force -Path $dstSrc | Out-Null
    Copy-Item -Path (Join-Path $realSrc '*') -Destination $dstSrc -Recurse -Force
    New-Item -ItemType Directory -Force -Path (Join-Path $root 'refkeen\scripts') | Out-Null
    Copy-Item -LiteralPath $gatePath  -Destination (Join-Path $root 'refkeen\scripts\Test-ShimSurface.ps1') -Force
    Copy-Item -LiteralPath $factsPath -Destination (Join-Path $root 'refkeen\scripts\BoundaryFacts.psd1') -Force

    # Minimal upstream tree.
    $up = Join-Path $root '_deps\reflectionhle-upstream'
    Write-File -Root $up -Rel 'src/CMakeLists.txt' -Text (@'
add_library(backend STATIC
    SRC_LIST_MARKER
)
'@ -replace 'SRC_LIST_MARKER', $BackendTuName)
    Write-File -Root $up -Rel $BackendTuName -Text $BackendSource
    Write-File -Root $up -Rel 'src/kdreams/CMakeLists.txt' -Text @'
add_library(kdreams STATIC
    src/kdreams/okengine.c
)
'@
    Write-File -Root $up -Rel 'src/kdreams/okengine.c' -Text "int okengine_stub(void) { return 1; }`n"
    return $root
}

function Invoke-Gate {
    param([string]$Root)
    $gate = Join-Path $Root 'refkeen\scripts\Test-ShimSurface.ps1'
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = (& $gate -Gcc $Gcc 2>&1 | Out-String -Width 4096)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Rc = $rc; Text = $out }
}

# Assert-Case records the verdict in script state and returns nothing, so that
# a caller can never swallow the result with a pipeline (the mistake that made
# an earlier harness report nothing at all).
function Assert-Case {
    param(
        [string]$Name,
        [ValidateSet('pass', 'fail')][string]$Expect,
        [string[]]$MustMatch = @(),
        [string[]]$MustNotMatch = @()
    )
    $script:CaseCount++
    $r = Invoke-Gate -Root $script:Root
    $problems = New-Object 'System.Collections.Generic.List[string]'
    foreach ($m in $MustMatch) {
        if ($r.Text -notmatch $m) { $problems.Add("expected to match /$m/ but did not") }
    }
    foreach ($m in $MustNotMatch) {
        if ($r.Text -match $m) { $problems.Add("expected NOT to match /$m/ but did") }
    }
    if ($Expect -eq 'pass' -and $r.Rc -ne 0) { $problems.Add("expected exit 0, got $($r.Rc)") }
    if ($Expect -eq 'fail' -and $r.Rc -eq 0) { $problems.Add("expected non-zero exit, got 0") }

    if ($problems.Count -eq 0) {
        Say ("  ok    {0}" -f $Name)
    } else {
        $script:BadCases++
        Say ("  FAIL  {0}" -f $Name)
        foreach ($p in $problems) { Say ("          " + $p) }
        foreach ($line in ($r.Text -split "`r?`n")) {
            if ($line -match '^(FAIL|SHIM SURFACE|  note)') { Say ("          | " + $line) }
        }
    }
}

Say "S20 SHIM SURFACE GATE -- NEGATIVE PATH SELF TEST"
Say ("Gate     : {0}" -f $gatePath)
Say ("Scratch  : {0}" -f $base)
Say ""

# --- C0: the clean synthetic tree must pass ----------------------------------
Say "C0  clean synthetic tree -> the gate must PASS"
$script:Root = New-Synth -Tag 'c0'
Assert-Case -Name 'clean tree passes' -Expect 'pass' -MustMatch @('SHIM SURFACE: PASS', 'shim implementation itself compiles')
Say ""

# --- C1: an SDL symbol the shim does not cover -------------------------------
Say "C1  a TU uses SDL_GetWindowSize (not in the shim) -> must FAIL"
$script:Root = New-Synth -Tag 'c1' `
    -BackendSource "#include <SDL3/SDL.h>`nint leak(void) { SDL_Window *w = 0; int x = 0, y = 0; SDL_GetWindowSize(w, &x, &y); return x + y; }`n"
Assert-Case -Name 'uncovered SDL symbol detected' -Expect 'fail' `
    -MustMatch @('not covered by the shim: src/ok\.c', 'SHIM SURFACE: FAILED')
Say ""

# --- C2: a shim header is missing -------------------------------------------
Say "C2  SDL_atomic.h absent -> must FAIL before measuring anything"
$script:Root = New-Synth -Tag 'c2' -OmitAtomicHeader
Assert-Case -Name 'missing shim header detected' -Expect 'fail' `
    -MustMatch @('shim header missing', 'SHIM SURFACE: FAILED')
Say ""

# --- C3: a known-not-yet-covered TU must not be reported as a defect ---------
Say "C3  a TU listed as known-not-yet-covered -> must STAY PASS"
$script:Root = New-Synth -Tag 'c3' `
    -BackendTuName 'src/backend/events/be_events_sdl.c' `
    -BackendSource "#include <SDL3/SDL.h>`nint ev(void) { SDL_DisplayID d = 0; (void)d; return (int)SDL_SCANCODE_A; }`n"
Assert-Case -Name 'known list does not create false failures' -Expect 'pass' `
    -MustMatch @('not covered by design', 'not covered by the shim', 'SHIM SURFACE: PASS')
Say ""

Say "---------------------------------------------------------------------------"
if ($script:BadCases -eq 0) {
    Say ("SHIM SURFACE NEGATIVE TESTS: PASS ({0}/{0})" -f $script:CaseCount)
    exit 0
}
Say ("SHIM SURFACE NEGATIVE TESTS: FAILED ({0} of {1} cases)" -f $script:BadCases, $script:CaseCount)
exit 1
