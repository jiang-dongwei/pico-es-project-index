#Requires -Version 5.1
<#
.SYNOPSIS
    Platform-boundary gate for the S20 ReflectionHLE (Keen Dreams) port.

.DESCRIPTION
    ReflectionHLE keeps the DOS engine and the platform layer apart, and this
    script turns that separation into something that FAILS when it is broken,
    instead of something we merely believe.

    The two invariants it enforces come from reading the upstream tree at
    release-20260606 (74f07ef) and measuring it, not from assumption:

      1. Real libSDL must be confined to a known, enumerated set of translation
         units. The engine (src/kdreams/**, src/*.c core, the support libraries)
         must NOT pull in SDL3. If it did, the backend abstraction would not
         hold and the Pico port would have to patch engine code.

      2. The SDL_-prefixed symbols used by the engine are id Software's own
         sound library (id_sd.h / id_sd.c), not libSDL. The port replaces the
         platform layer; it must NOT "fix" those names.

    Why this is worth having: the naive signal is misleading. A plain grep for
    SDL_ finds ~530 distinct symbols across 33 files, which reads like a huge
    SDL dependency. In reality the engine only uses its own sound-library names,
    and the real libSDL surface is a handful of files. Without this gate, a
    future patch that leaks an SDL include into the engine would go unnoticed
    until link time on the target.

    Nothing is modified. The only output is the report file given by -ReportPath.

.PARAMETER UpstreamRoot
    Path to the pinned ReflectionHLE checkout. Defaults to
    _deps/reflectionhle-upstream relative to the workspace root.

.PARAMETER C99Check
    Additionally compile the engine-side translation units as C99 to confirm
    that BUILD_AS_CPP=OFF remains viable, and report the measured footprint.
    Requires gcc on PATH or -Gcc.

.PARAMETER Gcc
    Path to gcc. Defaults to resolving gcc from PATH.

.PARAMETER ReportPath
    Optional path for a plain-text copy of the report.

.EXAMPLE
    & refkeen/scripts/Test-PlatformBoundary.ps1

.EXAMPLE
    & refkeen/scripts/Test-PlatformBoundary.ps1 -C99Check -ReportPath .emv2\logs\s20-boundary.txt
#>
[CmdletBinding()]
param(
    [string]$UpstreamRoot,
    [string]$Gcc,
    [string]$ReportPath,
    [switch]$C99Check
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

# ---------------------------------------------------------------------------
# Expected facts. These are the measured values at 74f07ef; a change here is
# either an upstream change or a regression, and both deserve a look.
#
# Note be_video.c is a platform TU, not a neutral core: it owns g_sdlTexture,
# calls BEL_ST_LockTexture and picks the render driver. And be_events_sdl.c is
# NOT a direct SDL3 includer -- it gets SDL through its own header. Both were
# wrong in the first draft of this list, which is exactly why the gate exists.
# ---------------------------------------------------------------------------
# Measured facts. They live in BoundaryFacts.psd1 because Test-ShimSurface.ps1
# buckets the same TU set, and a duplicated list would let the two gates drift
# into reporting two different numbers for one fact.
# ---------------------------------------------------------------------------
$facts = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1')
$EXPECTED_SDL_TUS            = @($facts.PlatformTus)
$ALLOWED_SDL_HEADERS         = @($facts.AllowedSdlHeaders)
$ALLOWED_OUTSIDE_BACKEND     = @($facts.AllowedOutsideBackend)
$LAUNCHER_PREFIX             = $facts.LauncherPrefix
$EXPECTED_BEL_ST             = @($facts.BelSt)
$EXPECTED_ID_SD_SYMBOLS      = @($facts.IdSdSymbols)


# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (-not $UpstreamRoot) { $UpstreamRoot = Join-Path $workspace '_deps\reflectionhle-upstream' }

Emit "S20 PLATFORM BOUNDARY GATE"
Emit ("Upstream : {0}" -f $UpstreamRoot)
Emit ""

if (-not (Test-Path -LiteralPath (Join-Path $UpstreamRoot 'src') -PathType Container)) {
    Emit "NOT BOOTSTRAPPED: upstream checkout not present."
    Emit "Boundary checks skipped. Bootstrap _deps/reflectionhle-upstream first."
    Emit ""
    Emit "PLATFORM BOUNDARY: SKIPPED"
    Flush-Report
    exit 1
}

$srcRoot = Join-Path $UpstreamRoot 'src'
$files = Get-ChildItem -LiteralPath $srcRoot -Recurse -File |
    Where-Object { $_.Extension -in '.c', '.h', '.cpp', '.hpp' }

# ---------------------------------------------------------------------------
# Check 1: where does real libSDL get included?
# ---------------------------------------------------------------------------
$sdlUsers = New-Object 'System.Collections.Generic.List[string]'
foreach ($f in $files) {
    $rel = $f.FullName.Substring($UpstreamRoot.Length + 1) -replace '\\', '/'
    $txt = [System.IO.File]::ReadAllText($f.FullName)
    if ($txt -match '#\s*include\s*[<"]SDL3?/[A-Za-z0-9_./-]+\.h[>"]') {
        $sdlUsers.Add($rel)
    }
}
# Scope this to src/backend on purpose. Every .c that includes SDL3 would
# otherwise land in this list, and an engine file that leaked an SDL include
# would be reported as an "extra platform TU", which is a mislabel -- Check 1
# and Check 3 are what catch engine leaks. Here we only ask whether the set of
# backend TUs the port plans to replace is still exactly what we measured.
$sdlTus = @($sdlUsers | Where-Object {
        $_ -like 'src/backend/*.c' -and
        $_ -notlike "$LAUNCHER_PREFIX*"
    } | Sort-Object)
$unexpected = New-Object 'System.Collections.Generic.List[string]'
foreach ($u in $sdlUsers) {
    if ($u -like "$LAUNCHER_PREFIX*") { continue }         # launcher is off
    if ($EXPECTED_SDL_TUS -contains $u) { continue }
    if ($ALLOWED_SDL_HEADERS -contains $u) { continue }
    if ($ALLOWED_OUTSIDE_BACKEND -contains $u) { continue }
    $unexpected.Add($u)
}

Emit "Check 1  files including real SDL3 outside the enumerated set"
if ($unexpected.Count -eq 0) {
    # Decompose the known set so that the printed arithmetic actually adds up.
    # An earlier draft printed "16 known files: 7 TUs + 5 allowed headers +
    # 2 entry/private", which sums to 14: the total came from the full list
    # while the breakdown silently dropped the two launcher files. Assert the
    # decomposition instead of trusting it.
    $launcherCount = @($sdlUsers | Where-Object { $_ -like "$LAUNCHER_PREFIX*" }).Count
    $knownParts = @($EXPECTED_SDL_TUS.Count, $ALLOWED_SDL_HEADERS.Count,
                    $ALLOWED_OUTSIDE_BACKEND.Count, $launcherCount)
    $knownSum = ($knownParts | Measure-Object -Sum).Sum
    if ($knownSum -ne $sdlUsers.Count) {
        Fail ("the known-SDL3 set does not decompose: {0} parts sum to {1}, but {2} files include SDL3" -f `
              $knownParts.Count, $knownSum, $sdlUsers.Count)
    } else {
        Emit ("  ok    none ({0} known files = {1} TUs + {2} allowed headers + {3} entry/private + {4} launcher (off))" -f `
              $sdlUsers.Count, $EXPECTED_SDL_TUS.Count, $ALLOWED_SDL_HEADERS.Count,
              $ALLOWED_OUTSIDE_BACKEND.Count, $launcherCount)
    }
} else {
    foreach ($u in $unexpected) { Fail "unexpected SDL3 include: $u" }
}
Emit ""

# ---------------------------------------------------------------------------
# Check 2: the platform TUs are exactly the ones we plan to replace
# ---------------------------------------------------------------------------
Emit "Check 2  the platform TUs to replace are unchanged in count and identity"
$missing = @($EXPECTED_SDL_TUS | Where-Object { -not ($sdlTus -contains $_) })
$extra = @($sdlTus | Where-Object { $EXPECTED_SDL_TUS -notcontains $_ })
if (($missing.Count -eq 0) -and ($extra.Count -eq 0)) {
    Emit ("  ok    all {0} expected platform TUs include SDL3; no others do" -f $EXPECTED_SDL_TUS.Count)
} else {
    foreach ($m in $missing) { Fail "expected platform TU no longer includes SDL3 (or was moved): $m" }
    foreach ($e in $extra) { Fail "unexpected additional platform TU: $e" }
}
Emit ""

# ---------------------------------------------------------------------------
# Check 3: the engine must not reference libSDL
# ---------------------------------------------------------------------------
Emit "Check 3  engine side is free of real libSDL"
$engineSdl = @($sdlUsers | Where-Object { $_ -notlike 'src/backend/*' -and $ALLOWED_OUTSIDE_BACKEND -notcontains $_ })
if ($engineSdl.Count -eq 0) {
    Emit "  ok    no engine file includes SDL3 (backend abstraction holds)"
} else {
    foreach ($e in $engineSdl) { Fail "engine file includes SDL3 - abstraction is leaking: $e" }
}
Emit ""

# ---------------------------------------------------------------------------
# Check 4: BEL_ST_* surface is still the small documented seam
# ---------------------------------------------------------------------------
Emit "Check 4  BEL_ST_* platform seam is unchanged"
$priv = Join-Path $UpstreamRoot 'src\be_st_sdl_private.h'
if (-not (Test-Path -LiteralPath $priv -PathType Leaf)) {
    Fail "missing src/be_st_sdl_private.h - the platform seam header is gone"
} else {
    $txt = [System.IO.File]::ReadAllText($priv)
    $bel = @([regex]::Matches($txt, '\bBEL_ST_[A-Za-z0-9_]+') | ForEach-Object { $_.Value } | Sort-Object -Unique)
    # C identifiers are case-sensitive, and PowerShell's -contains/-notcontains
    # are not. Use the -c forms or a pure case rename (BEL_ST_SleepMS ->
    # BEL_ST_SleepMs) silently passes as "no change".
    $added = @($bel | Where-Object { $EXPECTED_BEL_ST -cnotcontains $_ })
    $gone = @($EXPECTED_BEL_ST | Where-Object { $bel -cnotcontains $_ })
    if (($added.Count -eq 0) -and ($gone.Count -eq 0)) {
        Emit ("  ok    seam is exactly: {0}" -f ($EXPECTED_BEL_ST -join ', '))
    } else {
        foreach ($a in $added) { Fail "new BEL_ST_* in the seam (a new platform responsibility): $a" }
        foreach ($g in $gone) { Fail "BEL_ST_* disappeared from the seam: $g" }
    }
}
Emit ""

# ---------------------------------------------------------------------------
# Check 5: id's own SDL_-prefixed sound library is intact
# ---------------------------------------------------------------------------
Emit "Check 5  id's own SDL_* sound-library namespace is intact"
$sdH = Join-Path $UpstreamRoot 'src\kdreams\id_sd.h'
if (-not (Test-Path -LiteralPath $sdH -PathType Leaf)) {
    Fail "missing src/kdreams/id_sd.h"
} else {
    $txt = [System.IO.File]::ReadAllText($sdH)
    $declared = @([regex]::Matches($txt, '\bSDL_[A-Za-z0-9_]+') | ForEach-Object { $_.Value } | Sort-Object -Unique)
    $before = $script:Problems
    foreach ($sym in $EXPECTED_ID_SD_SYMBOLS) {
        if ($declared -cnotcontains $sym) {
            Fail "id sound library symbol missing from id_sd.h: $sym"
        }
    }
    if ($script:Problems -eq $before) {
        Emit ("  ok    all {0} id sound-library symbols present (these are NOT libSDL; do not rename)" -f `
              $EXPECTED_ID_SD_SYMBOLS.Count)
    }
}
Emit ""

# ---------------------------------------------------------------------------
# Check 6 (optional): C99 compile of the engine-side TUs
# ---------------------------------------------------------------------------
if ($C99Check) {
    Emit "Check 6  engine-side TUs still compile as C99 (BUILD_AS_CPP=OFF viability)"
    if (-not $Gcc) {
        $cmd = Get-Command gcc -ErrorAction SilentlyContinue
        if ($cmd) { $Gcc = $cmd.Source }
    }
    if (-not $Gcc -or -not (Test-Path -LiteralPath $Gcc)) {
        Emit "  skipped: gcc not found (pass -Gcc)"
    } else {
        # The port's own configuration header, not a scratch copy, so this
        # check is reproducible from a fresh checkout.
        #
        # Used WITHOUT refkeen/host/shim on the include path, on purpose: the
        # point of this check is to enumerate what upstream still needs from
        # the port, and putting the SDL3 shim first would hide exactly that.
        # Test-ShimSurface.ps1 is the complementary check -- same TU set, shim
        # enabled, asking instead whether the shim is sufficient.
        $cfgDir = Join-Path $workspace 'refkeen\host\include'
        if (-not (Test-Path -LiteralPath (Join-Path $cfgDir 'refkeen_config.h'))) {
            Emit ("  skipped: no refkeen_config.h at {0}" -f $cfgDir)
        } else {
            # Derive the source lists from the upstream CMakeLists rather than a
            # stored copy, so the check tracks upstream instead of drifting.
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
                Emit "  skipped: could not parse the upstream CMake source lists"
            } else {
                # REFKEEN_PLATFORM_PICO is defined so this check compiles under
                # the port's *real* target configuration. Without it the desktop
                # feature block (upstream: guarded on !ANDROID && !EMSCRIPTEN)
                # switches itself on, and the resulting failure list describes a
                # build the port never performs -- which is exactly what
                # happened before refkeen_config.h existed.
                #
                # No shim directory on the include path: see the note above.
                $defs = @('-DREFKEEN_VER_KDREAMS=1', '-DREFKEEN_HAS_VER_KDREAMS=1',
                          '-DREFKEEN_PLATFORM_PICO=1')
                $incs = @('-I', "$UpstreamRoot\src", '-I', "$UpstreamRoot\src\crc32",
                          '-I', "$UpstreamRoot\src\depklite", '-I', "$UpstreamRoot\src\unlzexe",
                          '-I', "$UpstreamRoot\src\kdreams", '-I', "$UpstreamRoot\src\kdreams\lscr",
                          '-I', $cfgDir)
                # Classify failures by their diagnosis instead of hard-coding a
                # "pending" list: a TU that fails because of a known platform
                # seam is expected, anything else is a real C99 regression.
                # Each seam is a thing the port must supply itself.
                #
                # Order matters and is not arbitrary: a TU can hit more than one
                # seam, and the first match wins. The audio TUs are the case in
                # point -- their first diagnostic names the threading header
                # (be_mutex_sdl.h), while the SDL include that header pulls in is
                # the second-order reason. Filing them under the threading seam
                # is more useful, because that header is the dispatch point the
                # port has to reason about, and it is what decides whether
                # BE_ST_Mutex_T exists at all. Keep this order in sync with any
                # other measurement script that buckets the same TUs.
                $seams = @(
                    @{ Name = 'filesys'; Count = 0; Pattern = 'be_filesystem_tchar\.h|be_filesystem_mkdir\.h|BE_FILESYSTEM_CHAR_H'; Names = (New-Object 'System.Collections.Generic.List[string]') }
                    @{ Name = 'threads'; Count = 0; Pattern = 'BE_ST_Mutex_T|be_mutex|be_atomic';                                 Names = (New-Object 'System.Collections.Generic.List[string]') }
                    @{ Name = 'sdl';     Count = 0; Pattern = 'be_st_sdl_private\.h|SDL3?/';                                        Names = (New-Object 'System.Collections.Generic.List[string]') }
                )
                $notCompilable = $EXPECTED_SDL_TUS + $ALLOWED_OUTSIDE_BACKEND
                $okCount = 0
                $candidates = @($beList + $kdList | Where-Object { $notCompilable -notcontains $_ })
                foreach ($rel in $candidates) {
                    $f = Join-Path $UpstreamRoot ($rel -replace '/', '\')
                    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { continue }
                    $o = Join-Path $env:TEMP ('s20-' + (($rel -replace '[/\\]', '_') -replace '\.c$', '.o'))
                    # Native stderr must not be escalated to a terminating error
                    # while $ErrorActionPreference is Stop -- that would abort the
                    # gate on a TU that is merely expected to fail.
                    $prev = $ErrorActionPreference
                    $ErrorActionPreference = 'Continue'
                    try {
                        # Out-String defaults to the console width and folds
                        # long lines, which cuts compiler diagnostics mid-path.
                        $diag = (& $Gcc @incs @defs '-c' '-std=c99' '-fno-common' '-O0' '-fmessage-length=0' '-o' $o $f 2>&1 | Out-String -Width 4096)
                        $rc = $LASTEXITCODE
                    } finally {
                        $ErrorActionPreference = $prev
                    }
                    if ($rc -eq 0) { $okCount++; continue }
                    $bucketed = $false
                    foreach ($s in $seams) {
                        if ($diag -match $s.Pattern) { $s.Count++; $s.Names.Add($rel); $bucketed = $true; break }
                    }
                    if ($bucketed) { continue }
                    Fail "C99 compile failed (not explained by the platform seam): $rel"
                    $firstErr = (($diag -split "`r?`n") | Where-Object { $_ -match ':\d+:\d+: error' } | Select-Object -First 1)
                    if ($firstErr) { Emit ("        " + $firstErr.Trim()) }
                }
                Emit ("  ok    {0} engine-side TUs compile as C99" -f $okCount)
                $seamNotes = @{
                    sdl     = @('  note  {0} TU(s) blocked by be_st_sdl_private.h / SDL3 headers (expected).',
                                '        The port replaces those headers; no engine edit is implied.')
                    threads = @('  note  {0} TU(s) reach SDL through the threading headers. Those',
                                '        headers only exist when REFKEEN_CONFIG_THREADS is set --',
                                '        be_mutex.h is nothing but #ifdef REFKEEN_CONFIG_THREADS around',
                                '        an include -- and be_audio_mixer.h embeds BE_ST_Mutex_T',
                                '        unconditionally. The port therefore SETS the macro (see',
                                '        refkeen/host/include/refkeen_config.h) and supplies the mutex',
                                '        and atomic types through the shim. This shows up here only',
                                '        because this check runs without the shim on purpose.')
                    filesys = @('  note  {0} TU(s) blocked by the filesystem seam. The tchar/mkdir arm takes',
                                '        the non-Windows branch (this config defines no REFKEEN_PLATFORM_*),',
                                '        and that branch assumes a POSIX host; upstream only ever exercises it',
                                '        on POSIX. The port needs its own tchar/mkdir variant.')
                }
                foreach ($s in $seams) {
                    if ($s.Count -gt 0) {
                        $lines = $seamNotes[$s.Name]
                        Emit (($lines[0] -f $s.Count))
                        for ($i = 1; $i -lt $lines.Count; $i++) { Emit $lines[$i] }
                        Emit ("        TUs: " + ($s.Names -join ', '))
                    }
                }
            }
        }
    }
    Emit ""
}

Emit "---------------------------------------------------------------------------"
if ($script:Problems -eq 0) {
    Emit "PLATFORM BOUNDARY: PASS"
    Flush-Report
    exit 0
}
Emit ("PLATFORM BOUNDARY: FAILED ({0} problem(s))" -f $script:Problems)
Flush-Report
exit 1
