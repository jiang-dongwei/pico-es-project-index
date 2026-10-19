#Requires -Version 5.1
<#
    Negative-path harness for refkeen/scripts/Test-PlatformBoundary.ps1.

    A gate that cannot fail is decoration. This harness builds a small synthetic
    upstream tree that is *deliberately clean*, confirms the gate PASSes on it,
    then injects one defect at a time and confirms the gate FAILs with the exact
    message we expect. Nothing here touches the real upstream checkout.

    The synthetic trees it creates land under _deps/_probe/neg/<runid>-<case>, which
    is gitignored: this script is the deliverable, its scratch trees are not.
#>
[CmdletBinding()]
param(
    [string]$Workspace,
    [string]$ReportPath,
    # Fresh tree per run: the sandbox refuses recursive deletes, so a run must
    # never need to clean up after a previous one.
    [string]$RunId
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if (-not $Workspace) {
    $Workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
}
if (-not $RunId) { $RunId = Get-Date -Format 'yyyyMMdd-HHmmss' }
$gate  = Join-Path $Workspace 'refkeen\scripts\Test-PlatformBoundary.ps1'
$base  = Join-Path $Workspace '_deps\_probe\neg'
$script:Root = ''

$script:Log  = New-Object 'System.Collections.Generic.List[string]'
$script:Bad  = 0
$script:Seen = 0

function Say {
    param([string]$Line)
    $script:Log.Add($Line)
    Write-Output $Line
}

# --- the synthetic layout mirrors the real upstream tree ----------------------
$SDL_TUS = @(
    'backend/audio/be_audio_sdl.c'
    'backend/cfg/be_cfg_sdl.c'
    'backend/filesystem/be_filesystem_app_paths.c'
    'backend/input/be_input_sdl.c'
    'backend/startup/be_startup_sdl.c'
    'backend/video/be_video.c'
    'backend/video/be_video_sdl.c'
)
$SDL_HDRS = @(
    'backend/events/be_events_sdl.h'
    'backend/input/be_input_sdl.h'
    'backend/video/be_video_sdl.h'
    'backend/threading/be_atomic_sdl.h'
    'backend/threading/be_mutex_sdl.h'
)
$ENGINE_NO_SDL = @(
    'kdreams/id_us_1.c'
    'kdreams/id_vh.c'
    'id_ca.c'
)

function Write-File {
    param([string]$Rel, [string]$Text)
    $p = Join-Path $script:Root ($Rel -replace '/', '\')
    $d = Split-Path -Parent $p
    if ($d -and -not (Test-Path -LiteralPath $d)) {
        New-Item -ItemType Directory -Force -Path $d | Out-Null
    }
    Set-Content -LiteralPath $p -Value $Text -Encoding ASCII
}

# Each case gets its own directory: the sandbox refuses recursive deletes
# (they are routed to a trash operation that fails closed), and reusing one
# tree would need exactly that. A fresh tree is cleaner anyway -- no case can
# inherit a mutation from the previous one.
function New-Synth {
    param([string]$Tag)
    $script:Root = Join-Path $base ($RunId + '-' + $Tag)
    New-Item -ItemType Directory -Force -Path $script:Root | Out-Null

    foreach ($rel in $SDL_TUS) {
        Write-File "src/$rel" ("#include <SDL3/SDL.h>`n/* " + $rel + " */`n")
    }
    foreach ($rel in $SDL_HDRS) {
        Write-File "src/$rel" ("#include <SDL3/SDL.h>`n/* " + $rel + " */`n")
    }
    foreach ($rel in $ENGINE_NO_SDL) {
        Write-File "src/$rel" ("#include ""id_heads.h""`n/* " + $rel + " */`n")
    }

    # Entry point and the backend's private header: the two files the gate allows
    # outside src/backend.
    Write-File 'src/actual_main.c' "#include <SDL3/SDL.h>`nint main(void) { return 0; }`n"
    # be_st_sdl_private.h MUST include <SDL3/SDL.h> here, exactly as the real one
    # does (upstream src/be_st_sdl_private.h:30). The gate asserts its own
    # decomposition arithmetic -- 7 TUs + 5 headers + 2 entry/private + N
    # launcher -- against the number of files that really include SDL3. Leave the
    # include out of the fixture and the sum is 14 against a measured 13, so the
    # clean baseline case fails for a reason that has nothing to do with the
    # gate's subject. That is a fixture lie, not a finding.
    Write-File 'src/be_st_sdl_private.h' @'
#ifndef BE_ST_SDL_PRIVATE_H
#define BE_ST_SDL_PRIVATE_H
#include <SDL3/SDL.h>
void BEL_ST_CPUPauseInstruction(void);
unsigned int BEL_ST_GetTicksMS(void);
unsigned long BEL_ST_GetTicksNS(void);
void BEL_ST_SleepMS(unsigned int msec);
#endif
'@

    # id Software's own sound library names -- NOT libSDL, must never be renamed.
    Write-File 'src/kdreams/id_sd.h' @'
#ifndef ID_SD_H
#define ID_SD_H
void SDL_ALPlaySound(int sound);
void SDL_ALStopSound(void);
void SDL_PCPlaySound(int sound);
void SDL_PCStopSound(void);
void SDL_SBPlaySample(void);
void SDL_SBStopSample(void);
void SDL_SSPlaySample(void);
void SDL_SSStopSample(void);
#endif
'@
}

# --- run the gate against the synthetic tree ---------------------------------
function Invoke-Gate {
    param([string]$Root)
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $txt = (& $gate -UpstreamRoot $Root 2>&1 | Out-String)
        $rc  = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return @{ Text = $txt; Exit = $rc }
}

function Assert-Case {
    param(
        [string]$Name,
        [string]$Expect,
        [string[]]$MustMatch,
        [string[]]$MustNotMatch
    )
    $script:Seen++
    $r    = Invoke-Gate -Root $script:Root
    $script:LastRun = $r
    $prob = @()
    if ($r.Exit -ne 0 -and $Expect -eq 'pass') { $prob += ("exit {0}, expected 0" -f $r.Exit) }
    if ($r.Exit -eq 0 -and $Expect -eq 'fail') { $prob += "exit 0, expected non-zero" }
    foreach ($rx in $MustMatch) {
        if ($r.Text -notmatch $rx) { $prob += ("missing expected diagnostic: " + $rx) }
    }
    foreach ($rx in $MustNotMatch) {
        if ($r.Text -match $rx) { $prob += ("unexpected diagnostic: " + $rx) }
    }
    if ($prob.Count -eq 0) {
        Say ("  ok    {0}" -f $Name)
    } else {
        $script:Bad++
        Say ("  FAIL  {0}" -f $Name)
        foreach ($p in $prob) { Say ("        " + $p) }
        foreach ($l in ($r.Text -split "`r?`n")) {
            if ($l -match '^(FAIL|PLATFORM BOUNDARY)') { Say ("        | " + $l) }
        }
    }
}

Say "S20 PLATFORM BOUNDARY GATE - NEGATIVE PATH HARNESS"
Say ("Gate     : {0}" -f $gate)
Say ("Cases    : {0}\{1}-<case>" -f $base, $RunId)
Say ""

# --- C0: the clean baseline must PASS ---------------------------------------
Say "C0  clean synthetic tree -> the gate must PASS"
New-Synth -Tag 'c0'
Assert-Case -Name 'clean baseline passes' -Expect 'pass' `
    -MustMatch    @('PLATFORM BOUNDARY: PASS', 'all 7 expected platform TUs include SDL3') `
    -MustNotMatch @('FAIL')
Say ""

# --- C1: an engine file leaks libSDL ---------------------------------------
Say "C1  engine file starts including SDL3 -> abstraction leak must FAIL"
New-Synth -Tag 'c1'
Write-File 'src/kdreams/leak.c' "#include <SDL3/SDL.h>`n/* an engine file that leaked SDL */`n"
# The leak must be reported as an engine leak (Check 1 + Check 3) and NOT as an
# "additional platform TU" (Check 2) -- Check 2 is scoped to src/backend.
Assert-Case -Name 'engine SDL leak detected' -Expect 'fail' `
    -MustMatch    @('engine file includes SDL3 - abstraction is leaking: src/kdreams/leak\.c',
                    'unexpected SDL3 include: src/kdreams/leak\.c',
                    'PLATFORM BOUNDARY: FAILED') `
    -MustNotMatch @('unexpected additional platform TU: src/kdreams/leak\.c')
Say ""

# --- C2: a platform TU we plan to replace loses its SDL include -------------
Say "C2  be_video.c stops including SDL3 -> the seam moved, must FAIL"
New-Synth -Tag 'c2'
Write-File 'src/backend/video/be_video.c' "#include ""be_video.h""`n/* someone refactored the SDL include out */`n"
Assert-Case -Name 'platform TU identity change detected' -Expect 'fail' `
    -MustMatch @('expected platform TU no longer includes SDL3 \(or was moved\): src/backend/video/be_video\.c')
Say ""

# --- C3: the BEL_ST_ seam gains/loses a symbol ------------------------------
Say "C3  BEL_ST_SleepMS renamed -> a new platform responsibility, must FAIL"
New-Synth -Tag 'c3'
Write-File 'src/be_st_sdl_private.h' @'
#ifndef BE_ST_SDL_PRIVATE_H
#define BE_ST_SDL_PRIVATE_H
#include <SDL3/SDL.h>
void BEL_ST_CPUPauseInstruction(void);
unsigned int BEL_ST_GetTicksMS(void);
unsigned long BEL_ST_GetTicksNS(void);
void BEL_ST_SleepMs(unsigned int msec);
void BEL_ST_SetWindowTitle(const char *t);
#endif
'@
Assert-Case -Name 'seam rename/extension detected' -Expect 'fail' `
    -MustMatch @('new BEL_ST_\* in the seam \(a new platform responsibility\): BEL_ST_SetWindowTitle',
                 'BEL_ST_\* disappeared from the seam: BEL_ST_SleepMS')
Say ""

# --- C4: someone "fixes" id's own SDL_ names --------------------------------
Say "C4  id sound-library symbol dropped from id_sd.h -> must FAIL"
New-Synth -Tag 'c4'
Write-File 'src/kdreams/id_sd.h' @'
#ifndef ID_SD_H
#define ID_SD_H
void SDL_ALPlaySound(int sound);
void SDL_ALStopSound(void);
void SDL_PCPlaySound(int sound);
void SDL_SBPlaySample(void);
void SDL_SBStopSample(void);
void SDL_SSPlaySample(void);
void SDL_SSStopSample(void);
#endif
'@
Assert-Case -Name 'id sound-library namespace guarded' -Expect 'fail' `
    -MustMatch @('id sound library symbol missing from id_sd\.h: SDL_PCStopSound')
Say ""

# --- C4b: the same change, differing only in case ---------------------------
Say "C4b id sound-library symbol renamed in case only (SDL_PCStopSound -> SDL_PCStopsound)"
New-Synth -Tag 'c4b'
Write-File 'src/kdreams/id_sd.h' @'
#ifndef ID_SD_H
#define ID_SD_H
void SDL_ALPlaySound(int sound);
void SDL_ALStopSound(void);
void SDL_PCPlaySound(int sound);
void SDL_PCStopsound(void);
void SDL_SBPlaySample(void);
void SDL_SBStopSample(void);
void SDL_SSPlaySample(void);
void SDL_SSStopSample(void);
#endif
'@
Assert-Case -Name 'case-only rename detected' -Expect 'fail' `
    -MustMatch @('id sound library symbol missing from id_sd\.h: SDL_PCStopSound')
Say ""

# --- C5: a brand-new SDL TU appears -----------------------------------------
Say "C5  a new SDL3 TU appears in the backend -> must FAIL"
New-Synth -Tag 'c5'
Write-File 'src/backend/video/be_video_new.c' "#include <SDL3/SDL.h>`n/* a new platform TU nobody enumerated */`n"
Assert-Case -Name 'new platform TU detected' -Expect 'fail' `
    -MustMatch @('unexpected additional platform TU: src/backend/video/be_video_new\.c')
Say ""

# --- C6: not bootstrapped -> loud skip, non-zero ----------------------------
Say "C6  no upstream checkout -> loud skip, non-zero exit"
$absent = Join-Path $base ($RunId + '-c6-absent-do-not-create')
$r = Invoke-Gate -Root $absent
$script:Seen++
$p = @()
if ($r.Exit -eq 0) { $p += "exit 0, expected non-zero" }
if ($r.Text -notmatch 'NOT BOOTSTRAPPED') { $p += 'missing NOT BOOTSTRAPPED' }
if ($r.Text -notmatch 'PLATFORM BOUNDARY: SKIPPED') { $p += 'missing SKIPPED verdict' }
if ($p.Count -eq 0) { Say "  ok    loud skip when not bootstrapped" }
else {
    $script:Bad++
    Say "  FAIL  loud skip when not bootstrapped"
    foreach ($x in $p) { Say ("        " + $x) }
}
Say ""

Say "---------------------------------------------------------------------------"
if ($script:Bad -eq 0) {
    Say ("BOUNDARY NEGATIVE TESTS: PASS ({0}/{0})" -f $script:Seen)
} else {
    Say ("BOUNDARY NEGATIVE TESTS: FAILED ({0} of {1} case(s) bad)" -f $script:Bad, $script:Seen)
}

if ($ReportPath) {
    $d = Split-Path -Parent $ReportPath
    if ($d -and -not (Test-Path -LiteralPath $d)) { New-Item -ItemType Directory -Force -Path $d | Out-Null }
    Set-Content -LiteralPath $ReportPath -Value $script:Log -Encoding UTF8
}

if ($script:Bad -eq 0) { exit 0 } else { exit 1 }
