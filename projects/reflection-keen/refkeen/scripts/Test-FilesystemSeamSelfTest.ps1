<#
.SYNOPSIS
    Negative-path self test for Test-FilesystemSeamContract.ps1.

.DESCRIPTION
    A gate that cannot fail is worse than no gate. This proves the filesystem
    seam gate fails, and fails for the RIGHT reason: each case injects exactly
    one defect and then asserts both that the intended check fires and that the
    other checks stay quiet. That second half is the part that keeps the
    attribution honest -- without it, "check 3 failed" could just as easily be
    an artifact of the fixture.

    C0 is the clean baseline. It must pass, or every later case is
    unattributable: a red C0 means the fixture itself is wrong, and a case's
    failure then says nothing about the gate. The fixture is therefore a
    byte-for-byte mirror of the real sources, not a hand-written miniature:
    a miniature with 9 invented files once produced an always-red C0 and cost a
    whole round to diagnose.

    Cases:

      C0  nothing injected                                  -> PASS
      C1  a new unclassified .c in src/backend/filesystem/   -> check 1
      C2  a mention of an excluded symbol, inside a comment  -> check 2
      C3  an unused variable in a retained TU                -> check 3
      C4  an implicit declaration in a retained TU           -> check 4 (and 3)
      C5  a retained TU calling BEL_Cross_IsDir              -> check 5
      C6  a new BEL_Cross_mkdir call site                    -> check 6

    C2 needs a comment on purpose: the check is textual, and a comment proves
    the check reads the source rather than "did the compile change". C4 cannot
    avoid also tripping check 3, because an implicit declaration is itself a new
    warning class in the frozen baseline; the case asserts both rather than
    pretending otherwise.

.PARAMETER RunId
    Suffix for the scratch trees. Nothing is ever deleted and re-created: the
    project's sandbox redirects recursive deletes to a fail-closed trash, so
    every run gets brand new directories instead. Each case copies about 2.6 MB
    of upstream sources, so scratch usage is real but bounded.
#>
[CmdletBinding()]
param(
    [string]$Workspace = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$TargetGcc = 'F:\Download\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe',
    [string]$RunId = '',
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $RunId) { $RunId = ('{0:yyyyMMddHHmmss}' -f (Get-Date)) }
if (-not $ReportPath) {
    $ReportPath = Join-Path $Workspace ('.emv2\logs\s20-fs-seam-selftest-{0}.txt' -f $RunId)
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
$gateName = 'Test-FilesystemSeamContract.ps1'
$gateReal = Join-Path $PSScriptRoot $gateName

# Only what a filesystem TU actually needs. id91_11 (3.3 MB), wolf3d (2.0 MB),
# compat, android-project and the rest are not reachable from the filesystem
# seam, and copying them seven times would cost 40 MB per run for nothing. If a
# needed header ever lives in a skipped directory, C0 goes red and says so.
$mirrorSubdirs = @('backend', 'kdreams', 'crc32', 'depklite', 'unlzexe', 'fonts', 'idtech_common', 'nukedopl')

function New-Mirror([string]$Tag) {
    $root = Join-Path $Workspace ("_deps\fs-seam-self-{0}-{1}" -f $RunId, $Tag)
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
    Copy-Item -LiteralPath (Join-Path $Workspace 'refkeen\host') -Destination $refDst -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $Workspace 'refkeen\platform') -Destination $refDst -Recurse -Force
    $scDst = Join-Path $refDst 'scripts'
    New-Item -ItemType Directory -Force -Path $scDst | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1') -Destination $scDst -Force
    Copy-Item -LiteralPath $gateReal -Destination $scDst -Force
    return $root
}

function Get-MirrorFsDir([string]$Root) {
    return (Join-Path $Root '_deps\reflectionhle-upstream\src\backend\filesystem')
}

# Append raw ASCII text. Add-Content with an explicit encoding keeps the file
# free of a BOM, so the compiler sees exactly what upstream's file would be plus
# the injection.
function Add-Raw([string]$Path, [string]$Text) {
    Add-Content -LiteralPath $Path -Value $Text -Encoding ASCII -NoNewline
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
                -TargetGcc $TargetGcc -ReportPath $rep | Out-Null
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
            if ($line -match '^\s*(FAIL|FILESYSTEM SEAM)') { Say ("          | " + $line.Trim()) }
        }
    }
}

Say "S20 FILESYSTEM SEAM GATE -- NEGATIVE PATH SELF TEST"
Say ("Workspace : {0}" -f $Workspace)
Say ("Gate      : {0}" -f $gateReal)
Say ("Run id    : {0}" -f $RunId)
Say ""

if (-not (Test-Path -LiteralPath $realUpstream -PathType Container)) {
    Say "upstream checkout not found -- cannot mirror a fixture"
    Flush-Out
    exit 2
}

