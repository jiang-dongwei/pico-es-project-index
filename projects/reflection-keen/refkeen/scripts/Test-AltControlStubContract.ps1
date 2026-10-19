<#
.SYNOPSIS
    S20-C altcontrol gate -- the contract the port's stub for the altcontrol
    subsystem has to satisfy.

.DESCRIPTION
    Answers one question: does the port replace src/kdreams/altcontroller.c and
    src/backend/input/be_input_controller_mappings.c completely, and with nothing
    more than it needs?

    platform-boundary.md 8.2 decided to leave the altcontrol subsystem out of the
    port (it is ReflectionHLE's gamepad/touch mapping feature; Keen Dreams 1991
    has none) and left the exact stub surface to C12. Removing two TUs is only
    safe if nothing is left dangling, and "nothing" is not something to read off
    the source: a header declaration is not a reference, a prototype inside a .c
    file is not a definition, and a reference can sit in a header the .c includes
    where the .c's own text never says the name.

    So every claim here is measured by compiling the Keen Dreams target and
    asking the objects:

      * the target's TU list comes from upstream's own CMake source lists, read
        with the port's CMake variables (WIN32=OFF, LINUX=OFF, launcher=OFF);
      * every built TU is compiled on the target toolchain, and the union of
        their undefined symbols is intersected with the two replaced TUs'
        definitions.

    Checks:

      1. the export ledger is exact: the two replaced TUs define 39 globals, the
         port's stub defines the 32 of those that a built TU asks for, and
         defines nothing else
      2. the build set is re-derived from upstream's CMake lists, and the TUs the
         port removes agree with the exclusions the other gates use -- one fact,
         read by two questions
      3. completeness, in both directions: every symbol recorded as required is
         undefined in at least one built object, and no symbol recorded as
         dropped is undefined in any
      4. the stub compiles on the target, its diagnostic multiset equals the
         frozen one, -Werror=implicit-function-declaration changes nothing, and
         its section sizes equal the recorded ones
      5. the host test passes, and the symbol list the compiled stub reports
         agrees with the recorded list -- two independent derivations of one fact
      6. the replaced TUs and their headers are unreachable from the port
      7. the cut pays for itself: the replaced TUs' writable data and the stub's
         are both measured, and the net RAM saving is the recorded one

    Checks 3-5 and 7 need compilers. Without -TargetGcc / -HostGcc they are
    SKIPPED loudly and named in the summary line rather than silently dropped.

.PARAMETER SelfTest
    Hand off to the companion script Test-AltControlStubSelfTest.ps1, which
    mirrors the real sources, confirms the clean copy passes, then injects one
    defect per check and requires exactly that check to fire. The companion is
    runnable on its own too; the switch exists so the gate and its negative-path
    test are found in the same place.
#>
[CmdletBinding()]
param(
    [string]$Workspace = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$UpstreamRoot,
    [string]$TargetGcc,
    [string]$HostGcc,
    [string]$ReportPath,
    [switch]$SelfTest,
    [string]$RunId = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# -SelfTest hands off before any of this gate's own defaults are applied: the
# companion has its own report-path default, and letting this gate's default win
# would silently redirect the self-test's report over the gate's own.
if ($SelfTest) {
    $companion = Join-Path $PSScriptRoot 'Test-AltControlStubSelfTest.ps1'
    $forward = @{ Workspace = $Workspace }
    if ($TargetGcc)  { $forward['TargetGcc']  = $TargetGcc }
    if ($HostGcc)    { $forward['HostGcc']    = $HostGcc }
    if ($RunId)      { $forward['RunId']      = $RunId }
    if ($ReportPath) { $forward['ReportPath'] = $ReportPath }
    & $companion @forward
    exit $LASTEXITCODE
}

if (-not $UpstreamRoot) { $UpstreamRoot = Join-Path $Workspace '_deps\reflectionhle-upstream' }
if (-not $TargetGcc) {
    $TargetGcc = 'F:\Download\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe'
}
if (-not $HostGcc) {
    $HostGcc = 'E:\LVGL\x86_64-14.2.0-release-win32-seh-msvcrt-rt_v12-rev2\mingw64\bin\gcc.exe'
}
if (-not $ReportPath) { $ReportPath = Join-Path $Workspace '.emv2\logs\s20-altcontrol.txt' }

$script:Lines = New-Object System.Collections.Generic.List[string]
$script:Problems = New-Object System.Collections.Generic.List[string]
$script:Skipped = New-Object System.Collections.Generic.List[string]
$script:CheckBase = 0
# Filled by check 2 and consumed by check 3. Explicitly initialised: under
# Set-StrictMode, testing an unset variable is an error, and a check that
# quietly reads $null is worse than one that says so.
$script:BuiltTus = $null

function Emit([string]$text) { [void]$script:Lines.Add($text) }
function Fail([string]$text) { [void]$script:Problems.Add($text); Emit ("  FAIL  " + $text) }
function Skip([string]$text) { [void]$script:Skipped.Add($text); Emit ("  SKIP  " + $text) }
function Note([string]$text) { Emit ("  ok    " + $text) }

# Per-check problem accounting: a check's success line must depend on whether
# THAT check failed, not on the running total, or one early failure silences
# every later check and a silent check is indistinguishable from a skipped one.
function Begin-Check([string]$title) {
    Emit $title
    $script:CheckBase = $script:Problems.Count
}

function Check-Clean { return ($script:Problems.Count -eq $script:CheckBase) }

function Flush-Report {
    $dir = Split-Path -Parent $ReportPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $script:Lines | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

# Invoke a native tool without letting its stderr become a terminating error.
# With $ErrorActionPreference = 'Stop', PowerShell 5.1 escalates native stderr to
# a terminating error, which would abort this script on the first TU that is
# merely *expected* to emit a warning. This gate reads diagnostics, so every
# compile goes through here.
function Invoke-Native([string]$Exe, [string[]]$Arguments) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # -Width matters: Out-String defaults to the console width and FOLDS long
        # lines, which cuts compiler diagnostics mid-path.
        $out = (& $Exe @Arguments 2>&1 | Out-String -Width 4096)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Rc = $rc; Out = $out }
}

