<#
.SYNOPSIS
    S20-C filesystem seam gate -- the target-side view of the filesystem TUs.

.DESCRIPTION
    Answers one question: does the port take the filesystem seam the way
    filesystem-seam.md says it does, and is the port's own build configuration
    free of the two traps that would silently break it?

    Every fact here was measured, and two of them overturned an earlier record:

      * The target compiles upstream's filesystem TUs unchanged. The
        "too many arguments to mkdir" error that S20-B recorded as a port
        requirement only happens on the HOST, because MinGW's sys/stat.h goes
        through io.h and declares MSVCRT's one-argument mkdir. newlib declares
        the two-argument POSIX form. So this gate runs on arm-none-eabi: the
        host build is the wrong instrument for this question.

      * BE_FILESYSTEM_DIR_H KEEPS TWO PLATFORM ARMS AND NOTHING ELSE. A
        platform that defines neither gets an EMPTY BODY out of
        BEL_Cross_IsDir -- undefined behaviour, not a compile error. The port
        is such a platform, so this gate proves the function is dead code
        rather than pretending the upstream defect does not exist.

    Checks:

      1. every .c under src/backend/filesystem/ is classified exactly once
      2. the excluded TUs' exported symbols are referenced nowhere
      3. each retained TU compiles on the target, and its diagnostic multiset
         equals the frozen one (a new or vanished warning is a change)
      4. -Werror=implicit-function-declaration changes nothing -- that one
         class is never tolerated upstream
      5. BEL_Cross_IsDir: upstream still has exactly two arms, neither defined
         by the port, and nothing in the built set calls it
      6. BEL_Cross_mkdir: still returns void, and its call sites are the known
         four, so no caller can depend on the directory being created

    Check 3 is a frozen-diagnostic baseline, not a "warnings are fine" pass:
    upstream is not -Wall-clean on a platform it was never built for, and the
    completed ports answer that by keeping -Werror for their own TUs only
    (xrick, digger and sdlpop all set -Wall;-Wextra;-Werror on port sources,
    and force just -Werror=implicit-function-declaration target-wide). Freezing
    the set keeps upstream's known noise visible instead of exempting a
    warning class.

.PARAMETER SelfTest
    Hand off to the companion script Test-FilesystemSeamSelfTest.ps1, which
    mirrors the real sources into a scratch tree, confirms the clean copy
    passes, then injects one defect per check and requires exactly that check to
    fire. This switch was declared and then never read until 2026-09-23, so
    "-SelfTest" ran the ordinary gate and printed PASS -- a negative-path test
    that could not fail because it never ran.

.PARAMETER RunId
    Suffix for the scratch trees. Nothing is ever deleted and re-created: the
    project's sandbox redirects recursive deletes to a fail-closed trash, so
    every run gets a brand new directory instead.
#>
[CmdletBinding()]
param(
    [string]$Workspace = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$UpstreamRoot,
    [string]$TargetGcc,
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
    $companion = Join-Path $PSScriptRoot 'Test-FilesystemSeamSelfTest.ps1'
    $forward = @{ Workspace = $Workspace }
    if ($TargetGcc)  { $forward['TargetGcc']  = $TargetGcc }
    if ($RunId)      { $forward['RunId']      = $RunId }
    if ($ReportPath) { $forward['ReportPath'] = $ReportPath }
    & $companion @forward
    exit $LASTEXITCODE
}

if (-not $UpstreamRoot) {
    $UpstreamRoot = Join-Path $Workspace '_deps\reflectionhle-upstream'
}
if (-not $TargetGcc) {
    $TargetGcc = 'F:\Download\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe'
}
if (-not $ReportPath) {
    $ReportPath = Join-Path $Workspace '.emv2\logs\s20-filesystem-seam.txt'
}

$script:Lines = New-Object System.Collections.Generic.List[string]
$script:Problems = New-Object System.Collections.Generic.List[string]

function Emit([string]$text) { [void]$script:Lines.Add($text) }
function Fail([string]$text) { [void]$script:Problems.Add($text); Emit ("  FAIL  " + $text) }

