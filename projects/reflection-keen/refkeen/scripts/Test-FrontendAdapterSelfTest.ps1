<#
.SYNOPSIS
    Negative-path self test for Test-FrontendAdapterContract.ps1.

.DESCRIPTION
    A gate that cannot fail is worse than no gate. This proves the frontend gate
    fails, and fails for the RIGHT reason: each case injects exactly one defect
    and then asserts both that the intended check fires and that the other six
    stay quiet. The second half is what keeps the attribution honest -- without
    it, "check 4 failed" could just as easily be an artifact of the fixture.

    C0 is the clean baseline and must pass, or every later case is
    unattributable. The fixture mirrors the real sources rather than being a
    hand-written miniature: this project has been through five rounds where a
    fixture defect was reported by the gate in business language ("the adapter
    does not compile", "the shared surface moved") and cost a round to diagnose.

    Two of the lessons from those rounds are baked in here rather than written
    down. Every injection write RETRIES and then reads its own result back,
    because the first dry run of this file lost 13 of 25 injections to a
    transient "access denied" on files that were present and writable a moment
    later -- a fixture that fails at random is worse than one that fails loudly.
    And every mirror is checked for the existence and length of each file the
    injections and the gate touch BEFORE anything is injected, so a truncated
    copy is reported as a broken fixture instead of as a defect in the port.

    Cases:

      C0   nothing injected                                        -> PASS
      C1   the compiled adapter gains an export the record lacks    -> check 1
      C1b  the record's symbol spelling changes CASE only           -> checks 1 and 5
      C2a  the recorded .text figure drifts from the measurement    -> check 2
      C2b  the recorded diagnostic set drifts from the measurement  -> check 2
      C3a  the recorded shared surface drops a call                 -> check 3
      C3b  a port file calls a ROM browser function                 -> check 3
      C3c  the compiled adapter gains an undefined symbol           -> check 3
      C4a  the caller stops calling the hook                        -> check 4
      C4b  the caller stops reading the SETTINGS_RAN bit            -> check 4
      C4c  the hook header grows an include                         -> check 4
      C4d  a fourth file starts including the hook header           -> check 4
      C5a  the host test prints a symbol the adapter lacks          -> check 5
      C5b  the host test's CONTRACT scalar is forged                -> check 5
      C5c  the host test fails an assertion, no PASS marker, rc 0    -> check 5
      C5d  the host test prints PASS while an assertion failed      -> check 5
      C6a  the port stops naming the quit primitive                 -> check 6
      C6b  the defining TU is no longer in PortRemovedTus           -> check 6
      C6c  the defining TU stops defining the symbol                -> check 6
      C6d  the defining TU does not compile on the host             -> check 6
      C7a1 the port's own header drifts from the recorded interface -> check 7
      C7a2 a sibling port's header drifts from the same record      -> check 7
      C7b  a port file carries the legacy menu gesture              -> check 7
      C7c  a landed CMakeLists is missing a required token          -> check 7
      C7d  a landed CMakeLists links rom_browser.c                  -> check 7
      C7e  a landed CMakeLists satisfies the CMake contract         -> PASS, no skip

    NO CASE IS ALLOWED TO TRIP TWO CHECKS, with ONE deliberate exception, and
    the injections are chosen for it:

      * C1 and check 2 see the same compiled object, so C1 grows the adapter's
        EXPORT SET in a way that adds no bytes to any section check 2 sums: the
        injected symbol is placed in a section of its own (.probe_extra) rather
        than in .text or .bss. A plain added function would have moved check 2's
        .text ledger as well, and the case would have proven less than it looks
        like it proves.
      * the same coupling is why C2a, C2b, C3a and C6b inject into the RECORD
        (BoundaryFacts.psd1) instead of into the artifact where a text edit is
        enough. Any real edit to frontend.c moves .text, so an artifact-side
        injection for those checks would have to be measured byte-for-byte
        neutral, and there is no such edit that is also a realistic defect.
      * C3c is the artifact-side half of check 3, and it needs the same
        neutrality. It is guarded by #ifdef __arm__ so the host build (check 5,
        which links frontend.c and would otherwise fail to link an undefined
        symbol) never sees it.
      * C4c and C4d are different assertions inside check 4 -- the hook header's
        OWN includes versus its includer SET -- and they fail for different
        reasons, so one case would have proven only one of them. The includer
        set is the one that produced a real false red this round, when the
        record turned out to be incomplete rather than the port wrong.
      * C6a, C6b and C6c are the three questions check 6 asks (is the name in
        the port / is the definer a TU the port removes / is the definition
        really there), and C6d is check 6's other toolchain half.
      * C5c and C5d are the two halves of "the gate must not trust the test's own
        summary": no PASS marker at all, versus a PASS marker that lies. C5d also
        settled a question about the gate itself -- its FAIL-line scan lives
        inside the branch that requires the PASS marker, and with this test's
        structure (PASS printed only when nothing failed) that scan could not be
        reached by ANY injection until C5d existed.
      * C7a1 and C7a2 are the two derivations of one interface. C7a1 is also
        the case that found a real gap: before this round, check 7 compared the
        SIBLING headers against the record but only searched the port's own
        header for the NAME, so a port whose declaration had drifted would have
        compiled, linked and been called a pass. The gate was extended (last
        match wins, signature compared) and C7a1 is what keeps that extension
        from being deletable.

    C1b IS the deliberate exception, and it has to be a two-check case: it
    changes the CASE of one name in the record, and the record is what BOTH
    check 1 (against nm's reading) and check 5 (against the host test's own list)
    are compared with. A case-insensitive comparison in either place would let a
    renamed symbol through while both sides print identically -- this project has
    already been caught by exactly that, in the S20-B gate. Splitting it in two
    would have meant two injections of one fact and no honest attribution.

    DELIBERATELY NOT COVERED, and why:

      * a landed CMakeLists that links pico_shared/menu.cpp: same code path and
        same shape as C7d's rom_browser.c rule, so C7d already proves the rule
        can fire.
      * a missing sibling header file: it fails check 7 already, but its wording
        is the one place where the gate's attribution is weaker than it could be
        (one list holds both file paths and declaration names). Recorded as a
        follow-up item rather than tested, because testing it would be asserting
        a known-imprecise message.
      * the "no -TargetGcc / -HostGcc" SKIP branches: they are loud by
        construction, and C0's anchored PASS line is what proves no OTHER check
        silently skipped in the clean tree.

.PARAMETER RunId
    Suffix for the scratch trees. Nothing is ever deleted and re-created: the
    sandbox redirects recursive deletes to a fail-closed trash, so every run gets
    brand new directories. Each of the twenty-six cases copies a few MB of
    upstream sources, so a re-run needs its own RunId by construction.

