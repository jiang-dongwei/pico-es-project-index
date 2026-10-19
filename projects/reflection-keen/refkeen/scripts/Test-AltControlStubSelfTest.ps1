<#
.SYNOPSIS
    Negative-path self test for Test-AltControlStubContract.ps1.

.DESCRIPTION
    A gate that cannot fail is worse than no gate. This proves the altcontrol
    gate fails, and fails for the RIGHT reason: each case injects exactly one
    defect and then asserts both that the intended check fires and that the other
    six stay quiet. The second half is what keeps the attribution honest --
    without it, "check 3 failed" could just as easily be an artifact of the
    fixture.

    C0 is the clean baseline and must pass, or every later case is
    unattributable. The fixture is a mirror of the real sources, not a
    hand-written miniature: a miniature once produced an always-red C0 in this
    project and cost a whole round to diagnose.

    Cases:

      C0   nothing injected                                       -> PASS
      C1   a replaced TU grows one extra export                    -> check 1
      C2   PortRemovedTus naming a TU the target never had         -> check 2
      C3a  a built TU stops referencing a required symbol          -> check 3
      C3b  a dropped symbol gains a referencer in a built TU       -> check 3
      C4   the recorded .bss size drifts from the measurement      -> check 4
      C5   the host test prints a CONTRACT symbol that is a lie    -> check 5
      C6a  a port file #includes a replaced TU                     -> check 6
      C6b  a port file names a replaced TU in code, not prose      -> check 6
      C7a  the recorded removed-RAM figure drifts                  -> check 7
      C7b  the recorded removed-flash figure drifts                -> check 7

    NO CASE IS ALLOWED TO TRIP TWO CHECKS, and the injections are chosen for
    that, because this gate's checks read the same artifacts in different ways:

      * check 1 and check 3 both look at the two replaced TUs' definitions. C1
        therefore grows the *export set* of a replaced TU in a way that adds no
        bytes to any section check 7 sums -- the extra symbol is placed in a
        section of its own (`.probe_extra`) rather than in .text or .bss. A
        plain added function would have moved check 7's flash ledger too, and
        the case would have proved less than it looks like it proves.
      * check 4 and check 7 both read the stub's recorded sizes. C4 drifts
        StubBssBytes only; check 7's net saving stays positive (and it has no
        recorded net to compare), so it stays quiet.
      * check 1 and check 3 also share the symbol ledger, which is why the
        completeness cases inject into the ARTIFACTS (a call removed from a
        built TU; a reference added to one) instead of moving a name between
        RequiredSymbols and DroppedSymbols. Moving a name would have fired
        check 1 as well -- legitimately, but then the case proves neither check
        specifically.

    C3 is split in two because check 3 has a negative half and a positive half
    and they fail for different reasons: C3a makes a required symbol stop being
    used, C3b makes a dropped symbol start being used. One case would have
    proven only one direction, and the direction that is easy to get wrong when
    the stub is extended later is the second one.

    C6 is split for the same reason, and check 6 really has two questions in it:
    an include pulls the replaced TU's code back in, while a name surviving in
    code is a symbol the linker will ask for. C6b puts the leaf in code; C6a
    puts it in an #include. The 12 prose mentions in the real tree are what prove
    comments are not treated as either.

.PARAMETER RunId
    Suffix for the scratch trees. Nothing is ever deleted and re-created: the
    sandbox redirects recursive deletes to a fail-closed trash, so every run gets
    brand new directories. Each case copies a few MB of upstream sources.