# Warning classes as a sorted multiset. Message wording is excluded on purpose:
# only the class and its count, so a compiler-version wording change is not
# mistaken for an upstream change.
function Get-DiagnosticSet([string]$text) {
    $counts = @{}
    foreach ($m in [regex]::Matches($text, '\[(-W[A-Za-z0-9-]+)\]')) {
        $k = $m.Groups[1].Value
        if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
    }
    $items = @($counts.Keys | ForEach-Object { '{0}={1}' -f $_, $counts[$_] })
    return @($items | Sort-Object)
}

function Get-StringHash([string]$text) {
    $sha = [System.Security.Cryptography.SHA1]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($text)
        return ([BitConverter]::ToString($sha.ComputeHash($bytes)) -replace '-', '').Substring(0, 10).ToLower()
    } finally {
        $sha.Dispose()
    }
}

# Two views of one file, because "the port names the replaced TU" and "the port
# can reach the replaced TU" are different questions:
#
#   Remove-Comments               -> still sees string literals, so an #include
#                                    directive survives
#   Remove-CommentsAndStrings     -> what is left is code, so a symbol named
#                                    here is a symbol the linker will ask for
#
# Both replacements blank the match character-for-character instead of deleting
# it, so a multi-line block comment cannot glue two definitions onto one line.
$script:BlankNonNewlines = [System.Text.RegularExpressions.MatchEvaluator]{
    param($m) ($m.Value -replace '[^\r\n]', ' ')
}

function Remove-Comments([string]$text) {
    $t = [regex]::Replace($text, '/\*.*?\*/', $script:BlankNonNewlines, 'Singleline')
    return [regex]::Replace($t, '//[^\r\n]*', $script:BlankNonNewlines)
}

function Remove-CommentsAndStrings([string]$text) {
    $t = Remove-Comments $text
    return [regex]::Replace($t, '"(?:[^"\\]|\\.)*"', $script:BlankNonNewlines)
}

# Every src/... .c path named inside a list(APPEND ...) block, regardless of the
# condition guarding it. The conditions are then applied by name against the
# facts, because interpreting CMake is not this gate's job -- and an earlier
# attempt at a conditional evaluator got the single-line form of list(APPEND)
# wrong in a way that produced a plausible but wrong build set.
#
# The one trap that must not be repeated: advance the line index EXACTLY ONCE per
# iteration. Incrementing before the continuation loop and again after it
# double-advances on the single-line form, so each such line swallows the next
# one -- in src/CMakeLists.txt:56 that ate the block's own `endif ()` and
# desynchronised every conditional after it.
function Get-CmakeListedPaths([string]$path) {
    $lines = @(Get-Content -LiteralPath $path)
    $found = New-Object System.Collections.Generic.List[string]
    $i = 0
    while ($i -lt $lines.Count) {
        $s = $lines[$i].Trim()
        $m = [regex]::Match($s, '^list\(\s*APPEND\s+([A-Za-z0-9_]+)\b(.*)$')
        if (-not $m.Success) { $i++; continue }

        $rest = $m.Groups[2].Value.Trim()
        $block = New-Object System.Collections.Generic.List[string]
        if ($rest -ne '') { [void]$block.Add($rest) }
        while (-not $rest.EndsWith(')')) {
            $i++
            if ($i -ge $lines.Count) { break }
            $rest = $lines[$i].Trim()
            [void]$block.Add($rest)
        }
        foreach ($piece in $block) {
            foreach ($mm in [regex]::Matches($piece, '\bsrc/[A-Za-z0-9_./-]+\.c\b')) {
                [void]$found.Add($mm.Value)
            }
        }
        $i++
    }
    return @($found)
}

