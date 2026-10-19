<#
.SYNOPSIS
    Negative-path self test for Test-EventSourceContract.ps1.

.DESCRIPTION
    A gate that cannot fail is worse than no gate. This proves the event source
    gate fails, and fails for the RIGHT reason: each case injects exactly one
    defect and then asserts both that the intended check fires and that the other
    checks stay quiet. The second half is what keeps the attribution honest --
    without it, "check 3 failed" could just as easily be an artifact of the
    fixture.

    C0 is the clean baseline and must pass, or every later case is
    unattributable. The fixture is a mirror of the real sources, not a
    hand-written miniature: a miniature with invented files once produced an
    always-red C0 in this project and cost a whole round to diagnose.

    Cases:

      C0   nothing injected                                      -> PASS
      C1   a third top-level export in the replaced TU            -> check 1
      C2   a port file that #includes the replaced TU's header    -> check 2
      C2b  the replaced TU's name surviving in code, not comments -> check 2
      C3   the keymap entry for a scancode the port emits, blank  -> check 3
      C4   the compiled contract disagreeing with the source      -> check 4
      C5   an implicit declaration in the replacement             -> check 5 (twice)
      C6   a second definition of shared key-repeat state         -> check 6
      C7a  NonBuiltSymbols naming a TU the port builds            -> check 7
      C7b  NamedBuiltGlobals naming a symbol nobody names         -> check 7

    Two cases deliberately split one check's two halves, because the halves fail
    for different reasons and a single case would prove only one of them:

      * C2 vs C2b. Check 2 must catch reachability (an include, which pulls real
        SDL in) and must NOT treat documentation as a defect. C2b proves the
        code reference is caught; the 13 prose and 1 message-literal mentions in
        the real tree are what proves comments are not.
      * C7a vs C7b. The ledger has a negative and a positive direction. C7a
        injects a wrong OWNER (the mistake that was actually made: two of eight
        entries named symbols a built TU defines), C7b injects a stale POSITIVE
        claim.

    C5 trips two messages inside check 5: the diagnostic multiset changes because
    an implicit declaration IS a new warning class, and the stricter
    -Werror=implicit-function-declaration arm rejects it. The case asserts both
    rather than pretending one suffices.

    C3 does not touch the port at all: it blanks the upstream keymap entry for a
    scancode the port emits, which is the defect the check exists for. Note that
    it cannot be simulated by renumbering the shim, because the compiled contract
    in check 4 would follow the shim and stay consistent.

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
    $ReportPath = Join-Path $Workspace ('.emv2\logs\s20-event-source-selftest-{0}.txt' -f $RunId)
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
$gateName = 'Test-EventSourceContract.ps1'
$gateReal = Join-Path $PSScriptRoot $gateName

# Enough of upstream for the event source's own includes, the keymap, the three
# TUs whose ownership check 7 looks up, and the target compile. id91_11 (3.3 MB),
# wolf3d (2.0 MB) and the rest are not reachable from this seam. If a needed
# header ever lives in a skipped directory, C0 goes red and says so.
$mirrorSubdirs = @('backend', 'kdreams', 'crc32', 'depklite', 'unlzexe', 'fonts', 'idtech_common', 'nukedopl')