#>
[CmdletBinding()]
param(
    [string]$Workspace = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$TargetGcc = 'F:\Download\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe',
    [string]$HostGcc = 'E:\LVGL\x86_64-14.2.0-release-win32-seh-msvcrt-rt_v12-rev2\mingw64\bin\gcc.exe',
    [string]$RunId = '',
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $RunId) { $RunId = ('{0:yyyyMMddHHmmss}' -f (Get-Date)) }
if (-not $ReportPath) {
    $ReportPath = Join-Path $Workspace ('.emv2\logs\s20-altcontrol-selftest-{0}.txt' -f $RunId)
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
$gateName = 'Test-AltControlStubContract.ps1'
$gateReal = Join-Path $PSScriptRoot $gateName

# The mirror is the whole upstream src/ tree minus the three directories that
# belong to games this port does not build. That is deliberately MORE than the
# 62 built TUs need, and the first draft of this file proved why it has to be:
# mirroring only the directories the built TUs sit in left out src/*.c (the
# be_cross*.c files live at src/ root) and src/rsrc/ (the XPM images that
# altcontroller.c and be_video_ui.c #include), so seven TUs failed to compile
# and C0 went red -- which is exactly what C0 is for. Copying the tree wholesale
# removes a whole class of "the fixture is not a fixture" mistakes; id91_11
# (3.3 MB), wolf3d (2.0 MB) and android-project are skipped only so that eleven
# cases do not each copy 5.5 MB more than they need.
$mirrorSkipDirs = @('id91_11', 'wolf3d', 'android-project')

function New-Mirror([string]$Tag) {
    $root = Join-Path $Workspace ("_deps\altcontrol-self-{0}-{1}" -f $RunId, $Tag)
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
    # rsrc/ is a SIBLING of src/, not a child of it: the XPM images are included
    # as "../rsrc/something.xpm", which reaches <upstream>/rsrc through
    # -I <upstream>/src. Leaving it out is what made altcontroller.c and
    # be_video_ui.c fail to compile in the first two drafts of this file, and
    # the failure reads as "the replaced TU does not compile" -- i.e. as a fact
    # about the port rather than about the fixture.
    $rsrc = Join-Path $realUpstream 'rsrc'
    if (Test-Path -LiteralPath $rsrc -PathType Container) {
        Copy-Item -LiteralPath $rsrc -Destination $upDst -Recurse -Force
    }

    $refDst = Join-Path $root 'refkeen'
    New-Item -ItemType Directory -Force -Path $refDst | Out-Null
    # host carries the shim and refkeen_config.h; platform carries the stub;
    # tests carries the host test that check 5 compiles and that C5 has to make
    # lie.
    foreach ($sub in @('host', 'platform', 'tests')) {
        Copy-Item -LiteralPath (Join-Path $Workspace ('refkeen\' + $sub)) -Destination $refDst -Recurse -Force
    }
    $scDst = Join-Path $refDst 'scripts'
    New-Item -ItemType Directory -Force -Path $scDst | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1') -Destination $scDst -Force
    Copy-Item -LiteralPath $gateReal -Destination $scDst -Force

    # compat/ is the device boundary the port's includes read.
    Copy-Item -LiteralPath (Join-Path $Workspace 'compat') -Destination $root -Recurse -Force
    return $root
}

function Get-MirrorPath {
    param([string]$Root, [string]$Relative)
    return (Join-Path $Root $Relative)
}

# Append or rewrite raw ASCII text. An explicit encoding keeps the file free of a
# BOM, so the compiler sees exactly upstream's file plus the injection.
function Add-Raw([string]$Path, [string]$Text) {
    Add-Content -LiteralPath $Path -Value $Text -Encoding ASCII -NoNewline
}

function Set-Raw([string]$Path, [string]$Text) {
    Set-Content -LiteralPath $Path -Value $Text -Encoding ASCII -NoNewline
}

function Replace-InFile([string]$Path, [string]$From, [string]$To) {
    $text = Get-Content -LiteralPath $Path -Raw
    if (-not $text.Contains($From)) {
        throw ("injection anchor not found in {0}: {1}" -f (Split-Path -Leaf $Path), $From)
    }
    Set-Raw $Path ($text.Replace($From, $To))
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
    if ($Inject) { & $Inject $root }

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
    $text = if (Test-Path -LiteralPath $rep -PathType Leaf) { Get-Content -LiteralPath $rep -Raw } else { '' }

    $problems = New-Object 'System.Collections.Generic.List[string]'
    if (-not $text) { [void]$problems.Add('the gate wrote no report at all') }
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
            if ($line -match '^\s*(FAIL|ALTCONTROL)') { Say ("          | " + $line.Trim()) }
        }
    }
}

Say "S20 ALTCONTROL STUB GATE -- NEGATIVE PATH SELF TEST"
Say ("Workspace : {0}" -f $Workspace)
Say ("Gate      : {0}" -f $gateReal)
Say ("Run id    : {0}" -f $RunId)
Say ""

if (-not (Test-Path -LiteralPath $realUpstream -PathType Container)) {
    Say "upstream checkout not found -- cannot mirror a fixture"
    Flush-Out
    exit 2
}

$otherChecks = @('check 1:', 'check 2:', 'check 3:', 'check 4:', 'check 5:', 'check 6:', 'check 7:')
function Others-But([string]$Keep) {
    return @($otherChecks | Where-Object { $_ -ne $Keep })
}

# --- C0: the clean mirror must pass ----------------------------------------
# The assertions are deliberately more than "it says PASS": every check title
# must be present, and the PASS line must END where it says (7 checks). The gate
# appends " -- N check(s) SKIPPED" when a check could not run, so a suffix would
# mean C0 passed with checks missing -- which would make every later case's
# "the other checks stayed quiet" claim meaningless.
Say "C0   clean mirror of the real sources -> the gate must PASS"
Assert-Case -Name 'clean mirror passes' -Tag 'c0' -Expect 'pass' -Inject $null `
    -MustMatch @('ALTCONTROL STUB: PASS \(7 checks\)\s*$',
                 'Check 1\s', 'Check 2\s', 'Check 3\s', 'Check 4\s',
                 'Check 5\s', 'Check 6\s', 'Check 7\s+the cut pays for itself')

# --- C1: a replaced TU grows an export that costs no section any check sums -
Say "C1   an extra export in a replaced TU -> check 1"
Assert-Case -Name 'export drift is caught' -Tag 'c1' -Expect 'fail' `
    -MustMatch @("check 1: the replaced TUs. exported set moved") `
    -MustNotMatch (Others-But 'check 1:') `
    -Inject {
        param($root)
        # Placed in its own section on purpose: a plain added function would add
        # .text bytes and C1 would move check 7's flash ledger as well. The
        # section is never summed by check 7, so only the export set moves.
        $f = Get-MirrorPath $root '_deps\reflectionhle-upstream\src\backend\input\be_input_controller_mappings.c'
        Add-Raw $f "`n__attribute__((section(`".probe_extra`"))) int bel_probe_extra_export;`n"
    }

# --- C2: the removal list names a TU the target never had ------------------
Say "C2   PortRemovedTus naming a TU outside the target -> check 2"
Assert-Case -Name 'a removal outside the target is caught' -Tag 'c2' -Expect 'fail' `
    -MustMatch @('check 2: TU recorded as removed by the port is not in the target') `
    -MustNotMatch (Others-But 'check 2:') `
    -Inject {
        param($root)
        # wolf3d is reached by its own CMakeLists, so it is in neither list the
        # gate reads -- which is exactly what makes it a removal that cannot
        # have happened. The build count stays 62, so only this half reacts.
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f "'src/backend/input/be_input_controller_mappings.c' = 'altcontrol decision (same)'" `
            "'src/backend/input/be_input_controller_mappings.c' = 'altcontrol decision (same)'`n            'src/wolf3d/id_misc.c' = 'probe: never in this target'"
    }