# ---------------------------------------------------------------------------

$facts = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1')
$ac = $facts.AltControlStub

$portDir = Join-Path $Workspace 'refkeen\platform\retropicoes'
$portTu = Join-Path $portDir ($ac.PortTus[0])
$portTest = Join-Path (Join-Path $Workspace 'refkeen\tests') $ac.PortTest

$upSrc = Join-Path $UpstreamRoot 'src'
$gccDir = Split-Path -Parent $TargetGcc
$nmExe = Join-Path $gccDir 'arm-none-eabi-nm.exe'
$sizeExe = Join-Path $gccDir 'arm-none-eabi-size.exe'

$incs = @(
    '-I', (Join-Path $Workspace 'refkeen\host\shim'),
    '-I', (Join-Path $Workspace 'refkeen\host\include'),
    '-I', (Join-Path $Workspace 'compat'),
    '-I', $portDir,
    '-I', $upSrc,
    '-I', (Join-Path $upSrc 'crc32'),
    '-I', (Join-Path $upSrc 'depklite'),
    '-I', (Join-Path $upSrc 'unlzexe'),
    '-I', (Join-Path $upSrc 'kdreams'),
    '-I', (Join-Path $upSrc 'kdreams\lscr')
)
$defs = @(
    '-DREFKEEN_VER_KDREAMS=1',
    '-DREFKEEN_HAS_VER_KDREAMS=1',
    '-DREFKEEN_PLATFORM_PICO=1',
    '-DREFKEEN_SDL_SHIM_HOST=1',
    '-include', 'string.h'
)
# -mcpu matters and is not decoration: the shim's SDL_CPUPauseInstruction emits
# `yield`, which the default -mcpu=arm7tdmi cannot assemble in ARM mode, so any
# TU reaching it through be_st_sdl_private.h would report a failure the target
# does not have. Measured: be_timing.c compiles cleanly with these flags.
$arch = @('-mcpu=cortex-m33', '-mthumb')
$cflags = @('-c', '-std=c99', '-fno-common', '-O0', '-fmessage-length=0')

# The object cache is keyed by the flag set AND the tree, so that changing a
# flag, or pointing at a mirrored tree, can never reuse an object built under the
# old conditions -- a stale object does not look stale, it just answers wrong.
$cacheKey = ((@($incs) + @($defs) + @($arch) + @($cflags)) -join '|') + '||' + $UpstreamRoot
$objRoot = Join-Path $Workspace ('_deps\_probe\altstub-' + (Get-StringHash $cacheKey))
if (-not (Test-Path -LiteralPath $objRoot)) {
    [void](New-Item -ItemType Directory -Path $objRoot -Force)
}

function Compile-Tu([string]$RelPath) {
    $obj = Join-Path $objRoot (($RelPath -replace '[^A-Za-z0-9]', '_') + '.o')
    if (Test-Path -LiteralPath $obj) { return [pscustomobject]@{ Rc = 0; Out = ''; Obj = $obj } }
    $r = Invoke-Native $TargetGcc (@($incs) + @($defs) + @($arch) + @($cflags) + @('-o', $obj, (Join-Path $UpstreamRoot $RelPath)))
    return [pscustomobject]@{ Rc = $r.Rc; Out = $r.Out; Obj = $obj }
}

function Get-DefinedSymbols([string]$ObjPath) {
    $r = Invoke-Native $nmExe @('--defined-only', '-g', $ObjPath)
    $names = New-Object System.Collections.Generic.List[string]
    foreach ($line in ($r.Out -split "`r?`n")) {
        $parts = @($line.Trim() -split '\s+' | Where-Object { $_ -ne '' })
        if ($parts.Count -ge 2) {
            $n = $parts[$parts.Count - 1]
            if ($n -notlike '*:*') { [void]$names.Add($n) }
        }
    }
    return @($names)
}

function Get-UndefinedSymbols([string]$ObjPath) {
    $r = Invoke-Native $nmExe @('-u', $ObjPath)
    $names = New-Object System.Collections.Generic.List[string]
    foreach ($line in ($r.Out -split "`r?`n")) {
        $parts = @($line.Trim() -split '\s+' | Where-Object { $_ -ne '' })
        if ($parts.Count -ge 2) { [void]$names.Add($parts[$parts.Count - 1]) }
    }
    return @($names)
}