function New-Mirror([string]$Tag) {
    $root = Join-Path $Workspace ("_deps\event-source-self-{0}-{1}" -f $RunId, $Tag)
    $upDst = Join-Path $root '_deps\reflectionhle-upstream'
    $srcDst = Join-Path $upDst 'src'
    New-Item -ItemType Directory -Force -Path $srcDst | Out-Null
    Copy-Item -Path (Join-Path $realUpstream 'src\*.h') -Destination $srcDst -Force
    foreach ($d in $mirrorSubdirs) {
        $from = Join-Path $realUpstream ('src\' + $d)
        if (Test-Path -LiteralPath $from) {
            Copy-Item -LiteralPath $from -Destination $srcDst -Recurse -Force
        }
    }

    $refDst = Join-Path $root 'refkeen'
    New-Item -ItemType Directory -Force -Path $refDst | Out-Null
    # host carries the shim and the port config header; platform carries the
    # replacement and the scancode table; tests carries the host test that check 4
    # compiles and that C4 has to make lie.
    foreach ($sub in @('host', 'platform', 'tests')) {
        Copy-Item -LiteralPath (Join-Path $Workspace ('refkeen\' + $sub)) -Destination $refDst -Recurse -Force
    }
    $scDst = Join-Path $refDst 'scripts'
    New-Item -ItemType Directory -Force -Path $scDst | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1') -Destination $scDst -Force
    Copy-Item -LiteralPath $gateReal -Destination $scDst -Force

    # compat/ is the device boundary the event source reads, and check 3 parses
    # its channel enum. Without it the mirror is not a mirror.
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
            if ($line -match '^\s*(FAIL|EVENT SOURCE)') { Say ("          | " + $line.Trim()) }
        }
    }
}

Say "S20 EVENT SOURCE GATE -- NEGATIVE PATH SELF TEST"
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
Say "C0  clean mirror of the real sources -> the gate must PASS"
Assert-Case -Name 'clean mirror passes' -Tag 'c0' -Expect 'pass' -Inject $null `
    -MustMatch @('EVENT SOURCE: PASS \(7 checks\)', 'Check 7 .*ledger')

# --- C1: the replaced TU grows a third export ------------------------------
Say "C1  a third top-level export in the replaced TU -> check 1"
Assert-Case -Name 'export drift is caught' -Tag 'c1' -Expect 'fail' `
    -MustMatch @('check 1: the replaced TU.s unconditional exports moved') `
    -MustNotMatch (Others-But 'check 1:') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root '_deps\reflectionhle-upstream\src\backend\events\be_events_sdl.c'
        Add-Raw $f "`nvoid bel_probe_extra_export(void)`n{`n}`n"
    }

# --- C2: a port file reaches the replaced TU's header ----------------------
Say "C2  a port file that #includes the replaced TU's header -> check 2"
Assert-Case -Name 'an include of the replaced header is caught' -Tag 'c2' -Expect 'fail' `
    -MustMatch @('check 2: the port includes the replaced TU.s header') `
    -MustNotMatch (Others-But 'check 2:') `
    -Inject {
        param($root)
        # A new file on purpose. Injection into the replacement itself would also
        # break the target compile in check 5 and blur the attribution.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\probe_reach.c'
        Set-Raw $f "#include `"backend/events/be_events_sdl.h`"`n"
    }

# --- C2b: the same name, but in code rather than in prose ------------------
Say "C2b the replaced TU's name surviving in code -> check 2"
Assert-Case -Name 'a code reference (not a comment) is caught' -Tag 'c2b' -Expect 'fail' `
    -MustMatch @('check 2: the port names the replaced TU or its header in code') `
    -MustNotMatch (Others-But 'check 2:') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\probe_mention.c'
        Set-Raw $f "const char *bel_probe_mention = be_events_sdl.c;`n"
    }

# --- C3: a scancode the port emits is dead in upstream's keymap ------------
Say "C3  the keymap entry for a scancode the port emits, blanked -> check 3"
Assert-Case -Name 'a dead scancode is caught' -Tag 'c3' -Expect 'fail' `
    -MustMatch @('check 3: channel LEFT emits SDL scancode 80, which upstream.s keymap leaves EMPTY') `
    -MustNotMatch (Others-But 'check 3:') `
    -Inject {
        param($root)
        # LEFT is channel 0 and reaches SDL_SCANCODE_LEFT = 80. Blanking that
        # keymap slot leaves the channel pointing at nothing.
        $f = Get-MirrorPath $root '_deps\reflectionhle-upstream\src\backend\events\be_events_sdl_keymap.h'
        Replace-InFile $f '{true,  BE_ST_SC_LEFT}, // SDL_SCANCODE_LEFT' 'emptyDOSKeyEvent, // SDL_SCANCODE_LEFT'
    }

# --- C4: the compiled contract and the source disagree ---------------------
Say "C4  the compiled contract disagreeing with the source -> check 4"
Assert-Case -Name 'a lying compiled contract is caught' -Tag 'c4' -Expect 'fail' `
    -MustMatch @('check 4: channel LEFT: compiled=999') `
    -MustNotMatch (Others-But 'check 4:') `
    -Inject {
        param($root)
        # Check 4 cross-checks two derivations of one fact, so a defect in either
        # one must be caught. Here the module reports a value the source does not
        # contain.
        $f = Get-MirrorPath $root 'refkeen\tests\test_event_source.c'
        Replace-InFile $f '    puts("REFKEEN EVENT SOURCE: PASS");' `
            "    printf(`"CONTRACT channel=LEFT scancode=999\n`");`n    puts(`"REFKEEN EVENT SOURCE: PASS`");"
    }