function Flush-Report {
    $dir = Split-Path -Parent $ReportPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $script:Lines | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

# Invoke the compiler without letting its native stderr become a terminating
# error. With $ErrorActionPreference = 'Stop', PowerShell 5.1 escalates native
# stderr to a terminating error, which would abort this script on the first TU
# that is merely *expected* to emit a warning. This gate reads diagnostics, so
# every compile goes through here.
function Invoke-Native([string]$Exe, [string[]]$Arguments) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        # -Width matters: Out-String defaults to the console width (often 120)
        # and FOLDS long lines, which cuts compiler diagnostics mid-path.
        $out = (& $Exe @Arguments 2>&1 | Out-String -Width 4096)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Rc = $rc; Out = $out }
}

# Warning classes as a sorted multiset, e.g. @('-Wreturn-type=1',
# '-Wunused-function=1'). Message wording is deliberately not part of this:
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

# ---------------------------------------------------------------------------

$facts = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1')
$seam = $facts.FilesystemSeam
$fsRel = 'src/backend/filesystem'

function Get-PortText {
    $root = Join-Path $Workspace 'refkeen'
    if (-not (Test-Path -LiteralPath $root)) { return '' }
    $files = @(Get-ChildItem -LiteralPath $root -Recurse -File -Include '*.c', '*.h' -ErrorAction SilentlyContinue)
    $sb = New-Object System.Text.StringBuilder
    foreach ($f in $files) {
        if ($f.FullName -match '\\scripts\\') { continue }
        [void]$sb.AppendLine((Get-Content -LiteralPath $f.FullName -Raw))
    }
    return $sb.ToString()
}