function Get-SectionSizes([string]$ObjPath) {
    $sizes = @{}
    $r = Invoke-Native $sizeExe @('-A', $ObjPath)
    foreach ($line in ($r.Out -split "`r?`n")) {
        $m = [regex]::Match($line, '^\s*(\.[A-Za-z0-9._]+)\s+(\d+)\s')
        if ($m.Success) { $sizes[$m.Groups[1].Value] = [int]$m.Groups[2].Value }
    }
    return $sizes
}

Emit "S20 ALTCONTROL STUB GATE"
Emit ("workspace     : {0}" -f $Workspace)
Emit ("upstream      : {0}" -f $UpstreamRoot)
Emit ("target gcc    : {0}" -f $TargetGcc)
Emit ("host gcc      : {0}" -f $HostGcc)
if ($RunId) { Emit ("run id        : {0}" -f $RunId) }
Emit ""

$targetAvailable = Test-Path -LiteralPath $TargetGcc -PathType Leaf
$hostAvailable = Test-Path -LiteralPath $HostGcc -PathType Leaf

# --- Check 1: the export ledger is exact ------------------------------------
Begin-Check "Check 1  the two replaced TUs define 39 globals; the port's stub defines the 32 that are used"
$required = @($ac.RequiredSymbols | Sort-Object -Unique)
$dropped = @($ac.DroppedSymbols.Keys | Sort-Object -Unique)
$expectedAll = @(@($required) + @($dropped) | Sort-Object -Unique)