# --- C3a: a required symbol stops being used -------------------------------
Say "C3a  a built TU stops referencing a required symbol -> check 3"
Assert-Case -Name 'a required symbol nobody uses is caught' -Tag 'c3a' -Expect 'fail' `
    -MustMatch @('check 3: recorded as required, but no built object asks for it: BE_ST_AltControlScheme_Reset') `
    -MustNotMatch (Others-But 'check 3:') `
    -Inject {
        param($root)
        # The call site, not the ledger: be_startup.c is the ONLY referencer of
        # Reset. The statement is replaced rather than deleted so the TU still
        # compiles -- a TU that failed to compile would take the other branch of
        # check 3 and prove something else.
        $f = Get-MirrorPath $root '_deps\reflectionhle-upstream\src\backend\startup\be_startup.c'
        Replace-InFile $f 'BE_ST_AltControlScheme_Reset();' '(void)0; /* probe: reference removed */'
    }

# --- C3b: a dropped symbol gains a referencer -----------------------------
Say "C3b  a dropped symbol gaining a referencer -> check 3"
Assert-Case -Name 'a dropped symbol that is used anyway is caught' -Tag 'c3b' -Expect 'fail' `
    -MustMatch @('check 3: recorded as dropped, but a built object does ask for it: BEL_ST_AltControlScheme_HandleTextInputEvent') `
    -MustNotMatch (Others-But 'check 3:') `
    -Inject {
        param($root)
        # Declared here rather than through be_input_controller_mappings.h: this
        # TU is not meant to know about altcontrol, and the point of the case is
        # the linker reference, not the prototype.
        $f = Get-MirrorPath $root '_deps\reflectionhle-upstream\src\backend\startup\be_startup.c'
        Add-Raw $f "`nextern int BEL_ST_AltControlScheme_HandleTextInputEvent;`nvoid *bel_probe_dropped_reference = &BEL_ST_AltControlScheme_HandleTextInputEvent;`n"
    }