.PARAMETER VerifyInjections
    Fixture check only: build every mirror and land every injection, then stop
    without compiling or running the gate. Use it after changing what the
    injections target -- a stale anchor is otherwise discovered twenty-six
    cases into a twenty-six-case run, which is how this switch came to exist.
    Its verdict line is deliberately not shaped like the self-test's: it says
    nothing about whether the gate can fail.
#>
[CmdletBinding()]
param(
    [string]$Workspace = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$TargetGcc = 'F:\Download\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe',
    [string]$HostGcc = 'E:\LVGL\x86_64-14.2.0-release-win32-seh-msvcrt-rt_v12-rev2\mingw64\bin\gcc.exe',
    [string]$RunId = '',
    [string]$ReportPath,
    [switch]$VerifyInjections
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Fixture-only mode. It builds every mirror and performs every injection, then
# stops -- no compiler, no gate. It exists because the failure it catches is the
# fixture's own: a stale anchor throws, and in a twenty-six-case run a throw
# costs the whole run and hides which case was wrong. Run it once after any
# change to what the injections target.
#
# It is NOT a cheaper way to run this self test, and the verdict line says so:
# "INJECTION CHECK: OK" is a statement about the fixture, not about the gate.
$script:VerifyOnly = [bool]$VerifyInjections

if (-not $RunId) { $RunId = ('{0:yyyyMMddHHmmss}' -f (Get-Date)) }
if (-not $ReportPath) {
    $ReportPath = Join-Path $Workspace ('.emv2\logs\s20-frontend-selftest-{0}.txt' -f $RunId)
}

$script:Out = New-Object System.Collections.Generic.List[string]
$script:Cases = 0
$script:Bad = 0

function Say([string]$text) { [void]$script:Out.Add($text) }

function Flush-Out {
    $dir = Split-Path -Parent $ReportPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $script:Out | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

$realUpstream = Join-Path $Workspace '_deps\reflectionhle-upstream'
$gateName = 'Test-FrontendAdapterContract.ps1'
$gateReal = Join-Path $PSScriptRoot $gateName
$factsPath = Join-Path $PSScriptRoot 'BoundaryFacts.psd1'

# Read the gate's own record rather than restating any of it here: the fixture
# has to supply exactly the trees the gate's checks reach for, and a hard-coded
# copy of those paths would drift silently the first time the record changes.
$facts = Import-PowerShellDataFile -LiteralPath $factsPath
$fe = $facts.Frontend
$sdlRel = $fe.Sdl3HostInclude
$sdlTop = @($sdlRel -split '[\\/]')[1]
$realSdl = Join-Path $Workspace ('_deps\' + $sdlTop)

# The mirror is the whole upstream src/ tree minus the three directories that
# belong to games this port does not build, plus rsrc/ (a SIBLING of src/, not a
# child: the XPM images are included as "../rsrc/x.xpm"). That is deliberately
# more than the TUs under test need -- rsrc/ was learned the hard way in the
# altcontrol fixture, where leaving it out made two real TUs fail to compile and
# the gate reported it as "the replaced TU does not compile". id91_11 (3.3 MB),
# wolf3d (2.0 MB) and android-project are skipped so that twenty-six cases do
# not each copy 5.5 MB more than they need. Check 6's definition index is a scan
# over src/, and that skip is safe for a measured reason rather than a hopeful
# one: with the full tree the real run finds exactly ONE definer of the quit
# primitive, so no definition lives in a skipped tree -- otherwise the real run
# would have reported a count of two.
$mirrorSkipDirs = @('id91_11', 'wolf3d', 'android-project')

# Fixture integrity: exactly the files the cases inject into and the gate reads
# for its contract. Checked (existence + length) before every injection, because
# a truncated or missing copy makes the gate report a defect in the PORT -- this
# project's most expensive fixture mistake, five rounds running. Length is not a
# hash: it catches the modes a copy actually fails in (missing, truncated) for
# the price of a stat call, and a same-length corruption of a fresh copy is not
# one of them.
$script:MirrorMustHave = @(
    'refkeen\platform\retropicoes\frontend.c',
    'refkeen\platform\retropicoes\frontend.h',
    'refkeen\platform\retropicoes\frontend_hook.h',
    'refkeen\platform\retropicoes\be_events_port.c',
    'refkeen\tests\test_frontend_adapter.c',
    'refkeen\scripts\BoundaryFacts.psd1',
    'xrick\platform\retropicoes\runtime.h',
    'digger\platform\retropicoes\runtime.h',
    'common\frontend\frontend_shell.c',
    '_deps\reflectionhle-upstream\src\backend\startup\be_startup_sdl.c'
)

function New-Mirror([string]$Tag) {
    $root = Join-Path $Workspace ("_deps\frontend-self-{0}-{1}" -f $RunId, $Tag)
    $upDst = Join-Path $root '_deps\reflectionhle-upstream'
    $srcDst = Join-Path $upDst 'src'
    $upSrc = Join-Path $realUpstream 'src'
    New-Item -ItemType Directory -Force -Path $srcDst | Out-Null
    # Loose files first (headers, be_cross*.c, CMakeLists.txt), then every
    # subdirectory but the skipped ones.
    foreach ($f in (Get-ChildItem -LiteralPath $upSrc -File)) {
        Copy-Item -LiteralPath $f.FullName -Destination $srcDst -Force
    }
    foreach ($d in (Get-ChildItem -LiteralPath $upSrc -Directory)) {
        if ($mirrorSkipDirs -contains $d.Name) { continue }
        Copy-Item -LiteralPath $d.FullName -Destination $srcDst -Recurse -Force
    }
    $rsrc = Join-Path $realUpstream 'rsrc'
    if (Test-Path -LiteralPath $rsrc -PathType Container) {
        Copy-Item -LiteralPath $rsrc -Destination $upDst -Recurse -Force
    }

    # Check 6 compiles one of the TUs the port removes, on the host, against the
    # real SDL3 -- that TU is a full SDL platform TU and the port's shim is not
    # meant to compile it. Without this tree that whole half of check 6 SKIPs,
    # so C0's baseline would be "2 checks skipped" instead of the single skip the
    # board-side run has, and C6b/C6c/C6d would be injecting into a TU nothing
    # compiles. A directory junction, not a copy: 3.7 MB per case, and nothing
    # ever writes into the tree. Junctions need no elevation; symlinks do.
    $sdlDst = Join-Path $root ('_deps\' + $sdlTop)
    $sdlParent = Split-Path -Parent $sdlDst
    if (-not (Test-Path -LiteralPath $sdlParent)) {
        New-Item -ItemType Directory -Force -Path $sdlParent | Out-Null
    }
    New-Item -ItemType Junction -Path $sdlDst -Target $realSdl | Out-Null

    $refDst = Join-Path $root 'refkeen'
    New-Item -ItemType Directory -Force -Path $refDst | Out-Null
    foreach ($sub in @('host', 'platform', 'tests')) {
        Copy-Item -LiteralPath (Join-Path $Workspace ('refkeen\' + $sub)) -Destination $refDst -Recurse -Force
    }
    $scDst = Join-Path $refDst 'scripts'
    New-Item -ItemType Directory -Force -Path $scDst | Out-Null
    Copy-Item -LiteralPath $factsPath -Destination $scDst -Force
    Copy-Item -LiteralPath $gateReal -Destination $scDst -Force

    # compat/ is the device boundary the port's includes read; common/ is the
    # shared layer, and frontend_shell.c is compiled and linked by check 5.
    Copy-Item -LiteralPath (Join-Path $Workspace 'compat') -Destination $root -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $Workspace 'common') -Destination $root -Recurse -Force

    # The sibling ports' platform headers: check 7 cross-checks the port's four
    # runtime declarations against these, so a mirror without them would report
    # "no sibling declaration found" in C0. Copied whole rather than one file at
    # a time, so a record that grows a sibling header does not silently turn
    # into a fixture defect.
    foreach ($sib in @('digger', 'xrick')) {
        $sibDst = Join-Path $root ($sib + '\platform')
        New-Item -ItemType Directory -Force -Path $sibDst | Out-Null
        Copy-Item -LiteralPath (Join-Path $Workspace ($sib + '\platform\retropicoes')) `
                  -Destination $sibDst -Recurse -Force
    }
    return $root
}

function Get-MirrorPath {
    param([string]$Root, [string]$Relative)
    return (Join-Path $Root $Relative)
}

# Raw ASCII text, no BOM, no line-ending translation. The injected snippets are
# ASCII-only and every source they touch is ASCII-only, so ASCII is lossless
# here -- and an explicit encoding is what keeps the compiler from seeing a BOM
# the real file never had.
# Retrying raw write. The first dry run of this file lost 13 of 25 injections to
# "access denied" on files that were present, writable and readable a moment
# later -- the filesystem was fine, the writes were colliding with something
# holding each freshly-copied file open for a moment (a scanner, in the shape
# that only blocks writers: reads kept working, which is why the gate never saw
# it). A fixture that fails at random is worse than one that fails loudly, so the
# write retries, and then READS ITS OWN RESULT BACK: a write that did not land
# byte-for-byte is a broken fixture, not a broken port.
function Write-RawChecked([string]$Path, [string]$Text, [int]$Attempts = 10) {
    $attempt = 0
    while ($true) {
        $attempt++
        try {
            Set-Content -LiteralPath $Path -Value $Text -Encoding ASCII -NoNewline -ErrorAction Stop
            $back = Get-Content -LiteralPath $Path -Raw -ErrorAction Stop
            if ($back -ceq $Text) { return }
            $lastError = 'the file did not read back identically after the write'
        } catch {
            $lastError = $_.Exception.Message
        }
        if ($attempt -ge $Attempts) {
            throw ("write of {0} failed after {1} attempt(s): {2}" -f (Split-Path -Leaf $Path), $attempt, $lastError)
        }
        Start-Sleep -Milliseconds 200
    }
}

function Append-LinesChecked([string]$Path, [string[]]$Lines, [int]$Attempts = 10) {
    $before = ''
    if (Test-Path -LiteralPath $Path) { $before = Get-Content -LiteralPath $Path -Raw }
    $attempt = 0
    while ($true) {
        $attempt++
        try {
            Add-Content -LiteralPath $Path -Value ($Lines -join "`n") -Encoding ASCII -ErrorAction Stop
            $after = Get-Content -LiteralPath $Path -Raw -ErrorAction Stop
            if ($after.Length -gt $before.Length -and $after.Substring(0, $before.Length) -ceq $before) { return }
            $lastError = 'the appended text is not visible in the file'
        } catch {
            $lastError = $_.Exception.Message
        }
        if ($attempt -ge $Attempts) {
            throw ("append to {0} failed after {1} attempt(s): {2}" -f (Split-Path -Leaf $Path), $attempt, $lastError)
        }
        Start-Sleep -Milliseconds 200
    }
}

function Set-Raw([string]$Path, [string]$Text) { Write-RawChecked $Path $Text }

function Add-Lines([string]$Path, [string[]]$Lines) { Append-LinesChecked $Path $Lines }

# Anchors are written in LF and matched in LF, with the file's own convention
# restored afterwards. The first draft of this file assumed every source was
# CRLF and appended a literal CRLF to one anchor -- and the assumption came from
# a bad measurement rather than from the file: `grep -c $'\r'` in this shell does
# NOT expand $'\r', so grep is given an empty pattern and counts every line,
# which reads exactly like "every line ends with CR". The real answer is that
# this whole tree is LF-only (`printf '\r'` is what measures it).
#
# Normalising removes the class: an anchor cannot be wrong about line endings
# because it does not state any, and the mirror keeps the original convention.
function To-Lf([string]$Text) { return $Text.Replace("`r`n", "`n") }

function Replace-Once([string]$Path, [string]$FromLf, [string]$ToLf, [string]$What) {
    $raw = Get-Content -LiteralPath $Path -Raw
    $wasCrlf = $raw.Contains("`r`n")
    $lf = To-Lf $raw
    # Exactly once, not "at least once": a plain .Replace() rewrites EVERY
    # occurrence, so an anchor that also appears in a second block would edit a
    # part of the record the case never meant to touch -- and the case would
    # still report the defect it was aiming for, hiding the collateral damage.
    $count = ([regex]::Matches($lf, [regex]::Escape($FromLf))).Count
    if ($count -eq 0) {
        throw ("injection anchor not found in {0}: {1}" -f (Split-Path -Leaf $Path), $What)
    }
    if ($count -gt 1) {
        throw ("injection anchor is not unique in {0} -- {1} matches: {2}" -f (Split-Path -Leaf $Path), $count, $What)
    }
    $out = $lf.Replace($FromLf, $ToLf)
    if ($wasCrlf) { $out = $out.Replace("`n", "`r`n") }
    Set-Raw $Path $out
}

function Replace-InFile([string]$Path, [string]$From, [string]$To) {
    Replace-Once $Path (To-Lf $From) (To-Lf $To) $From
}

# Multi-line anchors go through the same helper, so an anchor written with bare
# "`n" is safe whatever the file's own convention is.
function Replace-Block([string]$Path, [string[]]$FromLines, [string[]]$ToLines) {
    Replace-Once $Path ($FromLines -join "`n") ($ToLines -join "`n") $FromLines[0]
}

function New-Probe([string]$Path, [string[]]$Lines) {
    Set-Raw $Path (($Lines -join "`n") + "`n")
}

function Assert-Case {
    param(
        [string]$Name,
        [string]$Tag,
        [ValidateSet('pass', 'fail')][string]$Expect,
        [scriptblock]$Inject,
        [string[]]$MustMatch = @(),
        [string[]]$MustNotMatch = @()
    )
    $script:Cases++
    $root = New-Mirror $Tag

    # Integrity first, injection second, gate third. Without this the two failure
    # modes are indistinguishable at the end of the run, and the one that costs a
    # whole round is the wrong conclusion: a broken fixture reported as a port
    # defect.
    $intact = $null
    foreach ($rel in $script:MirrorMustHave) {
        $srcItem = Get-Item -LiteralPath (Join-Path $Workspace $rel) -ErrorAction SilentlyContinue
        $dstItem = Get-Item -LiteralPath (Join-Path $root $rel) -ErrorAction SilentlyContinue
        if ($null -eq $dstItem) { $intact = 'not in the mirror: ' + $rel; break }
        if ($null -eq $srcItem) { $intact = 'not in the workspace: ' + $rel; break }
        if ($dstItem.Length -ne $srcItem.Length) {
            $intact = ('wrong length in the mirror: {0} ({1} vs {2})' -f $rel, $dstItem.Length, $srcItem.Length)
            break
        }
    }
    if ($intact) {
        $script:Bad++
        Say ("  FAIL  {0}" -f $Name)
        Say ("          the fixture is not intact, so this case proves nothing: " + $intact)
        return
    }

    # The injection is a fixture, and a fixture that did not land must be
    # reported as THIS case failing rather than aborting the run: the first
    # draft let a bad anchor throw out of a twenty-six-case run, which loses
    # every later case's evidence and reads as "the self test is broken" instead
    # of "C6b's anchor is wrong". A case whose injection did not land proves
    # nothing, so it does not get to run the gate at all and counts as failed.
    $injectError = $null
    if ($Inject) {
        try { & $Inject $root } catch { $injectError = $_.Exception.Message }
    }
    if ($injectError) {
        $script:Bad++
        Say ("  FAIL  {0}" -f $Name)
        Say ("          the injection did not land, so the case proves nothing: " + $injectError)
        return
    }
    if ($script:VerifyOnly) {
        Say ("  ok    {0} (injection landed)" -f $Name)
        return
    }

    $gate = Join-Path $root ('refkeen\scripts\' + $gateName)
    $rep = Join-Path $root 'gate-report.txt'
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $gate -Workspace $root -UpstreamRoot (Join-Path $root '_deps\reflectionhle-upstream') `
                -TargetGcc $TargetGcc -HostGcc $HostGcc -ReportPath $rep | Out-Null
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    # A missing report with rc=0 is the false-pass signature this project has
    # hit before: check for the artifact, not for the exit code.
    $exists = Test-Path -LiteralPath $rep -PathType Leaf
    $text = if ($exists) { Get-Content -LiteralPath $rep -Raw } else { '' }

    $problems = New-Object 'System.Collections.Generic.List[string]'
    if (-not $exists) { [void]$problems.Add('the gate wrote no report at all') }
    foreach ($m in $MustMatch) {
        if ($text -notmatch $m) { [void]$problems.Add("expected to match /$m/ but did not") }
    }
    foreach ($m in $MustNotMatch) {
        if ($text -match $m) { [void]$problems.Add("expected NOT to match /$m/ but did") }
    }
    if ($Expect -eq 'pass' -and $rc -ne 0) { [void]$problems.Add("expected exit 0, got $rc") }
    if ($Expect -eq 'fail' -and $rc -eq 0) { [void]$problems.Add('expected a non-zero exit, got 0') }

    if ($problems.Count -eq 0) {
        Say ("  ok    {0}" -f $Name)
    } else {
        $script:Bad++
        Say ("  FAIL  {0}" -f $Name)
        foreach ($p in $problems) { Say ("          " + $p) }
        foreach ($line in ($text -split "`r?`n")) {
            if ($line -match '^\s*(FAIL|SKIP|FRONTEND)') { Say ("          | " + $line.Trim()) }
        }
    }
}

Say "S20 FRONTEND ADAPTER GATE -- NEGATIVE PATH SELF TEST"
Say ("Workspace : {0}" -f $Workspace)
Say ("Gate      : {0}" -f $gateReal)
Say ("Target gcc: {0}" -f $TargetGcc)
Say ("Host gcc  : {0}" -f $HostGcc)
Say ("SDL3 tree : {0} (junctioned into every mirror)" -f $realSdl)
Say ("Run id    : {0}" -f $RunId)
Say ""

if (-not (Test-Path -LiteralPath $realUpstream -PathType Container)) {
    Say "upstream checkout not found -- cannot mirror a fixture"
    Flush-Out
    exit 2
}
if (-not (Test-Path -LiteralPath $realSdl -PathType Container)) {
    # Fail closed rather than run a baseline that silently checks less: check 6's
    # object half would SKIP in every case, C0 would be asserting a different
    # report than the real one, and the three cases that inject into that TU
    # would be injecting into something nothing compiles.
    Say ("the pinned SDL3 tree is missing: {0}" -f $realSdl)
    Say "that is the tree check 6 compiles the removed SDL TU against; refusing to run a fixture that would skip it"
    Flush-Out
    exit 2
}

$otherChecks = @('check 1:', 'check 2:', 'check 3:', 'check 4:', 'check 5:', 'check 6:', 'check 7:')
function Others-But([string[]]$Keep) {
    return @($otherChecks | Where-Object { $Keep -notcontains $_ })
}

# --- C0: the clean mirror must pass ----------------------------------------
# The assertions are deliberately more than "it says PASS": every check title
# must be present, the two contract notes that carry the measured numbers must
# be there, and the PASS line must END exactly where the real run's does --
# including the single known skip. The gate appends " -- N check(s) SKIPPED"
# whenever a sub-check could not run, so a different count means C0 passed with
# checks missing, which would make every later case's "the other checks stayed
# quiet" claim meaningless.
Say "C0   clean mirror of the real sources -> the gate must PASS"
Assert-Case -Name 'clean mirror passes, with the one known skip' -Tag 'c0' -Expect 'pass' -Inject $null `
    -MustMatch @('FRONTEND ADAPTER: PASS \(7 checks\) -- 1 check\(s\) SKIPPED, see above\s*$',
                 'Check 1\s', 'Check 2\s', 'Check 3\s', 'Check 4\s',
                 'Check 5\s', 'Check 6\s', 'Check 7\s',
                 'ok    the adapter exports exactly the 8 recorded symbol\(s\)',
                 'ok    section sizes: \.text=628 \.data=0 \.bss=64 \.rodata=64',
                 'ok    the adapter uses exactly the 11 recorded shared call',
                 'ok    the host test passes with 76 assertion\(s\) under -Werror',
                 'ok    BE_ST_QuickExit is named by be_events_port\.c')

# --- C1: an export in a section no size check sums -------------------------
Say "C1   the compiled adapter gains an export the record lacks -> check 1"
Assert-Case -Name 'export drift is caught' -Tag 'c1' -Expect 'fail' `
    -MustMatch @("check 1: the adapter's exported set is not the recorded one",
                 'extra   : fe_probe_extra_export') `
    -MustNotMatch (Others-But @('check 1:')) `
    -Inject {
        param($root)
        # Its own section on purpose: a plain added function would add .text
        # bytes, and check 2 sums .text for the same object -- the case would
        # then be proving check 2 as much as check 1.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\frontend.c'
        Add-Lines $f @(
            '',
            '/* probe: an export the record does not have, in a section no size check sums */',
            '__attribute__((section(".probe_extra"))) int fe_probe_extra_export;'
        )
    }

# --- C1b: the record's spelling changes only in CASE ----------------------
# The one deliberate two-check case: both check 1 (nm) and check 5 (the host
# test's own list) are compared against this same record, so a case-insensitive
# comparison in either would let a renamed symbol through while both sides print
# identically.
Say "C1b  the record's symbol spelling changes CASE only -> checks 1 and 5"
Assert-Case -Name 'a case-only rename is not mistaken for a match' -Tag 'c1b' -Expect 'fail' `
    -MustMatch @("check 1: the adapter's exported set is not the recorded one",
                 "check 5: the host test's symbol list disagrees with the record") `
    -MustNotMatch (Others-But @('check 1:', 'check 5:')) `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f "'retropicoes_refkeen_frontend_boot'" "'retropicoes_refkeen_frontend_Boot'"
    }

# --- C2a: the recorded .text figure drifts --------------------------------
Say "C2a  the recorded .text figure drifting from the measurement -> check 2"
Assert-Case -Name 'a drifted section size is caught' -Tag 'c2a' -Expect 'fail' `
    -MustMatch @('check 2: \.text is 628 B, recorded 640 B') `
    -MustNotMatch (Others-But @('check 2:')) `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f 'TextBytes = 628' 'TextBytes = 640'
    }

# --- C2b: the recorded diagnostic set drifts ------------------------------
Say "C2b  the recorded diagnostic set drifting from the measurement -> check 2"
Assert-Case -Name 'a drifted diagnostic record is caught' -Tag 'c2b' -Expect 'fail' `
    -MustMatch @("check 2: the adapter's diagnostic set changed",
                 'measured: \(none\)') `
    -MustNotMatch (Others-But @('check 2:')) `
    -Inject {
        param($root)
        # The record side, not the source side: making the compiler actually warn
        # would need code both toolchains see, and check 5 builds the same TU
        # under -Werror -- so the warning would fail check 5's build too, and the
        # case would stop being about check 2.
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f 'FrozenDiagnostics = @()' "FrozenDiagnostics = @('-Wunused-function=1')"
    }

# --- C3a: the recorded shared surface drops a call ------------------------
Say "C3a  the recorded shared surface dropping a call -> check 3"
Assert-Case -Name 'a drifted shared-surface record is caught' -Tag 'c3a' -Expect 'fail' `
    -MustMatch @('check 3: the shared surface the adapter uses is not the recorded one',
                 'extra   : retropicoes_input_menu_pressed') `
    -MustNotMatch (Others-But @('check 3:')) `
    -Inject {
        param($root)
        # The RECORD is short by one; the adapter is untouched. Removing the call
        # from the adapter instead would move .text and fire check 2 (see the
        # description) -- and it would also silently change what the port does,
        # which a fixture has no business doing.
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-Block $f `
            @("            'retropicoes_frontend_shell_settings_action'",
              "            'retropicoes_input_menu_pressed'") `
            @("            'retropicoes_frontend_shell_settings_action'")
    }

# --- C3b: a port file reaches a ROM browser ------------------------------
Say "C3b  a port file calling a ROM browser function -> check 3"
Assert-Case -Name 'a ROM browser call in the port tree is caught' -Tag 'c3b' -Expect 'fail' `
    -MustMatch @('check 3: a port file calls a ROM browser function: probe_rom\.c') `
    -MustNotMatch (Others-But @('check 3:')) `
    -Inject {
        param($root)
        # Never compiled, so it does not have to link -- what matters is that the
        # call is reachable in the tree's code view. Using the name the port
        # would have had to invent (a retropicoes_rom_* symbol) rather than
        # draw_rom_menu(): the latter is also a name check 7 looks for, so that
        # injection would trip two checks.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\probe_rom.c'
        New-Probe $f @(
            '/* probe: reaches a ROM browser, which a single native game must not */',
            'void fe_probe_rom(void) { retropicoes_rom_browser_open(); }'
        )
    }

# --- C3c: the compiled adapter gains an undefined symbol -----------------
Say "C3c  the compiled adapter gaining an undefined symbol -> check 3"
Assert-Case -Name 'an extra shared call is caught from the object' -Tag 'c3c' -Expect 'fail' `
    -MustMatch @('check 3: the shared surface the adapter uses is not the recorded one',
                 'extra   : retropicoes_probe_extra_call') `
    -MustNotMatch (Others-But @('check 3:')) `
    -Inject {
        param($root)
        # The measured side of check 3, which C3a's record drift cannot prove.
        # Three properties are needed at once and each one is why the snippet
        # looks the way it does:
        #   static + used   -> no -Wunused-variable warning, and it stays out of
        #                      nm -g, so check 1 does not see it
        #   .probe_extra    -> .text/.data/.bss/.rodata stay byte-identical, so
        #                      check 2 does not see it
        #   #ifdef __arm__  -> the host build, which links this TU, does not see
        #                      it either: an undefined symbol would fail the link
        #                      and check 5 would fire for the wrong reason
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\frontend.c'
        Add-Lines $f @(
            '',
            '#ifdef __arm__',
            '/* probe: one undefined symbol the record does not have */',
            'extern void retropicoes_probe_extra_call(void);',
            'static void *fe_probe_extra_ref __attribute__((used, section(".probe_extra"))) =',
            '    (void *)retropicoes_probe_extra_call;',
            '#endif'
        )
    }

# --- C4a: the caller stops calling the hook ------------------------------
Say "C4a  the caller stops calling the per-frame hook -> check 4"
Assert-Case -Name 'a hook nothing runs is caught' -Tag 'c4a' -Expect 'fail' `
    -MustMatch @('check 4: be_events_port\.c does not call retropicoes_refkeen_frontend_pump') `
    -MustNotMatch (Others-But @('check 4:')) `
    -Inject {
        param($root)
        # The call site, not the definition: the TU still compiles, so the case
        # proves the wiring check rather than "the port stopped building".
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\be_events_port.c'
        Replace-InFile $f 'frontendBits = retropicoes_refkeen_frontend_pump();' `
            'frontendBits = 0u; /* probe: the hook is never called */'
    }

# --- C4b: the caller stops consuming SETTINGS_RAN ------------------------
Say "C4b  the caller stops reading the SETTINGS_RAN bit -> check 4"
Assert-Case -Name 'a hook whose answer nobody reads is caught' -Tag 'c4b' -Expect 'fail' `
    -MustMatch @('check 4: the caller does not look at the SETTINGS_RAN bit') `
    -MustNotMatch (Others-But @('check 4:')) `
    -Inject {
        param($root)
        # IDLE is 0, so this is the defect and not a typo that would not compile:
        # the engine's keyboard view is simply never reseeded after the page.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\be_events_port.c'
        Replace-InFile $f 'if (frontendBits & RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN)' `
            'if (frontendBits & RETROPICOES_REFKEEN_FRONTEND_IDLE)'
    }

# --- C4c: the hook header grows an include -------------------------------
Say "C4c  the hook header growing an include -> check 4"
Assert-Case -Name 'a dependency-free hook header that is not is caught' -Tag 'c4c' -Expect 'fail' `
    -MustMatch @('check 4: frontend_hook\.h grew an include') `
    -MustNotMatch (Others-But @('check 4:')) `
    -Inject {
        param($root)
        # Inside the guard, so the include is real as well as visible -- the
        # case is about the event TU acquiring a dependency, which is exactly
        # what happens when this header stops being self-contained.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\frontend_hook.h'
        Replace-InFile $f '#define REFKEEN_RETROPICOES_FRONTEND_HOOK_H' `
            "#define REFKEEN_RETROPICOES_FRONTEND_HOOK_H`n`n#include <stddef.h>"
    }

# --- C4d: a fourth includer appears --------------------------------------
Say "C4d  a fourth file including the hook header -> check 4"
Assert-Case -Name 'a moved includer set is caught' -Tag 'c4d' -Expect 'fail' `
    -MustMatch @("check 4: the hook header's includers moved",
                 'measured: be_events_port\.c\|frontend\.h\|probe_hook\.c\|test_frontend_adapter\.c') `
    -MustNotMatch (Others-But @('check 4:')) `
    -Inject {
        param($root)
        # A new file on purpose, and one nothing compiles: adding the include to
        # an existing TU would also be a change to a measured surface.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\probe_hook.c'
        New-Probe $f @('#include "frontend_hook.h"')
    }

# --- C5a: the host test's symbol list lies -------------------------------
Say "C5a  the host test printing a symbol the adapter does not have -> check 5"
Assert-Case -Name 'a lying compiled contract is caught' -Tag 'c5a' -Expect 'fail' `
    -MustMatch @("check 5: the host test's symbol list disagrees with the record") `
    -MustNotMatch (Others-But @('check 5:')) `
    -Inject {
        param($root)
        # check 5 cross-checks two derivations of one fact -- the record and what
        # the compiled test reports -- so a defect in either must be caught.
        $f = Get-MirrorPath $root 'refkeen\tests\test_frontend_adapter.c'
        $anchor = '    printf("CONTRACT symbols=%u\n", (unsigned)SYMBOL_COUNT);'
        $forged = '    printf("CONTRACT symbol=retropicoes_probe_lying_contract\n");'
        Replace-InFile $f $anchor ($forged + "`n" + $anchor)
    }

# --- C5b: a forged CONTRACT scalar ---------------------------------------
Say "C5b  the host test's CONTRACT scalar being forged -> check 5"
Assert-Case -Name 'a forged scalar is caught' -Tag 'c5b' -Expect 'fail' `
    -MustMatch @('check 5: CONTRACT bit_settings=9, recorded 1') `
    -MustNotMatch (Others-But @('check 5:')) `
    -Inject {
        param($root)
        # Printing a literal rather than changing what the enum says: the case is
        # about the gate comparing the two, not about the shared header moving.
        $f = Get-MirrorPath $root 'refkeen\tests\test_frontend_adapter.c'
        Replace-Block $f `
            @('    printf("CONTRACT bit_settings=%u\n",',
              '           (unsigned)RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN);') `
            @('    printf("CONTRACT bit_settings=9\n");')
    }

# --- C5c: a failing assertion whose harness stops reporting --------------
Say "C5c  the host test failing an assertion and exiting 0 without a PASS marker -> check 5"
Assert-Case -Name 'a harness that swallows the outcome cannot pass by exit code' -Tag 'c5c' -Expect 'fail' `
    -MustMatch @('check 5: the host test did not print its PASS marker') `
    -MustNotMatch (Others-But @('check 5:')) `
    -Inject {
        param($root)
        # A real defect somewhere in the port plus a harness that has stopped
        # reporting it. The gate has to notice without the help of the exit code
        # -- the lesson this project learned when a missing report file came with
        # rc=0.
        $f = Get-MirrorPath $root 'refkeen\tests\test_frontend_adapter.c'
        Replace-InFile $f '    ok(bits & RETROPICOES_REFKEEN_FRONTEND_QUIT, "the quit bit is reported");' `
            '    ok(0, "the quit bit is reported");'
        Replace-Block $f `
            @('        printf("REFKEEN FRONTEND ADAPTER: FAIL (%u of %u assertion(s))\n",',
              '               g_failures, g_checks);',
              '        return 1;') `
            @('        printf("REFKEEN FRONTEND ADAPTER: FAIL (%u of %u assertion(s))\n",',
              '               g_failures, g_checks);',
              '        return 0;')
    }

# --- C5d: a harness that reports success while assertions failed ----------
Say "C5d  a harness printing the PASS marker with failed assertion(s) -> check 5"
Assert-Case -Name 'the PASS marker is not enough on its own' -Tag 'c5d' -Expect 'fail' `
    -MustMatch @('check 5: the host test reported 1 failed assertion\(s\)') `
    -MustNotMatch (Others-But @('check 5:')) `
    -Inject {
        param($root)
        # The half that C5c cannot reach, and it is not hypothetical: the gate's
        # FAIL-line scan lives inside the branch that requires the PASS marker, so
        # with this test's structure -- PASS printed only when nothing failed --
        # that scan was UNREACHABLE until a case existed that prints both. A
        # sub-assertion nothing can trigger is worth knowing about, and the fix is
        # a case, not a code change: the scan is a correct belt for a harness that
        # reports success while assertions failed.
        $f = Get-MirrorPath $root 'refkeen\tests\test_frontend_adapter.c'
        Replace-InFile $f '    ok(bits & RETROPICOES_REFKEEN_FRONTEND_QUIT, "the quit bit is reported");' `
            '    ok(0, "the quit bit is reported");'
        Replace-Block $f `
            @('        printf("REFKEEN FRONTEND ADAPTER: FAIL (%u of %u assertion(s))\n",',
              '               g_failures, g_checks);',
              '        return 1;') `
            @('        printf("REFKEEN FRONTEND ADAPTER: PASS (%u assertions)\n", g_checks);',
              '        return 0;')
    }

# --- C6a: the port stops naming the quit primitive -----------------------
Say "C6a  the port stopping naming the quit primitive -> check 6"
Assert-Case -Name 'a renamed quit primitive is caught' -Tag 'c6a' -Expect 'fail' `
    -MustMatch @('check 6: the set of port files naming the quit primitive moved') `
    -MustNotMatch (Others-But @('check 6:')) `
    -Inject {
        param($root)
        # BE_ST_HandleExit is a real upstream name, so this is the plausible
        # mistake rather than a typo: it is the function the port's own primitive
        # is inlined into, and naming it instead is exactly the confusion the
        # record exists to prevent.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\be_events_port.c'
        Replace-InFile $f 'BE_ST_QuickExit();' 'BE_ST_HandleExit();'
    }

# --- C6b: the defining TU is no longer removed ---------------------------
Say "C6b  the defining TU no longer recorded as removed -> check 6"
Assert-Case -Name 'a definer the port still builds is caught' -Tag 'c6b' -Expect 'fail' `
    -MustMatch @('check 6: src/backend/startup/be_startup_sdl\.c is defined by a TU the port still builds') `
    -MustNotMatch (Others-But @('check 6:')) `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f "            'src/backend/startup/be_startup_sdl.c' = 'platform TU'`n" ''
    }

# --- C6c: the defining TU stops defining it ------------------------------
Say "C6c  the defining TU stopping defining the symbol -> check 6"
Assert-Case -Name 'a defining TU that no longer defines is caught' -Tag 'c6c' -Expect 'fail' `
    -MustMatch @('check 6: BE_ST_QuickExit has 0 defining TU\(s\) in upstream.s src; expected exactly 1') `
    -MustNotMatch (Others-But @('check 6:')) `
    -Inject {
        param($root)
        # The definition line only (this file has no prototype of its own), so
        # the TU still compiles -- a TU that failed to compile would take the
        # other branch and prove something else.
        $f = Get-MirrorPath $root '_deps\reflectionhle-upstream\src\backend\startup\be_startup_sdl.c'
        Replace-InFile $f 'void BE_ST_QuickExit(void)' 'void BE_ST_QuickExitProbe(void)'
    }

# --- C6d: the defining TU does not compile on the host -------------------
Say "C6d  the defining TU failing to compile on the host -> check 6"
Assert-Case -Name 'a definer that cannot be observed is caught' -Tag 'c6d' -Expect 'fail' `
    -MustMatch @('check 6: the defining TU does not compile on the host against real SDL3') `
    -MustNotMatch (Others-But @('check 6:')) `
    -Inject {
        param($root)
        # A deliberate preprocessor failure rather than a deleted semicolon: the
        # point is that the gate notices it cannot observe the definition, and a
        # syntax error's cascade of diagnostics is not part of what is proven.
        $f = Get-MirrorPath $root '_deps\reflectionhle-upstream\src\backend\startup\be_startup_sdl.c'
        Add-Lines $f @('#error probe: the defining TU must not compile')
    }

# --- C7a1: the port's own declaration drifts -----------------------------
Say "C7a1 the port's own header drifting from the recorded interface -> check 7"
Assert-Case -Name 'a drifted port-side declaration is caught' -Tag 'c7a1' -Expect 'fail' `
    -MustMatch @('check 7: the port declares \[void retropicoes_return_to_bootloader\(int force\);\]') `
    -MustNotMatch (Others-But @('check 7:')) `
    -Inject {
        param($root)
        # The gap this case found: the gate used to compare only the SIBLING
        # headers against the record, and to search the port's own header for the
        # name alone. A drifted signature there compiles and links -- the C linker
        # does not check signatures -- so it was a false green until check 7 was
        # extended to compare the port's declaration too.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\frontend.h'
        Replace-InFile $f 'void retropicoes_return_to_bootloader(void);' `
            'void retropicoes_return_to_bootloader(int force);'
    }

# --- C7a2: a sibling's declaration drifts --------------------------------
Say "C7a2 a sibling port's header drifting from the same record -> check 7"
Assert-Case -Name 'a drifted sibling declaration is caught' -Tag 'c7a2' -Expect 'fail' `
    -MustMatch @('check 7: .*runtime\.h: retropicoes_return_to_bootloader says \[void retropicoes_return_to_bootloader\(int force\);\]') `
    -MustNotMatch (Others-But @('check 7:')) `
    -Inject {
        param($root)
        # One sibling only: the other still declares it, so the check's "found at
        # least one" half stays satisfied and only the mismatch branch reacts.
        $f = Get-MirrorPath $root 'xrick\platform\retropicoes\runtime.h'
        Replace-InFile $f 'void retropicoes_return_to_bootloader(void);' `
            'void retropicoes_return_to_bootloader(int force);'
    }

# --- C7b: the legacy menu gesture comes back -----------------------------
Say "C7b  a port file carrying the legacy menu gesture -> check 7"
Assert-Case -Name 'the pre-frontend menu gesture is caught' -Tag 'c7b' -Expect 'fail' `
    -MustMatch @('check 7: legacy menu gesture in probe_gesture\.c') `
    -MustNotMatch (Others-But @('check 7:')) `
    -Inject {
        param($root)
        # In a string, not in a comment: check 7 scans this rule over the raw
        # text on purpose (a gesture is a gesture whether or not it is commented
        # out in someone's draft), and the case should exercise the rule rather
        # than the comment stripping.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\probe_gesture.c'
        New-Probe $f @(
            '/* probe: the private gesture the shared frontend replaced */',
            'const char *fe_probe_gesture = "hold Start to open the menu";'
        )
    }

# --- C7c: a landed CMakeLists missing a token ----------------------------
Say "C7c  a landed CMakeLists missing a required token -> check 7"
Assert-Case -Name 'a landed CMakeLists contract that is incomplete is caught' -Tag 'c7c' -Expect 'fail' `
    -MustMatch @('check 7: refkeen/pico/CMakeLists\.txt must include native_game_settings\.c') `
    -MustNotMatch (Others-But @('check 7:')) `
    -Inject {
        param($root)
        # The CMake contract is the one sub-check that SKIPs in the real tree
        # (C13 has not landed), so these three cases are the only thing that
        # proves the branch can do anything at all.
        $f = Get-MirrorPath $root 'refkeen\pico\CMakeLists.txt'
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $f) | Out-Null
        Set-Raw $f ("target_sources(refkeen PRIVATE frontend_shell.c settings_host.c)`r`n")
    }

# --- C7d: a landed CMakeLists links the ROM browser ----------------------
Say "C7d  a landed CMakeLists linking rom_browser.c -> check 7"
Assert-Case -Name 'a landed CMakeLists with a ROM browser is caught' -Tag 'c7d' -Expect 'fail' `
    -MustMatch @('check 7: refkeen/pico/CMakeLists\.txt links rom_browser\.c, which a single native game must not') `
    -MustNotMatch (Others-But @('check 7:')) `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\pico\CMakeLists.txt'
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $f) | Out-Null
        Set-Raw $f (@(
            'target_sources(refkeen PRIVATE native_game_settings.c frontend_shell.c settings_host.c)',
            'target_sources(refkeen PRIVATE ${CMAKE_SOURCE_DIR}/common/frontend/rom_browser.c)'
        ) -join "`r`n")
    }

# --- C7e: a landed CMakeLists that satisfies the contract ----------------
# The positive half, and the only case where the gate must PASS with NO skip:
# without it, a check that could never run the CMake branch would look identical
# to one that runs it correctly, and C0's "1 check(s) SKIPPED" would be read as
# permanent rather than as a statement about C13.
Say "C7e  a landed CMakeLists that satisfies the contract -> PASS, no skip"
Assert-Case -Name 'a landed CMakeLists satisfies the CMake contract' -Tag 'c7e' -Expect 'pass' `
    -MustMatch @('FRONTEND ADAPTER: PASS \(7 checks\)\s*$') `
    -MustNotMatch @('SKIP\s') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\pico\CMakeLists.txt'
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $f) | Out-Null
        Set-Raw $f (@(
            'cmake_minimum_required(VERSION 3.13)',
            'target_sources(refkeen PRIVATE',
            '    ${CMAKE_SOURCE_DIR}/common/frontend/native_game_settings.c',
            '    ${CMAKE_SOURCE_DIR}/common/frontend/frontend_shell.c',
            '    ${CMAKE_SOURCE_DIR}/common/frontend/settings_host.c)'
        ) -join "`r`n")
    }

# --- Fixture check: did every injection land? -----------------------------
# -VerifyInjections stops here, and says what it did and did not establish.
if ($script:VerifyOnly) {
    Say ""
    Say "========================================================================"
    if ($script:Bad -eq 0) {
        Say ("INJECTION CHECK: OK ({0}/{0} anchors landed)" -f $script:Cases)
        Say "This is a statement about the fixture, not about the gate: no gate ran, so nothing here says the gate can fail."
        Flush-Out
        exit 0
    }
    Say ("INJECTION CHECK: FAILED ({0} of {1} anchors did not land)" -f $script:Bad, $script:Cases)
    Flush-Out
    exit 3
}

# --- Wire checks: the gate's own switch -----------------------------------
# Not an injection into a mirror: this is about the property that the switch
# declared in the gate's help is actually READ. This project has already shipped
# a switch that was documented, declared, and ignored -- passing it ran the
# ordinary gate and printed PASS, i.e. a negative test that could never fail.
# The companion did not exist while the gate was written, so its delegation
# pointed at a missing file; these two assertions are what keep that from being
# re-introduced the next time either file is renamed.
$gateText = Get-Content -LiteralPath $gateReal -Raw
$companion = Join-Path $PSScriptRoot 'Test-FrontendAdapterSelfTest.ps1'
$wire = New-Object 'System.Collections.Generic.List[string]'
if ($gateText -notmatch '\[switch\]\$SelfTest') { [void]$wire.Add('the gate does not declare a -SelfTest switch') }
if ($gateText -notmatch '(?m)^\s*if\s*\(\s*\$SelfTest\s*\)') { [void]$wire.Add('the gate declares -SelfTest but never branches on it') }
if ($gateText -notmatch [regex]::Escape('Test-FrontendAdapterSelfTest.ps1')) { [void]$wire.Add('the switch does not name this companion script') }
if (-not (Test-Path -LiteralPath $companion -PathType Leaf)) { [void]$wire.Add('the switch delegates to a file that does not exist') }
if ($gateText -notmatch 'exit\s+\$LASTEXITCODE') { [void]$wire.Add('the gate throws away the companion''s exit code') }

$gateLines = @(Get-Content -LiteralPath $gateReal)
$selfIdx = -1
$reportIdx = -1
for ($i = 0; $i -lt $gateLines.Count; $i++) {
    if ($selfIdx -lt 0 -and $gateLines[$i] -match 'if\s*\(\s*\$SelfTest\s*\)') { $selfIdx = $i }
    if ($reportIdx -lt 0 -and $gateLines[$i] -match 'if\s*\(-not\s+\$ReportPath\)') { $reportIdx = $i }
}
if ($selfIdx -lt 0 -or $reportIdx -lt 0 -or $selfIdx -gt $reportIdx) {
    # Order, not style: the gate's own ReportPath default would otherwise
    # overwrite the self-test's report path before the companion ever sees it.
    [void]$wire.Add('the gate applies its own defaults before handing off, so the companion would write to the wrong report')
}
$script:Cases++
Say "W1/W2 the gate's -SelfTest switch is declared, read, and delegates before its own defaults"
if ($wire.Count -eq 0) {
    Say "  ok    switch wired: declared, branched on, names an existing companion, and delegates before defaulting ReportPath"
} else {
    $script:Bad++
    Say "  FAIL  switch wiring"
    foreach ($w in $wire) { Say ("          " + $w) }
}

Say ""
Say "========================================================================"
if ($script:Bad -eq 0) {
    Say ("FRONTEND ADAPTER SELF-TEST: PASS ({0}/{0})" -f $script:Cases)
    Flush-Out
    exit 0
}
Say ("FRONTEND ADAPTER SELF-TEST: FAILED ({0} of {1} cases)" -f $script:Bad, $script:Cases)
Flush-Out
exit 1