if (-not $targetAvailable) {
    Skip "check 1 (export ledger): -TargetGcc not found"
} else {
    $measuredAll = New-Object System.Collections.Generic.List[string]
    $perTuCount = @{}
    foreach ($tu in $ac.ExcludedTus) {
        $c = Compile-Tu $tu
        if ($c.Rc -ne 0) {
            Fail ("check 1: the replaced TU does not compile: {0}" -f $tu)
            continue
        }
        $names = @(Get-DefinedSymbols $c.Obj)
        $perTuCount[$tu] = $names.Count
        foreach ($n in $names) { [void]$measuredAll.Add($n) }
    }
    $measuredText = ((@($measuredAll) | Sort-Object -Unique) -join '|')
    $expectedText = ($expectedAll -join '|')
    if ($measuredText -cne $expectedText) {
        Fail "check 1: the replaced TUs' exported set moved"
        Emit ("        recorded: " + $expectedText)
        Emit ("        measured: " + $measuredText)
    } elseif ((@($measuredAll) | Sort-Object -Unique).Count -ne $ac.ExcludedDefinitionCount) {
        Fail ("check 1: distinct definitions {0}, recorded {1}" -f (@($measuredAll) | Sort-Object -Unique).Count, $ac.ExcludedDefinitionCount)
    } else {
        foreach ($tu in ($perTuCount.Keys | Sort-Object)) {
            Emit ("        {0} -> {1} global definition(s)" -f $tu, $perTuCount[$tu])
        }
    }

    # The stub's half. It must define every required symbol and nothing else: an
    # extra definition would mean the stub is supplying something the ledger does
    # not account for, which is how a "temporary" shim becomes permanent.
    if (-not (Test-Path -LiteralPath $portTu -PathType Leaf)) {
        Fail ("check 1: the stub TU is missing: {0}" -f $ac.PortTus[0])
    } else {
        $stubObj = Join-Path $objRoot ('port_' + $ac.PortTus[0].Replace('.c', '') + '.o')
        $r = Invoke-Native $TargetGcc (@($incs) + @($defs) + @($arch) + @($cflags) + @('-o', $stubObj, $portTu))
        if ($r.Rc -ne 0) {
            Fail ("check 1: the stub does not compile on the target (rc={0})" -f $r.Rc)
        } else {
            $stubDefs = @(Get-DefinedSymbols $stubObj | Sort-Object -Unique)
            $stubText = ($stubDefs -join '|')
            $reqText = ($required -join '|')
            if ($stubText -cne $reqText) {
                Fail "check 1: the stub's exported set is not the recorded required set"
                $extra = @($stubDefs | Where-Object { $required -cnotcontains $_ })
                $missing = @($required | Where-Object { $stubDefs -cnotcontains $_ })
                if ($extra.Count -gt 0) { Emit ("        extra   : " + ($extra -join ', ')) }
                if ($missing.Count -gt 0) { Emit ("        missing : " + ($missing -join ', ')) }
            } elseif ($stubDefs.Count -ne $ac.StubExportedCount) {
                Fail ("check 1: the stub exports {0} symbols, recorded {1} -- one of the two is wrong" -f $stubDefs.Count, $ac.StubExportedCount)
            } else {
                Note ("the stub exports exactly the {0} recorded symbol(s), no more and no fewer" -f $stubDefs.Count)
            }
        }
    }

    # And the per-symbol ledger the stub is written against must be consistent:
    # a name in both lists is a name nobody would know how to classify.
    $overlap = @($required | Where-Object { $dropped -ccontains $_ })
    if ($overlap.Count -gt 0) {
        Fail ("check 1: symbol(s) recorded as both required and dropped: " + ($overlap -join ', '))
    } elseif ($expectedAll.Count -ne $ac.ExcludedDefinitionCount) {
        Fail ("check 1: required {0} + dropped {1} = {2}, recorded total {3}" -f `
              $required.Count, $dropped.Count, $expectedAll.Count, $ac.ExcludedDefinitionCount)
    }
}
Emit ""

# --- Check 2: the build set, from upstream's own CMake lists ----------------
Begin-Check "Check 2  the build set is re-derived from upstream's CMake lists and agrees with the other gates"
$listed = New-Object System.Collections.Generic.List[string]
foreach ($rel in $ac.SourceLists) {
    $p = Join-Path $UpstreamRoot $rel
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
        Fail ("check 2: cannot read {0}" -f $rel)
        continue
    }
    foreach ($x in (Get-CmakeListedPaths $p)) { [void]$listed.Add($x) }
}
$listedUnique = @($listed | Sort-Object -Unique)
if ($listedUnique.Count -eq 0) {
    Fail "check 2: parsed zero source paths out of upstream's CMake lists -- the parser is wrong, not the tree"
} else {
    $arms = @($ac.ConditionalArms | Sort-Object -Unique)
    $target = @($listedUnique | Where-Object { $arms -cnotcontains $_ })
    $removed = @($ac.PortRemovedTus.Keys | Sort-Object -Unique)
    $built = @($target | Where-Object { $removed -cnotcontains $_ })

    if ($target.Count -ne $ac.TargetTuCount) {
        Fail ("check 2: the target has {0} TUs, recorded {1}" -f $target.Count, $ac.TargetTuCount)
    }
    if ($built.Count -ne $ac.BuildTuCount) {
        Fail ("check 2: the build has {0} TUs, recorded {1}" -f $built.Count, $ac.BuildTuCount)
    }
    # A conditional arm that upstream no longer lists means the arm list is stale;
    # a removed TU that is not in the target means it was never there to remove.
    foreach ($a in $arms) {
        if ($listedUnique -cnotcontains $a) {
            Fail ("check 2: recorded conditional arm is not listed by upstream: {0}" -f $a)
        }
    }
    foreach ($r in $removed) {
        if ($target -cnotcontains $r) {
            Fail ("check 2: TU recorded as removed by the port is not in the target: {0}" -f $r)
        }
    }

    # The "one fact" half: the port's removals must be exactly the union of the
    # exclusions the other gates already use, so the same question cannot get two
    # different answers in two scripts.
    $expectedRemoved = New-Object System.Collections.Generic.List[string]
    foreach ($t in $facts.PlatformTus) { [void]$expectedRemoved.Add($t) }
    [void]$expectedRemoved.Add($facts.EventSource.ReplacedTu)
    foreach ($n in $facts.FilesystemSeam.ExcludedTus.Keys) {
        [void]$expectedRemoved.Add('src/backend/filesystem/' + $n)
    }
    foreach ($t in $facts.PortExcludedTus.Keys) { [void]$expectedRemoved.Add($t) }
    [void]$expectedRemoved.Add('src/actual_main.c')
    $expectedRemoved = @($expectedRemoved | Sort-Object -Unique | Where-Object { $target -ccontains $_ })
    $gotText = ($removed -join '|')
    $expText = ($expectedRemoved -join '|')
    if ($gotText -cne $expText) {
        Fail "check 2: the port's removal list disagrees with the exclusions the other gates use"
        Emit ("        this gate : " + $gotText)
        Emit ("        other gates: " + $expText)
    }
    if (Check-Clean) {
        Note ("upstream lists {0} source paths; {1} conditional arm(s) do not apply, {2} are removed by the port, {3} are built" -f `
              $listedUnique.Count, $arms.Count, $removed.Count, $built.Count)
    }
    $script:BuiltTus = $built
}
Emit ""