# --- C0: the clean mirror must pass ----------------------------------------
Say "C0  clean mirror of the real sources -> the gate must PASS"
Assert-Case -Name 'clean mirror passes' -Tag 'c0' -Expect 'pass' -Inject $null `
    -MustMatch @('FILESYSTEM SEAM: PASS \(6 checks\)', 'Check 3 .*frozen set')

# --- C1: an unclassified filesystem TU ------------------------------------
Say "C1  a new .c in src/backend/filesystem/ -> check 1"
Assert-Case -Name 'unclassified TU is caught' -Tag 'c1' -Expect 'fail' -MustMatch @('check 1: .*not classified') `
    -MustNotMatch @('check 2:', 'check 4:', 'check 5:', 'check 6:') `
    -Inject {
        param($root)
        $f = Join-Path (Get-MirrorFsDir $root) 'be_filesystem_probe.c'
        Add-Raw $f "#include `"be_filesystem_len_bounds.h`"`nint bel_fs_seam_probe;`n"
    }

# --- C2: a mention of an excluded symbol ----------------------------------
Say "C2  an excluded symbol named by a retained TU -> check 2"
Assert-Case -Name 'excluded symbol leak is caught' -Tag 'c2' -Expect 'fail' `
    -MustMatch @('check 2: .*BE_Cross_DirSelection_') `
    -MustNotMatch @('check 1:', 'check 3:', 'check 4:', 'check 5:', 'check 6:') `
    -Inject {
        param($root)
        $f = Join-Path (Get-MirrorFsDir $root) 'be_filesystem_file_ops.c'
        Add-Raw $f "`n/* probe: the port must never reach BE_Cross_DirSelection_Start */`n"
    }

# --- C3: an extra warning in a retained TU --------------------------------
Say "C3  a new warning class in a retained TU -> check 3"
Assert-Case -Name 'diagnostic drift is caught' -Tag 'c3' -Expect 'fail' `
    -MustMatch @('check 3: be_filesystem_mods\.c diagnostics changed') `
    -MustNotMatch @('check 1:', 'check 2:', 'check 4:', 'check 5:', 'check 6:') `
    -Inject {
        param($root)
        $f = Join-Path (Get-MirrorFsDir $root) 'be_filesystem_mods.c'
        Add-Raw $f "`nstatic void bel_fs_seam_probe(void)`n{`n`tint unused_probe = 1;`n}`n"
    }

# --- C4: an implicit declaration ------------------------------------------
Say "C4  an implicit declaration in a retained TU -> check 4 (check 3 too)"
Assert-Case -Name 'implicit declaration is caught' -Tag 'c4' -Expect 'fail' `
    -MustMatch @('check 4: these TUs rely on an implicit declaration',
                 'check 3: be_filesystem_mods\.c diagnostics changed') `
    -MustNotMatch @('check 1:', 'check 2:', 'check 5:', 'check 6:') `
    -Inject {
        param($root)
        $f = Join-Path (Get-MirrorFsDir $root) 'be_filesystem_mods.c'
        Add-Raw $f "`nvoid bel_fs_seam_probe_call(void)`n{`n`tbel_undeclared_probe(1);`n}`n"
    }

# --- C5: a retained TU reaches the empty-body inline ----------------------
Say "C5  a retained TU calling BEL_Cross_IsDir -> check 5"
Assert-Case -Name 'a live IsDir caller is caught' -Tag 'c5' -Expect 'fail' `
    -MustMatch @('check 5: a RETAINED TU reaches BEL_Cross_IsDir',
                 'check 5: direct callers moved') `
    -MustNotMatch @('check 1:', 'check 2:', 'check 3:', 'check 4:', 'check 6:') `
    -Inject {
        param($root)
        $f = Join-Path (Get-MirrorFsDir $root) 'be_filesystem_file_ops.c'
        Add-Raw $f "`nbool bel_fs_seam_probe_isdir(const TCHAR *path)`n{`n`treturn BEL_Cross_IsDir(path);`n}`n"
    }

# --- C6: a new mkdir call site --------------------------------------------
Say "C6  a new BEL_Cross_mkdir call site -> check 6"
Assert-Case -Name 'a new mkdir caller is caught' -Tag 'c6' -Expect 'fail' `
    -MustMatch @('check 6: BEL_Cross_mkdir call sites changed') `
    -MustNotMatch @('check 1:', 'check 2:', 'check 3:', 'check 4:', 'check 5:') `
    -Inject {
        param($root)
        $f = Join-Path (Get-MirrorFsDir $root) 'be_filesystem_mods.c'
        Add-Raw $f "`n#include `"be_filesystem_mkdir.h`"`nvoid bel_fs_seam_probe_mkdir(void)`n{`n`tBEL_Cross_mkdir(`"/probe`");`n}`n"
    }

Say ""
Say "========================================================================"
if ($script:Bad -eq 0) {
    Say ("FILESYSTEM SEAM SELF-TEST: PASS ({0}/{0})" -f $script:Cases)
    Flush-Out
    exit 0
}
Say ("FILESYSTEM SEAM SELF-TEST: FAILED ({0} of {1} cases)" -f $script:Bad, $script:Cases)
Flush-Out
exit 1