# --- C4: the recorded stub size drifts -------------------------------------
Say "C4   the recorded .bss size drifting from the measurement -> check 4"
Assert-Case -Name 'a size record that drifted is caught' -Tag 'c4' -Expect 'fail' `
    -MustMatch @('check 4: \.bss is 55836 B, recorded 55840 B') `
    -MustNotMatch (Others-But 'check 4:') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f 'StubBssBytes = 55836' 'StubBssBytes = 55840'
    }

# --- C5: the host test's own symbol list lies ------------------------------
Say "C5   a CONTRACT symbol the stub does not define -> check 5"
Assert-Case -Name 'a lying compiled contract is caught' -Tag 'c5' -Expect 'fail' `
    -MustMatch @('check 5: the compiled stub.s symbol list disagrees with the record') `
    -MustNotMatch (Others-But 'check 5:') `
    -Inject {
        param($root)
        # Check 5 cross-checks two derivations of one fact -- the record and what
        # the compiled stub reports -- so a defect in either must be caught.
        $f = Get-MirrorPath $root 'refkeen\tests\test_altcontrol_stub.c'
        Replace-InFile $f '    puts("REFKEEN ALTCONTROL STUB: PASS");' `
            "    printf(`"CONTRACT symbol=bel_probe_lying_contract`\n`");`n    puts(`"REFKEEN ALTCONTROL STUB: PASS`");"
    }

# --- C6a: a port file reaches a replaced TU --------------------------------
Say "C6a  a port file #including a replaced TU -> check 6"
Assert-Case -Name 'an include of a replaced TU is caught' -Tag 'c6a' -Expect 'fail' `
    -MustMatch @('check 6: the port includes a replaced TU.s header') `
    -MustNotMatch (Others-But 'check 6:') `
    -Inject {
        param($root)
        # A new file on purpose, and one nothing compiles: injecting into the
        # stub itself would also break check 4's sizes and blur the attribution.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\probe_reach.c'
        Set-Raw $f "#include `"kdreams/altcontroller.c`"`n"
    }

# --- C6b: the name survives in code rather than in prose -------------------
Say "C6b  a replaced TU's name surviving in code -> check 6"
Assert-Case -Name 'a code reference (not a comment) is caught' -Tag 'c6b' -Expect 'fail' `
    -MustMatch @('check 6: the port names a replaced TU in code') `
    -MustNotMatch (Others-But 'check 6:') `
    -Inject {
        param($root)
        # Never compiled, so it does not have to be valid C -- what matters is
        # that the leaf appears in code rather than inside a comment or a string.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\probe_mention.c'
        Set-Raw $f "const char *bel_probe_altcontrol_mention = altcontroller.c;`n"
    }

# --- C7a: the removed-RAM figure drifts -----------------------------------
Say "C7a  the recorded removed-RAM figure drifting -> check 7"
Assert-Case -Name 'a drifted RAM ledger entry is caught' -Tag 'c7a' -Expect 'fail' `
    -MustMatch @('check 7: the replaced TUs. writable data measures 74927 B, recorded 74928 B') `
    -MustNotMatch (Others-But 'check 7:') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f 'RemovedRamBytes = 74927' 'RemovedRamBytes = 74928'
    }

# --- C7b: the removed-flash figure drifts ---------------------------------
Say "C7b  the recorded removed-flash figure drifting -> check 7"
Assert-Case -Name 'a drifted flash ledger entry is caught' -Tag 'c7b' -Expect 'fail' `
    -MustMatch @('check 7: the replaced TUs. flash footprint measures 68927 B, recorded 68928 B') `
    -MustNotMatch (Others-But 'check 7:') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f 'RemovedFlashBytes = 68927' 'RemovedFlashBytes = 68928'
    }

Say ""
Say "========================================================================"
if ($script:Bad -eq 0) {
    Say ("ALTCONTROL STUB SELF-TEST: PASS ({0}/{0})" -f $script:Cases)
    Flush-Out
    exit 0
}
Say ("ALTCONTROL STUB SELF-TEST: FAILED ({0} of {1} cases)" -f $script:Bad, $script:Cases)
Flush-Out
exit 1