# --- Check 3: completeness, both directions, measured over the whole build ---
Begin-Check "Check 3  every recorded-required symbol is undefined in a built object, and no dropped symbol is"
if (-not $targetAvailable) {
    Skip "check 3 (completeness): -TargetGcc not found"
} elseif ($null -eq $script:BuiltTus) {
    Skip "check 3 (completeness): the build set could not be derived"
} else {
    $watch = @(@($required) + @($dropped) | Sort-Object -Unique)
    $undefinedIn = @{}
    $compileFailures = New-Object System.Collections.Generic.List[string]
    foreach ($rel in $script:BuiltTus) {
        $c = Compile-Tu $rel
        if ($c.Rc -ne 0) {
            [void]$compileFailures.Add($rel)
            continue
        }
        foreach ($n in (Get-UndefinedSymbols $c.Obj)) {
            if ($watch -ccontains $n) {
                if (-not $undefinedIn.ContainsKey($n)) {
                    $undefinedIn[$n] = New-Object System.Collections.Generic.List[string]
                }
                [void]$undefinedIn[$n].Add($rel)
            }
        }
    }
    $compiled = $script:BuiltTus.Count - $compileFailures.Count
    if ($compileFailures.Count -gt 0) {
        # Do not report a pass on a partial set: an uncompiled TU could be the
        # only referencer of a symbol.
        Fail ("check 3: {0} built TU(s) did not compile, so the build set is incomplete" -f $compileFailures.Count)
        foreach ($f in $compileFailures) { Emit ("        " + $f) }
    } else {
        $neverUsed = @($required | Where-Object { -not $undefinedIn.ContainsKey($_) })
        $usedAnyway = @($dropped | Where-Object { $undefinedIn.ContainsKey($_) })

        if ($neverUsed.Count -gt 0) {
            Fail ("check 3: recorded as required, but no built object asks for it: " + ($neverUsed -join ', '))
        }
        if ($usedAnyway.Count -gt 0) {
            Fail ("check 3: recorded as dropped, but a built object does ask for it: " + ($usedAnyway -join ', '))
            foreach ($n in $usedAnyway) {
                # Precomputed: with the -join left inline, the binary -join binds
                # looser than the rest of the expression and the report prints the
                # wrong thing (the trap recorded in MEMORY.md from S20-B).
                $who = (($undefinedIn[$n] | Sort-Object -Unique) -join ', ')
                Emit ("        {0} <- {1}" -f $n, $who)
            }
        }
        if (Check-Clean) {
            Note ("{0}/{0} built TU(s) compiled on the target; all {1} required symbol(s) are undefined in at least one object and none of the {2} dropped one(s) is" -f `
                  $compiled, $required.Count, $dropped.Count)
            # A sample of the referencer sets, so the report shows who needs what.
            foreach ($n in ($required | Where-Object { $undefinedIn.ContainsKey($_) } | Sort-Object)) {
                $who = (($undefinedIn[$n] | Sort-Object -Unique) -join ', ')
                Emit ("        {0} <- {1}" -f $n, $who)
            }
        }
    }
}
Emit ""

# --- Check 4: the stub compiles on the target, and its sizes are as recorded -
Begin-Check "Check 4  the stub compiles on the target; diagnostics and section sizes match the record"
if (-not $targetAvailable) {
    Skip "check 4 (target compile): -TargetGcc not found"
} elseif (-not (Test-Path -LiteralPath $portTu -PathType Leaf)) {
    Fail ("check 4: the stub TU is missing: {0}" -f $ac.PortTus[0])
} else {
    $probeObj = Join-Path $objRoot ('check4_' + $ac.PortTus[0].Replace('.c', '') + '.o')
    $base = Invoke-Native $TargetGcc (@($incs) + @($defs) + @($arch) + @($cflags) + @('-Wall', '-Wextra', '-o', $probeObj, $portTu))
    if ($base.Rc -ne 0) {
        Fail "check 4: the stub does not compile on the target"
        Emit ("        " + (($base.Out -split "`r?`n" | Where-Object { $_ -match ':\d+:\d+: (error|fatal error)' } | Select-Object -First 4) -join "`n        "))
    } else {
        $got = @(Get-DiagnosticSet $base.Out)
        $frozen = @($ac.FrozenDiagnostics[$ac.PortTus[0]] | Sort-Object)
        $gotText = ($got -join '|')
        $expText = ($frozen -join '|')
        if ($gotText -cne $expText) {
            Fail "check 4: the stub's diagnostic set changed"
            Emit ("        frozen  : " + $(if ($expText) { $expText } else { '(none)' }))
            Emit ("        measured: " + $(if ($gotText) { $gotText } else { '(none)' }))
        }
        # The port never tolerates this class, upstream or not.
        $strictObj = Join-Path $objRoot ('check4_' + $ac.PortTus[0].Replace('.c', '') + '-strict.o')
        $strict = Invoke-Native $TargetGcc (@($incs) + @($defs) + @($arch) + @($cflags) + @('-Wall', '-Wextra', '-Werror=implicit-function-declaration', '-o', $strictObj, $portTu))
        if ($strict.Rc -ne 0) {
            Fail "check 4: the stub has an implicit function declaration"
        }

        $sizes = Get-SectionSizes $probeObj
        foreach ($pair in @(@('.text', 'StubTextBytes'), @('.data', 'StubDataBytes'), @('.bss', 'StubBssBytes'))) {
            $name = $pair[0]
            $recorded = $ac.($pair[1])
            $gotSize = if ($sizes.ContainsKey($name)) { $sizes[$name] } else { 0 }
            if ($gotSize -ne $recorded) {
                Fail ("check 4: {0} is {1} B, recorded {2} B -- the mapping type or the stub changed" -f $name, $gotSize, $recorded)
            }
        }
        if (Check-Clean) {
            Note ("compiles on the target; diagnostics match the frozen set ({0})" -f $(if ($expText) { $expText } else { 'none' }))
            Note ("-Werror=implicit-function-declaration changes nothing")
            Note ("section sizes: .text={0} .data={1} .bss={2} (as recorded)" -f `
                  $sizes['.text'], $sizes['.data'], $sizes['.bss'])
        }
    }
}
Emit ""

# --- Check 5: the compiled stub's own symbol list vs the record -------------
Begin-Check "Check 5  the host test passes and the symbol list it reports equals the recorded one"
if (-not $hostAvailable) {
    Skip "check 5 (host test): -HostGcc not found"
} elseif (-not (Test-Path -LiteralPath $portTest -PathType Leaf)) {
    Fail ("check 5: the host test is missing: {0}" -f $ac.PortTest)
} else {
    $outDir = Join-Path $Workspace '_deps\_probe'
    if (-not (Test-Path -LiteralPath $outDir)) { [void](New-Item -ItemType Directory -Path $outDir -Force) }
    $exe = Join-Path $outDir ('s20c-altcontrol-stub' + $(if ($RunId) { '-' + $RunId } else { '' }) + '.exe')
    $hostIncs = @(
        '-I', (Join-Path $Workspace 'refkeen\host\shim'),
        '-I', (Join-Path $Workspace 'refkeen\host\include'),
        '-I', $portDir,
        '-I', $upSrc
    )
    # -Werror is deliberately absent: the stub includes be_st.h, which reaches
    # upstream's be_cross.h and its unused static. That is upstream's diagnostic,
    # and the same policy as every other seam in this port.
    $build = Invoke-Native $HostGcc (@('-std=c99', '-O1', '-Wall', '-Wextra') + @($defs) + $hostIncs + @('-o', $exe, $portTest, $portTu))
    if ($build.Rc -ne 0) {
        Fail ("check 5: the host test did not compile (rc={0})" -f $build.Rc)
        Emit ("        " + (($build.Out -split "`r?`n" | Where-Object { $_ -match 'error' } | Select-Object -First 4) -join "`n        "))
    } else {
        $hostDiag = @(Get-DiagnosticSet $build.Out)
        $hostFrozen = @($ac.HostDiagnostics | Sort-Object)
        $hGot = ($hostDiag -join '|')
        $hExp = ($hostFrozen -join '|')
        if ($hGot -cne $hExp) {
            Fail "check 5: the host build's diagnostic set changed"
            Emit ("        frozen  : " + $(if ($hExp) { $hExp } else { '(none)' }))
            Emit ("        measured: " + $(if ($hGot) { $hGot } else { '(none)' }))
        }
        $run = Invoke-Native $exe @()
        if ($run.Rc -ne 0) {
            Fail ("check 5: the host test exited {0}" -f $run.Rc)
        } elseif (-not $run.Out.Contains('REFKEEN ALTCONTROL STUB: PASS')) {
            Fail "check 5: the host test did not print its PASS marker"
        } else {
            $compiledSymbols = New-Object System.Collections.Generic.List[string]
            foreach ($m in [regex]::Matches($run.Out, 'CONTRACT\s+symbol=([A-Za-z_][A-Za-z0-9_]*)')) {
                [void]$compiledSymbols.Add($m.Groups[1].Value)
            }
            $declared = @{}
            $mDecl = [regex]::Match($run.Out, 'CONTRACT\s+symbols=(\d+)')
            if ($mDecl.Success) { $declared['count'] = [int]$mDecl.Groups[1].Value }

            if ($compiledSymbols.Count -eq 0) {
                Fail "check 5: the host test printed no per-symbol CONTRACT lines"
            } else {
                $cText = ((@($compiledSymbols) | Sort-Object -Unique) -join '|')
                $rText = ($required -join '|')
                if ($cText -cne $rText) {
                    Fail "check 5: the compiled stub's symbol list disagrees with the record"
                    Emit ("        recorded: " + $rText)
                    Emit ("        compiled: " + $cText)
                }
                if ($declared.ContainsKey('count') -and $declared['count'] -ne $required.Count) {
                    Fail ("check 5: the test declares {0} symbols, the record has {1}" -f $declared['count'], $required.Count)
                }
                if (Check-Clean) {
                    Note ("the host test passes and its own {0} symbol(s) match the record, derived independently" -f $compiledSymbols.Count)
                }
            }
        }
    }
}
Emit ""

# --- Check 6: the replaced TUs are unreachable from the port ----------------
Begin-Check "Check 6  the replaced TUs and their headers are unreachable from the port"
$staleIncludes = New-Object System.Collections.Generic.List[string]
$staleCode = New-Object System.Collections.Generic.List[string]
$proseMentions = 0
foreach ($tu in $ac.ExcludedTus) {
    # \b before the leaf stops "altcontroller.c" from also matching
    # "bmenace/altcontroller.c"-style neighbours, which run past a word boundary.
    $leaf = '(?<![A-Za-z0-9_])' + [regex]::Escape((Split-Path -Leaf $tu))
    foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File |
                    Where-Object { $_.Extension -in @('.c', '.h', '.cpp', '.txt') })) {
        $raw = Get-Content -LiteralPath $f.FullName -Raw
        if ([regex]::IsMatch($raw, '(?m)^[ \t]*#[ \t]*include\b.*' + $leaf)) {
            [void]$staleIncludes.Add($f.Name)
        }
        $code = Remove-CommentsAndStrings $raw
        if ([regex]::IsMatch($code, $leaf)) { [void]$staleCode.Add($f.Name) }
        $noComments = Remove-Comments $raw
        $proseMentions += ([regex]::Matches($raw, $leaf)).Count -
                          ([regex]::Matches($noComments, $leaf)).Count
    }
}
if ($staleIncludes.Count -gt 0) {
    Fail ("check 6: the port includes a replaced TU's header: " + ((@($staleIncludes) | Sort-Object -Unique) -join ', '))
}
if ($staleCode.Count -gt 0) {
    Fail ("check 6: the port names a replaced TU in code: " + ((@($staleCode) | Sort-Object -Unique) -join ', '))
}
if (Check-Clean) {
    Note ("neither replaced TU is included or named in port code ({0} prose mention(s), which are documentation)" -f $proseMentions)
}
Emit ""

# --- Check 7: the cut pays for itself --------------------------------------
Begin-Check "Check 7  the cut pays for itself: what the stub costs against what the two TUs cost"
if (-not $targetAvailable) {
    Skip "check 7 (RAM ledger): -TargetGcc not found"
} else {
    $removedRam = 0
    $removedFlash = 0
    foreach ($tu in $ac.ExcludedTus) {
        $c = Compile-Tu $tu
        if ($c.Rc -ne 0) { continue }
        $s = Get-SectionSizes $c.Obj
        # .data and .bss live in RAM; .data's initialisers also occupy flash, so
        # removing a .data object saves both.
        foreach ($k in @('.data', '.bss')) {
            if ($s.ContainsKey($k)) { $removedRam += $s[$k] }
        }
        foreach ($k in @('.text', '.rodata', '.data')) {
            if ($s.ContainsKey($k)) { $removedFlash += $s[$k] }
        }
    }
    $stubRam = $ac.StubDataBytes + $ac.StubBssBytes
    $stubFlash = $ac.StubTextBytes

    if ($removedRam -ne $ac.RemovedRamBytes) {
        Fail ("check 7: the replaced TUs' writable data measures {0} B, recorded {1} B" -f $removedRam, $ac.RemovedRamBytes)
    }
    if ($removedFlash -ne $ac.RemovedFlashBytes) {
        Fail ("check 7: the replaced TUs' flash footprint measures {0} B, recorded {1} B" -f $removedFlash, $ac.RemovedFlashBytes)
    }

    $net = $removedRam - $stubRam
    if ($removedRam -eq 0) {
        Fail "check 7: measured no writable data in the replaced TUs -- the measurement, not the tree, is wrong"
    } elseif ($net -le 0) {
        Fail ("check 7: the stub costs {0} B of RAM against {1} B removed -- the cut no longer pays" -f $stubRam, $removedRam)
    }
    if (Check-Clean) {
        Note ("RAM: {0} B removed, {1} B added by the stub, net saving {2} B (both sides measured)" -f $removedRam, $stubRam, $net)
        Note ("flash: about {0} B removed (text+rodata+.data image), {1} B added" -f $removedFlash, $stubFlash)
        Note ("the stub's {0} B belong on the S20-D PSRAM placement list" -f $stubRam)
    }
}
Emit ""

Emit "---------------------------------------------------------------------------"
if ($script:Problems.Count -eq 0) {
    $suffix = if ($script:Skipped.Count -gt 0) { " -- $($script:Skipped.Count) check(s) SKIPPED, see above" } else { '' }
    Emit ("ALTCONTROL STUB: PASS (7 checks){0}" -f $suffix)
    Flush-Report
    exit 0
}
Emit ("ALTCONTROL STUB: FAILED ({0} problem(s))" -f $script:Problems.Count)
Flush-Report
exit 1