# --- C5: an implicit declaration in the replacement ------------------------
Say "C5  an implicit declaration in the replacement -> check 5 (both arms)"
Assert-Case -Name 'an implicit declaration is caught' -Tag 'c5' -Expect 'fail' `
    -MustMatch @('check 5: diagnostic set changed',
                 'check 5: the replacement has an implicit function declaration') `
    -MustNotMatch (Others-But 'check 5:') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\be_events_port.c'
        Add-Raw $f "`nvoid retropicoes_refkeen_probe_implicit(void)`n{`n`tbel_probe_undeclared(1);`n}`n"
    }

# --- C6: a second copy of the shared repeat state --------------------------
Say "C6  a second definition of shared key-repeat state -> check 6"
Assert-Case -Name 'a forked repeat state is caught' -Tag 'c6' -Expect 'fail' `
    -MustMatch @('check 6: the replacement defines shared key-repeat state') `
    -MustNotMatch (Others-But 'check 6:') `
    -Inject {
        param($root)
        # A tentative definition: compiles cleanly, which is exactly why the fork
        # would otherwise survive to the link.
        $f = Get-MirrorPath $root 'refkeen\platform\retropicoes\be_events_port.c'
        Add-Raw $f "`nuint32_t g_sdlEmuKeyboardLastScanCodePressTime;`n"
    }

# --- C7a: the ledger claims an owner the port builds -----------------------
Say "C7a NonBuiltSymbols naming a TU the port builds -> check 7"
Assert-Case -Name 'a wrong owner in the ledger is caught' -Tag 'c7a' -Expect 'fail' `
    -MustMatch @('check 7: these are defined in TU\(s\) the port DOES build') `
    -MustNotMatch (Others-But 'check 7:') `
    -Inject {
        param($root)
        # The real mistake, reproduced: an entry whose owner is a portable TU.
        # The symbol is deliberately one the replacement does not name, so only
        # the ownership half of the ledger reacts.
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f "'g_sdlLastReportedWindowWidth'  = 'src/backend/video/be_video.c'" `
            "'g_sdlLastReportedWindowWidth'  = 'src/backend/video/be_video.c'`n            'g_be_audioMainThread_lastCallTicks' = 'src/backend/audio/be_audio_main_thread.c'"
    }

# --- C7b: the ledger's positive claim has gone stale ----------------------
Say "C7b NamedBuiltGlobals naming a symbol nobody names -> check 7"
Assert-Case -Name 'a stale positive entry is caught' -Tag 'c7b' -Expect 'fail' `
    -MustMatch @('check 7: the ledger records g_sdlInputbindStates as named by the replacement, but it is not named') `
    -MustNotMatch (Others-But 'check 7:') `
    -Inject {
        param($root)
        $f = Get-MirrorPath $root 'refkeen\scripts\BoundaryFacts.psd1'
        Replace-InFile $f "'g_sdlLastPollEventsTime' = 'src/backend/input/be_input.c'" `
            "'g_sdlLastPollEventsTime' = 'src/backend/input/be_input.c'`n            'g_sdlInputbindStates' = 'src/backend/input/be_input.c'"
    }

Say ""
Say "========================================================================"
if ($script:Bad -eq 0) {
    Say ("EVENT SOURCE SELF-TEST: PASS ({0}/{0})" -f $script:Cases)
    Flush-Out
    exit 0
}
Say ("EVENT SOURCE SELF-TEST: FAILED ({0} of {1} cases)" -f $script:Bad, $script:Cases)
Flush-Out
exit 1