function Get-RetainedText {
    $sb = New-Object System.Text.StringBuilder
    foreach ($tu in $seam.RetainedTus) {
        $p = Join-Path $UpstreamRoot ($fsRel -replace '/', '\')
        $p = Join-Path $p $tu
        if (Test-Path -LiteralPath $p -PathType Leaf) {
            [void]$sb.AppendLine((Get-Content -LiteralPath $p -Raw))
        }
    }
    return $sb.ToString()
}

$script:BlankNonNewlines = [System.Text.RegularExpressions.MatchEvaluator]{
    param($m) ($m.Value -replace '[^\r\n]', ' ')
}

function Get-CodeView([string]$text) {
    $view = [regex]::Replace(
        $text, '/\*.*?\*/', $script:BlankNonNewlines, 'Singleline')
    $view = [regex]::Replace(
        $view, '//[^\r\n]*', $script:BlankNonNewlines)
    return [regex]::Replace(
        $view, '"(?:[^"\\]|\\.)*"', $script:BlankNonNewlines)
}

$targetOk = Test-Path -LiteralPath $TargetGcc -PathType Leaf
$defs = @(
    '-DREFKEEN_PLATFORM_PICO=1'
    '-DREFKEEN_VER_KDREAMS=1'
    '-DREFKEEN_HAS_VER_KDREAMS=1'
)
$incs = @('-I', (Join-Path $Workspace 'refkeen\host\shim'),
          '-I', (Join-Path $Workspace 'refkeen\host\include'),
          '-I', "$UpstreamRoot\src",
          '-I', "$UpstreamRoot\src\crc32",
          '-I', "$UpstreamRoot\src\depklite",
          '-I', "$UpstreamRoot\src\unlzexe",
          '-I', "$UpstreamRoot\src\kdreams",
          '-I', "$UpstreamRoot\src\kdreams\lscr")

Emit "S20 FILESYSTEM SEAM GATE"
Emit ("workspace     : {0}" -f $Workspace)
Emit ("upstream      : {0}" -f $UpstreamRoot)
Emit ("target gcc    : {0}" -f $TargetGcc)
if ($RunId) { Emit ("run id        : {0}" -f $RunId) }
Emit ""

if (-not (Test-Path -LiteralPath $UpstreamRoot -PathType Container)) {
    Emit "upstream checkout not found -- loud skip"
    Flush-Report
    exit 2
}
if (-not $targetOk) {
    Emit ("target compiler not found: {0}" -f $TargetGcc)
    Emit "This gate must run on the target toolchain; the host is the wrong instrument here."
    Flush-Report
    exit 2
}

$fsDir = Join-Path $UpstreamRoot ($fsRel -replace '/', '\')

# --- Check 1: classification is complete and not stale ---------------------
Emit "Check 1  every filesystem .c is classified exactly once"
$actualTus = @(Get-ChildItem -LiteralPath $fsDir -File -Filter '*.c' | ForEach-Object { $_.Name })
$declared = @{}
foreach ($tu in $seam.RetainedTus) { $declared[$tu] = 'retained' }
foreach ($tu in $seam.ReplacedTus) { $declared[$tu] = 'replaced' }
foreach ($tu in @($seam.ExcludedTus.Keys)) { $declared[$tu] = 'excluded' }
$unclassified = @($actualTus | Where-Object { -not $declared.ContainsKey($_) })
$stale = @($declared.Keys | Where-Object { $actualTus -cnotcontains $_ })
if ($unclassified.Count -gt 0) {
    Fail ("check 1: {0} new filesystem TU(s) are not classified: {1}" -f `
          $unclassified.Count, ($unclassified -join ', '))
} elseif ($stale.Count -gt 0) {
    Fail ("check 1: facts list {0} name(s) upstream no longer has: {1}" -f `
          $stale.Count, ($stale -join ', '))
} else {
    Emit ("  ok    {0} TU(s): {1} retained, {2} replaced, {3} excluded" -f `
          $actualTus.Count, $seam.RetainedTus.Count, $seam.ReplacedTus.Count, $seam.ExcludedTus.Count)
}
Emit ""

# --- Check 2: excluded symbols are referenced nowhere ---------------------
Emit "Check 2  the excluded TUs' symbols are referenced nowhere"
$builtText = (Get-RetainedText) + "`n" + (Get-PortText)
$builtCode = Get-CodeView $builtText
$hits = New-Object System.Collections.Generic.List[string]
foreach ($sym in $seam.ExcludedSymbols) {
    if ($builtText.Contains($sym)) { [void]$hits.Add($sym) }
}
if ($hits.Count -gt 0) {
    Fail ("check 2: excluded symbol(s) still referenced by the built set: {0}" -f ($hits -join ', '))
} else {
    Emit ("  ok    none of {0} excluded symbol prefix(es) appear in the retained TUs or the port" -f `
          $seam.ExcludedSymbols.Count)
}
Emit ""

# --- Check 3: target compile + frozen diagnostics -------------------------
Emit "Check 3  retained TUs compile on the target, diagnostics equal the frozen set"
$measured = @{}
foreach ($tu in $seam.RetainedTus) {
    $src = Join-Path $fsDir $tu
    $obj = Join-Path $env:TEMP ("fsseam-" + ($tu -replace '\.c$', '.o'))
    $r = Invoke-Native $TargetGcc (@($incs) + @($defs) + @('-c', '-std=c99', '-Wall', '-Wextra', '-fmessage-length=0', '-o', $obj, $src))
    $set = Get-DiagnosticSet $r.Out
    $measured[$tu] = $set
    if ($r.Rc -ne 0) {
        Fail ("check 3: {0} does not compile on the target (rc={1})" -f $tu, $r.Rc)
        foreach ($e in @(($r.Out -split "`r?`n") | Where-Object { $_ -match ':\d+:\d+: (error|fatal error)' } | Select-Object -First 3)) {
            Emit ("        " + $e.Trim())
        }
        continue
    }
    $frozen = @($seam.FrozenDiagnostics[$tu])
    if (($set -join '|') -cne ($frozen -join '|')) {
        Fail ("check 3: {0} diagnostics changed" -f $tu)
        Emit ("        frozen  : " + (($frozen | ForEach-Object { $_ }) -join ' '))
        Emit ("        measured: " + (($set | ForEach-Object { $_ }) -join ' '))
    }
}
if ($script:Problems.Count -eq 0) {
    Emit ("  ok    all {0} retained TU(s) compile, diagnostics match the frozen baseline" -f $seam.RetainedTus.Count)
    foreach ($tu in $seam.RetainedTus) {
        Emit ("        {0,-28} {1}" -f $tu, (($measured[$tu] | ForEach-Object { $_ }) -join ' '))
    }
}
Emit ""

# --- Check 4: implicit declarations are never tolerated -------------------
Emit "Check 4  -Werror=implicit-function-declaration changes nothing"
$changed = New-Object System.Collections.Generic.List[string]
foreach ($tu in $seam.RetainedTus) {
    $src = Join-Path $fsDir $tu
    $obj = Join-Path $env:TEMP ("fsseam-ifd-" + ($tu -replace '\.c$', '.o'))
    $r = Invoke-Native $TargetGcc (@($incs) + @($defs) + @('-c', '-std=c99', '-Wall', '-Wextra',
        '-Werror=implicit-function-declaration', '-fmessage-length=0', '-o', $obj, $src))
    if ($r.Rc -ne 0) { [void]$changed.Add($tu) }
}
if ($changed.Count -gt 0) {
    Fail ("check 4: these TUs rely on an implicit declaration: {0}" -f ($changed -join ', '))
} else {
    Emit "  ok    every retained TU still compiles with implicit declarations treated as errors"
}
Emit ""

# --- Check 5: BEL_Cross_IsDir is dead code -------------------------------
Emit "Check 5  BEL_Cross_IsDir is dead code, and still has exactly two arms"
$dirH = Get-Content -LiteralPath (Join-Path $fsDir 'be_filesystem_dir.h') -Raw
$body = [regex]::Match($dirH, 'static inline bool BEL_Cross_IsDir[\s\S]*?\n\}')
if (-not $body.Success) {
    Fail "check 5: BEL_Cross_IsDir is no longer a static inline in be_filesystem_dir.h -- re-measure before trusting anything here"
} else {
    $arms = @([regex]::Matches($body.Value, 'REFKEEN_PLATFORM_[A-Z]+') | ForEach-Object { $_.Value } | Sort-Object -Unique)
    if (($arms -join '|') -cne (($seam.IsDirPlatformArms | Sort-Object) -join '|')) {
        Fail ("check 5: BEL_Cross_IsDir arms changed: {0} (frozen: {1})" -f `
              ($arms -join ','), ($seam.IsDirPlatformArms -join ','))
    }
    if ($body.Value -match 'REFKEEN_PLATFORM_PICO') {
        Fail "check 5: BEL_Cross_IsDir now has a Pico arm -- the port would no longer need it to be dead, so re-derive the seam"
    }
    if ($builtCode.Contains('BEL_Cross_IsDir')) {
        Fail "check 5: something in the built set calls BEL_Cross_IsDir; its body is empty for this platform"
    }
    # Dead through two links, and both are measured (see BoundaryFacts.psd1):
    # the direct .c callers, and the callers of the wrapper that inlines it.
    # The second link exists because app_paths.c does NOT name BEL_Cross_IsDir
    # -- it calls BEL_Cross_AddRootPathIfDir, so a one-link check would have
    # looked at the wrong file.
    $foundCallers = @(Get-ChildItem -LiteralPath $fsDir -File -Filter '*.c' |
        Where-Object { (Get-Content -LiteralPath $_.FullName -Raw) -match 'BEL_Cross_IsDir' } |
        ForEach-Object { $_.Name })
    # Compare precomputed strings, never
    #   (A | Sort-Object) -join '|' -cne (B | Sort-Object) -join '|'
    # -- that is a trap: binary -join binds LOOSER than the comparison
    # operators. Parsed with Parser::ParseInput() the expression above comes
    # out as  (((A -join '|') -cne B) -join '|'), i.e. the trailing -join
    # applies to the comparison's RESULT, stringifying the boolean into
    # "True" -- a non-empty string, which is always true. The first version of
    # this gate wrote exactly that and printed identical values on both sides
    # while reporting them as different.
    $declaredCallers = @($seam.IsDirDirectCallers)
    $gotText = (($foundCallers | Sort-Object) -join '|')
    $expText = (($declaredCallers | Sort-Object) -join '|')
    if ($gotText -cne $expText) {
        Fail ("check 5: direct callers moved: got {0}, expected {1}" -f `
              ($gotText -replace '\|', ','), ($expText -replace '\|', ','))
    }
    $wrapper = [string]$seam.IsDirWrapperSymbol
    $wrapperCallers = @(Get-ChildItem -LiteralPath $fsDir -File -Filter '*.c' |
        Where-Object { (Get-Content -LiteralPath $_.FullName -Raw).Contains($wrapper + '(') } |
        ForEach-Object { $_.Name })
    $declaredWrapper = @($seam.IsDirWrapperCallers)
    $gotW = (($wrapperCallers | Sort-Object) -join '|')
    $expW = (($declaredWrapper | Sort-Object) -join '|')
    if ($gotW -cne $expW) {
        Fail ("check 5: {0} callers moved: got {1}, expected {2}" -f `
              $wrapper, ($gotW -replace '\|', ','), ($expW -replace '\|', ','))
    }
    # A replaced TU may name it: that TU is not built. Only retained TUs matter,
    # and the port's own tree matters too.
    $leaked = @(@($foundCallers) + @($wrapperCallers) |
        Where-Object { $seam.RetainedTus -ccontains $_ } | Sort-Object -Unique)
    if ($leaked.Count -gt 0) {
        Fail ("check 5: a RETAINED TU reaches BEL_Cross_IsDir: {0}" -f ($leaked -join ', '))
    }
    if ($script:Problems.Count -eq 0) {
        Emit ("  ok    only arms are {0}; the port defines neither" -f ($arms -join ' + '))
        Emit ("  ok    named directly only by {0}, which is excluded" -f (($foundCallers | Sort-Object) -join ' + '))
        Emit ("  ok    {0}() is called only from {1}, which the port replaces" -f `
              $wrapper, (($wrapperCallers | Sort-Object) -join ' + '))
        Emit "  ok    0 references from the retained TUs or the port tree"
    }
}
Emit ""

# --- Check 6: BEL_Cross_mkdir cannot be depended upon ---------------------
Emit "Check 6  BEL_Cross_mkdir returns void, and its call sites are the known four"
$mkdirH = Get-Content -LiteralPath (Join-Path $fsDir 'be_filesystem_mkdir.h') -Raw
$decl = [regex]::Match($mkdirH, 'static inline\s+([A-Za-z_][A-Za-z0-9_ ]*?)\s+BEL_Cross_mkdir\s*\(')
if (-not $decl.Success) {
    Fail "check 6: cannot read BEL_Cross_mkdir's declaration"
} elseif ($decl.Groups[1].Value.Trim() -cne 'void') {
    Fail ("check 6: BEL_Cross_mkdir now returns '{0}'; a caller may depend on creation, which the target cannot honour" -f $decl.Groups[1].Value.Trim())
}
$sites = New-Object System.Collections.Generic.List[string]
foreach ($rel in $seam.RetainedTus) {
    $p = Join-Path $fsDir $rel
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { continue }
    $n = 0
    foreach ($line in (Get-Content -LiteralPath $p)) {
        $n++
        if ($line -match 'BEL_Cross_mkdir\s*\(') { [void]$sites.Add(("{0}:{1}" -f $rel, $n)) }
    }
}
$frozenSites = @($seam.MkidrCallSites | Sort-Object)
$gotSites = @($sites | Sort-Object)
if (($gotSites -join '|') -cne ($frozenSites -join '|')) {
    Fail ("check 6: BEL_Cross_mkdir call sites changed")
    Emit ("        frozen  : " + ($frozenSites -join ' '))
    Emit ("        measured: " + ($gotSites -join ' '))
} elseif ($decl.Success -and $decl.Groups[1].Value.Trim() -ceq 'void') {
    Emit ("  ok    returns void; {0} call site(s), all results discarded by construction" -f $gotSites.Count)
    Emit ("        the port creates its own directories ({0})" -f 'path_mapping / be_app_paths')
}
Emit ""

Emit "---------------------------------------------------------------------------"
if ($script:Problems.Count -eq 0) {
    Emit "FILESYSTEM SEAM: PASS (6 checks)"
    Flush-Report
    exit 0
}
Emit ("FILESYSTEM SEAM: FAILED ({0} problem(s))" -f $script:Problems.Count)
Flush-Report
exit 1
