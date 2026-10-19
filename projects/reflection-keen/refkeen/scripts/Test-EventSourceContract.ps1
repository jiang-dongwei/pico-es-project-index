<#
.SYNOPSIS
    S20-C event source gate -- the contract the port's replacement for
    src/backend/events/be_events_sdl.c has to satisfy.

.DESCRIPTION
    Answers one question: is the port's event source a faithful replacement for
    the one upstream TU the engine cannot run without?

    be_events_sdl.c is the eighth platform TU. It reaches SDL through its own
    header (be_events_sdl.h) rather than directly, so the platform boundary
    gate's "includes SDL3" test cannot see it -- which is why it was the one
    platform TU with no record at all until now. Dropping it from the build
    removes a small, fully enumerable set of symbols, and that set is the whole
    contract:

      BE_ST_PollEvents()                         the per-frame poll point,
                                                 called from be_input.c,
                                                 be_timing.c and id_us.c
      BE_ST_SetAppQuitCallback(void (*)(void))    called once, from
                                                 kdreams/kd_main.c

    Two further definitions live behind REFKEEN_CONFIG_EVENTS_CALLBACK, which
    belongs to the mobile/background ports and stays undefined here.

    Checks:

      1. the replaced TU's exported set is unchanged, and the port provides the
         unconditional half of it while REFKEEN_CONFIG_EVENTS_CALLBACK is still
         undefined in the port's config header
      2. upstream's keymap header has exactly one includer in the port, and it
         is the replacement TU -- upstream states this requirement itself,
         because the header defines a non-static table at file scope. Nothing in
         the port may include, compile or otherwise reach the replaced TU or its
         own header, which pulls real SDL in; a mention in a comment or in an
         #error message is documentation, not reachability, and is reported
         separately rather than counted as a defect
      3. every scancode the port can emit is actually live in that keymap, and
         the channel table covers exactly as many channels as compat.h declares
      4. the compiled module's own contract output agrees with the table parsed
         out of source -- two independent derivations of one fact
      5. the replacement compiles on the target toolchain, its diagnostic
         multiset equals the frozen one, and
         -Werror=implicit-function-declaration changes nothing
      6. the replacement owns none of the shared key-repeat state, and that state
         is still where the facts say it is
      7. the symbol ledger, both directions: no symbol owned by a TU the port
         does not build may be named, no SDL-internal name may be named, and
         every upstream global the replacement DOES name must actually be
         defined in a TU the port builds. Each entry's ownership is looked up in
         the upstream tree and compared against the build set derived from
         BoundaryFacts.psd1 -- so a wrong entry fails the gate instead of
         silently describing a tree that does not exist

    Checks 3-5 need compilers. Without -HostGcc / -TargetGcc they are SKIPPED
    loudly and named in the summary line rather than silently dropped.

.PARAMETER SelfTest
    Hand off to the companion script Test-EventSourceSelfTest.ps1, which mirrors
    the real sources, confirms the clean copy passes, then injects one defect per
    check and requires exactly that check to fire. The companion is runnable on
    its own too; the switch exists so the gate and its negative-path test are
    found in the same place.
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
    $companion = Join-Path $PSScriptRoot 'Test-EventSourceSelfTest.ps1'
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
if (-not $ReportPath) { $ReportPath = Join-Path $Workspace '.emv2\logs\s20-event-source.txt' }

$script:Lines = New-Object System.Collections.Generic.List[string]
$script:Problems = New-Object System.Collections.Generic.List[string]
$script:Skipped = New-Object System.Collections.Generic.List[string]
$script:CheckBase = 0

function Emit([string]$text) { [void]$script:Lines.Add($text) }
function Fail([string]$text) { [void]$script:Problems.Add($text); Emit ("  FAIL  " + $text) }
function Skip([string]$text) { [void]$script:Skipped.Add($text); Emit ("  SKIP  " + $text) }
function Note([string]$text) { Emit ("  ok    " + $text) }

# Per-check problem accounting. Whether a check prints its success line must
# depend on whether THAT check failed, not on the running total: with a global
# counter, one failure early on silences the success line of every later check,
# and a silent check is indistinguishable from a skipped one.
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
        # -Width matters: Out-String defaults to the console width (often 120)
        # and FOLDS long lines, which cuts compiler diagnostics mid-path.
        $out = (& $Exe @Arguments 2>&1 | Out-String -Width 4096)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Rc = $rc; Out = $out }
}

# Warning classes as a sorted multiset, e.g. @('-Wunused-function=1'). Message
# wording is deliberately excluded: only the class and its count, so a
# compiler-version wording change is not mistaken for an upstream change.
function Get-DiagnosticSet([string]$text) {
    $counts = @{}
    foreach ($m in [regex]::Matches($text, '\[(-W[A-Za-z0-9-]+)\]')) {
        $k = $m.Groups[1].Value
        if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
    }
    $items = @($counts.Keys | ForEach-Object { '{0}={1}' -f $_, $counts[$_] })
    return @($items | Sort-Object)
}

function Get-Text([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    return Get-Content -LiteralPath $path -Raw
}

# Two views of one file, because "the port names the replaced TU" and "the port
# can reach the replaced TU" are different questions:
#
#   Remove-Comments               -> still sees string literals, so an
#                                    #include directive survives
#   Remove-CommentsAndStrings     -> what is left is code, so a symbol named
#                                    here is a symbol the linker will ask for
#
# The difference between the two counts is the number of message-literal
# mentions (an #error text, say), which the gate reports rather than hides.
#
# Both replacements blank the match character-for-character instead of deleting
# it, so line numbers and offsets stay valid and a multi-line block comment
# cannot glue two definitions onto one line -- which would corrupt the
# definition scan below.
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

# Line ranges covered by an `#ifdef <macro>` block, using a generic preprocessor
# nesting walk so that a differently-shaped conditional elsewhere in the file
# cannot shift the answer.
function Get-GatedRanges([string]$text, [string]$macro) {
    $ranges = New-Object System.Collections.Generic.List[object]
    $stack = New-Object System.Collections.Generic.List[object]
    $n = 0
    foreach ($line in ($text -split "`r?`n")) {
        $n++
        if ($line -match '^\s*#\s*(?:if|ifdef|ifndef)\b\s*(.*)$') {
            $cond = $matches[1].Trim()
            [void]$stack.Add(@{ Line = $n; IsTarget = ($cond -ceq $macro) })
        } elseif ($line -match '^\s*#\s*endif\b') {
            if ($stack.Count -gt 0) {
                $top = $stack[$stack.Count - 1]
                [void]$stack.RemoveAt($stack.Count - 1)
                if ($top['IsTarget']) { [void]$ranges.Add(@{ Start = $top['Line']; End = $n }) }
            }
        }
    }
    # No unary comma: the caller wraps in @(), and a comma here would hand back
    # one List object instead of its elements -- which then breaks any pipeline
    # consumer with "cannot convert \"Name\" to System.Int32".
    return $ranges
}

# Non-static definitions at file scope. A line ending in ");" is a prototype;
# globals are the reason the rule cannot simply be "does not end in ;".
function Get-TopLevelDefinitions([string]$text) {
    $defs = New-Object System.Collections.Generic.List[object]
    $n = 0
    foreach ($rawLine in ($text -split "`r?`n")) {
        $n++
        $line = $rawLine
        $c = $line.IndexOf('//')
        if ($c -ge 0) { $line = $line.Substring(0, $c) }
        if ($line -notmatch '^[A-Za-z_]') { continue }
        if ($line -match '^(?:static|extern)\b') { continue }
        $trim = $line.TrimEnd()
        if ($trim -match '\)\s*;$') { continue }
        if ($trim.Contains('(')) {
            $m = [regex]::Match($trim, '^[A-Za-z_][A-Za-z0-9_ \t\*]*?([A-Za-z_][A-Za-z0-9_]*)\s*\(')
            if ($m.Success) { [void]$defs.Add(@{ Line = $n; Name = $m.Groups[1].Value }) }
        } else {
            foreach ($m in [regex]::Matches($trim, '\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=[,;=]|$)')) {
                $nm = $m.Groups[1].Value
                if ($nm -match '^(?:g_|BE_ST_|BEL_ST_)') { [void]$defs.Add(@{ Line = $n; Name = $nm }) }
            }
        }
    }
    return $defs
}

# Which TU defines which name, over the whole pinned upstream src tree. Built
# once and reused, so the ledger in check 7 costs one pass rather than one pass
# per symbol. Keys are 'src/...' paths -- the same shape the facts file uses.
#
# Only the pinned tree is walked. Walking _deps/ wholesale would also read the
# self-test scratch copies of upstream that the gates leave behind (there were
# twenty-odd at the time of writing) and report their contents as if they were
# the port's.
function Get-DefinitionIndex([string]$upstreamSrc) {
    $index = @{}
    foreach ($file in (Get-ChildItem -LiteralPath $upstreamSrc -Recurse -File -Filter '*.c')) {
        $body = Remove-CommentsAndStrings (Get-Content -LiteralPath $file.FullName -Raw)
        $names = New-Object System.Collections.Generic.List[string]
        foreach ($d in @(Get-TopLevelDefinitions $body)) { [void]$names.Add($d['Name']) }
        $rel = 'src/' + $file.FullName.Substring($upstreamSrc.Length).TrimStart('\', '/').Replace('\', '/')
        $index[$rel] = @($names)
    }
    return $index
}

# The build set, DERIVED from the facts rather than restated next to them:
# everything under src/ except the platform TUs, the replaced TUs, the excluded
# TUs and the disabled launcher. Restating it would be one more list to drift.
function Get-NonBuiltTus($facts) {
    $tus = New-Object System.Collections.Generic.List[string]
    foreach ($tu in $facts.PlatformTus) { [void]$tus.Add($tu) }
    [void]$tus.Add($facts.EventSource.ReplacedTu)
    foreach ($name in $facts.FilesystemSeam.ExcludedTus.Keys) {
        [void]$tus.Add('src/backend/filesystem/' + $name)
    }
    foreach ($tu in $facts.PortExcludedTus.Keys) { [void]$tus.Add($tu) }
    return $tus
}

function Test-TuBuilt([string]$relPath, $facts, [string[]]$nonBuilt) {
    if ($nonBuilt -ccontains $relPath) { return $false }
    if ($relPath.StartsWith($facts.LauncherPrefix, [StringComparison]::Ordinal)) { return $false }
    return $true
}

# Definition sites of one name. A trailing underscore in the name means
# "prefix", so one entry can stand for a whole symbol family instead of seven
# entries to keep in step.
function Get-DefinitionSites([string]$name, $index) {
    $isPrefix = $name.EndsWith('_')
    $sites = New-Object System.Collections.Generic.List[string]
    foreach ($tu in @($index.Keys | Sort-Object)) {
        foreach ($defined in $index[$tu]) {
            $isHit = $false
            if ($isPrefix) { $isHit = $defined.StartsWith($name, [StringComparison]::Ordinal) }
            else { $isHit = ($defined -ceq $name) }
            if ($isHit) { [void]$sites.Add($tu); break }
        }
    }
    return @($sites)
}

# ---------------------------------------------------------------------------

$facts = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1')
$es = $facts.EventSource

$portDir = Join-Path $Workspace 'refkeen\platform\retropicoes'
$portTu = Join-Path $portDir $es.KeymapIncluder       # the replacement TU
$eventsC = Join-Path (Join-Path $UpstreamRoot 'src\backend\events') 'be_events_sdl.c'

Emit "S20 EVENT SOURCE GATE"
Emit ("workspace     : {0}" -f $Workspace)
Emit ("upstream      : {0}" -f $UpstreamRoot)
Emit ("target gcc    : {0}" -f $TargetGcc)
Emit ("host gcc      : {0}" -f $HostGcc)
if ($RunId) { Emit ("run id        : {0}" -f $RunId) }
Emit ""

# --- Check 1: the export set of the replaced TU ----------------------------
Begin-Check "Check 1  the replaced TU exports exactly what the port provides"
if (-not (Test-Path -LiteralPath $eventsC -PathType Leaf)) {
    Fail ("check 1: cannot read the replaced TU: {0}" -f $es.ReplacedTu)
} else {
    $eventsText = Get-Content -LiteralPath $eventsC -Raw
    $gated = @(Get-GatedRanges $eventsText $es.EventsCallbackMacro)
    $allDefs = @(Get-TopLevelDefinitions $eventsText)
    $unconditional = New-Object System.Collections.Generic.List[string]
    $conditional = New-Object System.Collections.Generic.List[string]
    foreach ($d in $allDefs) {
        $inGate = $false
        foreach ($r in $gated) {
            if (($d['Line'] -ge $r['Start']) -and ($d['Line'] -le $r['End'])) { $inGate = $true; break }
        }
        if ($inGate) { [void]$conditional.Add($d['Name']) } else { [void]$unconditional.Add($d['Name']) }
    }

    # Precomputed strings: see the -join note in Test-FilesystemSeamContract.ps1.
    $gotUncondText = ((@($unconditional) | Sort-Object -Unique) -join '|')
    $expUncondText = ((@($es.RequiredExports) | Sort-Object -Unique) -join '|')
    $gotCondText = ((@($conditional) | Sort-Object -Unique) -join '|')
    $expCondText = ((@($es.ConditionalExports) | Sort-Object -Unique) -join '|')

    if ($allDefs.Count -eq 0) {
        Fail "check 1: parsed zero top-level definitions from the replaced TU -- the parser is wrong, not the tree"
    }
    if ($gotUncondText -cne $expUncondText) {
        Fail "check 1: the replaced TU's unconditional exports moved"
        Emit ("        frozen  : " + $expUncondText)
        Emit ("        measured: " + $gotUncondText)
    }
    if ($gotCondText -cne $expCondText) {
        Fail "check 1: the REFKEEN_CONFIG_EVENTS_CALLBACK-gated exports moved"
        Emit ("        frozen  : " + $expCondText)
        Emit ("        measured: " + $gotCondText)
    }

    if (Test-Path -LiteralPath $portTu -PathType Leaf) {
        $portDefs = @(Get-TopLevelDefinitions (Get-Content -LiteralPath $portTu -Raw))
        $portNames = @($portDefs | ForEach-Object { $_['Name'] })
        foreach ($needed in $es.RequiredExports) {
            if (-not ($portNames -ccontains $needed)) {
                Fail ("check 1: the replacement does not define {0}" -f $needed)
            }
        }
        foreach ($conditionalName in $es.ConditionalExports) {
            if ($portNames -ccontains $conditionalName) {
                Fail ("check 1: the replacement defines {0}, which is only valid when {1} is set" -f `
                      $conditionalName, $es.EventsCallbackMacro)
            }
        }
    } else {
        Fail ("check 1: the replacement TU is missing: {0}" -f $es.KeymapIncluder)
    }

    $cfg = Get-Text (Join-Path $Workspace $es.ConfigHeader)
    if ($null -eq $cfg) {
        Fail ("check 1: cannot read {0}" -f $es.ConfigHeader)
    } elseif ($cfg -match ('^\s*#\s*define\s+' + [regex]::Escape($es.EventsCallbackMacro) + '\b')) {
        Fail ("check 1: {0} is now defined in {1}; the replacement carries an #error guard for exactly this" -f `
              $es.EventsCallbackMacro, $es.ConfigHeader)
    } elseif ($script:Problems.Count -eq $script:CheckBase) {
        Note ("unconditional export(s) unchanged: " + $gotUncondText)
        Note ("conditional export(s) unchanged and the macro is still undefined: " + $gotCondText)
    }
}
Emit ""

# --- Check 2: the keymap has exactly one includer -------------------------
Begin-Check "Check 2  the keymap header has exactly one includer, and it is the replacement"
$keymapName = Split-Path -Leaf $es.KeymapHeader
$includers = New-Object System.Collections.Generic.List[string]
foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File -Filter '*.c')) {
    $body = Get-Content -LiteralPath $f.FullName -Raw
    if ($body -match ('#\s*include\s*[<"].*' + [regex]::Escape($keymapName))) { [void]$includers.Add($f.Name) }
}
foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File -Filter '*.h')) {
    $body = Get-Content -LiteralPath $f.FullName -Raw
    if ($body -match ('#\s*include\s*[<"].*' + [regex]::Escape($keymapName))) { [void]$includers.Add($f.Name) }
}
$gotIncText = ((@($includers) | Sort-Object -Unique) -join '|')
if ($includers.Count -eq 0) {
    Fail ("check 2: no port file includes {0}; the replacement would not see the keymap" -f $keymapName)
} elseif ($gotIncText -cne $es.KeymapIncluder) {
    Fail ("check 2: {0} has {1} includer(s) in the port, expected exactly {2}" -f `
          $keymapName, $includers.Count, $es.KeymapIncluder)
    Emit ("        measured: " + $gotIncText)
} else {
    Note ("{0} is included by exactly {1}, as upstream requires" -f $keymapName, $es.KeymapIncluder)
}
# The replaced TU must not be reachable from the port either, directly or
# through its own header: that header pulls real SDL in.
#
# Three views of every port source, because only two of them are defects:
#   * an #include directive naming it  -> defect, the TU becomes reachable
#   * the name surviving in CODE       -> defect, e.g. a build-file source list
#   * the name only in comments or in a message literal -> documentation. The
#     port is *supposed* to say what it replaced, and an #error that explains
#     itself is worth more than one that does not. Reported, not counted.
$replacedPattern = 'be_events_sdl\.(c|h)'
$staleIncludes = New-Object System.Collections.Generic.List[string]
$staleCode     = New-Object System.Collections.Generic.List[string]
$inProse       = 0
$inMessages    = 0
foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File |
                Where-Object { $_.Extension -in @('.c', '.h', '.cpp', '.txt') })) {
    $raw = Get-Content -LiteralPath $f.FullName -Raw
    if ([regex]::IsMatch($raw, '(?m)^[ \t]*#[ \t]*include\b.*' + $replacedPattern)) {
        [void]$staleIncludes.Add($f.Name)
    }
    $noComments = Remove-Comments $raw
    $code = Remove-CommentsAndStrings $raw
    $rawCount = ([regex]::Matches($raw, $replacedPattern)).Count
    $noCommentCount = ([regex]::Matches($noComments, $replacedPattern)).Count
    $codeCount = ([regex]::Matches($code, $replacedPattern)).Count
    if ($codeCount -gt 0) { [void]$staleCode.Add($f.Name) }
    $inMessages += ($noCommentCount - $codeCount)
    $inProse += ($rawCount - $noCommentCount)
}
if ($staleIncludes.Count -gt 0) {
    Fail ("check 2: the port includes the replaced TU's header: " + ((@($staleIncludes) | Sort-Object -Unique) -join ', '))
}
if ($staleCode.Count -gt 0) {
    Fail ("check 2: the port names the replaced TU or its header in code: " + ((@($staleCode) | Sort-Object -Unique) -join ', '))
}
if (Check-Clean) {
    if (($inProse + $inMessages) -gt 0) {
        Note ("{0} prose and {1} message-literal mention(s) of the replaced TU; no include, no code reference" -f $inProse, $inMessages)
    } else {
        Note "the replaced TU and its header are named nowhere in the port tree, not even in prose"
    }
}
Emit ""

# --- Check 3: every scancode the port can emit is live in the keymap -------
Begin-Check "Check 3  every scancode the port can emit exists in upstream's keymap"
$compatH  = Get-Text (Join-Path $Workspace 'compat\compat.h')
$shimText = Get-Text (Join-Path $Workspace $es.ShimScancode)
$imText   = Get-Text (Join-Path $portDir 'input_mapping.c')
$keymapText = Get-Text (Join-Path $UpstreamRoot $es.KeymapHeader)
$beStText = Get-Text (Join-Path $UpstreamRoot 'src\be_st.h')

$compatChannels = @()
if ($null -ne $compatH) {
    $m = [regex]::Match($compatH, 'typedef enum\s*\{(.*?)\}\s*compat_key_t;', 'Singleline')
    if ($m.Success) {
        $compatChannels = @([regex]::Matches($m.Groups[1].Value, '\bCOMPAT_KEY_([A-Z0-9_]+)\b') |
                            ForEach-Object { $_.Groups[1].Value } |
                            Where-Object { $_ -cne 'COUNT' })
    }
}
$portScancodes = @{}
if ($null -ne $imText) {
    foreach ($m in [regex]::Matches($imText, '\[COMPAT_KEY_([A-Z0-9_]+)\]\s*=\s*SDL_SCANCODE_([A-Z0-9_]+)')) {
        $portScancodes[$m.Groups[1].Value] = $m.Groups[2].Value
    }
}
$shimValues = @{}
if ($null -ne $shimText) {
    foreach ($m in [regex]::Matches($shimText, 'SDL_SCANCODE_([A-Z0-9_]+)\s*=\s*([0-9]+)\b')) {
        $shimValues[$m.Groups[1].Value] = [int]$m.Groups[2].Value
    }
}
$beStValues = @{}
if ($null -ne $beStText) {
    $start = $beStText.IndexOf('typedef enum BE_ST_ScanCode_T')
    $end = $beStText.IndexOf('} BE_ST_ScanCode_T;', $start)
    if (($start -ge 0) -and ($end -gt $start)) {
        $next = 0
        foreach ($rawLine in ($beStText.Substring($start, $end - $start) -split "`r?`n")) {
            $l = $rawLine
            $c = $l.IndexOf('//')
            if ($c -ge 0) { $l = $l.Substring(0, $c) }
            $l = $l.Trim()
            if ($l.Length -eq 0 -or $l -match '^typedef' -or $l -match 'BE_ST_ScanCode_T') { continue }
            foreach ($piece in ($l -split ',')) {
                $e = $piece.Trim().Trim('{').Trim()
                if ($e -match '^BE_ST_SC_([A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)$') {
                    $lit = $matches[2]
                    $v = if ($lit.StartsWith('0x') -or $lit.StartsWith('0X')) { [Convert]::ToInt32($lit.Substring(2), 16) } else { [int]$lit }
                    $beStValues['BE_ST_SC_' + $matches[1]] = $v
                    $next = $v + 1
                } elseif ($e -match '^BE_ST_SC_([A-Z0-9_]+)$') {
                    $beStValues['BE_ST_SC_' + $matches[1]] = $next
                    $next = $next + 1
                }
            }
        }
    }
}
# ordinal -> BE_ST_SC_ name for every non-empty keymap entry
$keymap = @{}
if ($null -ne $keymapText) {
    $ordinal = 0
    foreach ($rawLine in ($keymapText -split "`r?`n")) {
        if ($rawLine -match '^\s*emptyDOSKeyEvent\s*,') { $ordinal++; continue }
        $m = [regex]::Match($rawLine, '^\s*\{(?:true|false)\s*,\s*(BE_ST_SC_[A-Z0-9_]+)\s*\}')
        if ($m.Success) { $keymap[$ordinal] = $m.Groups[1].Value; $ordinal++ }
    }
}

if ($compatChannels.Count -eq 0) {
    Fail "check 3: could not parse the compat_key_t enum"
} elseif ($portScancodes.Count -eq 0) {
    Fail "check 3: could not parse the port's channel table"
} elseif ($keymap.Count -eq 0) {
    Fail "check 3: could not parse the keymap"
} else {
    if ($portScancodes.Count -ne $compatChannels.Count) {
        Fail ("check 3: the port table has {0} entries but compat.h declares {1} channels" -f `
              $portScancodes.Count, $compatChannels.Count)
    }
    foreach ($channel in ($compatChannels | Sort-Object)) {
        if (-not $portScancodes.ContainsKey($channel)) {
            Fail ("check 3: channel {0} has no scancode in the port table" -f $channel)
            continue
        }
        $shimName = $portScancodes[$channel]
        if (-not $shimValues.ContainsKey($shimName)) {
            Fail ("check 3: channel {0} names SDL_SCANCODE_{1}, absent from the shim" -f $channel, $shimName)
            continue
        }
        $sc = $shimValues[$shimName]
        if (-not $keymap.ContainsKey($sc)) {
            Fail ("check 3: channel {0} emits SDL scancode {1}, which upstream's keymap leaves EMPTY -- the key would be dead" -f $channel, $sc)
            continue
        }
        $dosName = $keymap[$sc]
        if (-not $beStValues.ContainsKey($dosName)) {
            Fail ("check 3: keymap entry {0} names {1}, absent from be_st.h" -f $sc, $dosName)
            continue
        }
        if ($beStValues[$dosName] -eq 0) {
            Fail ("check 3: channel {0} reaches {1}, whose DOS scancode is 0 -- be_input.c would treat that as a real key press" -f $channel, $dosName)
        }
    }
    if (Check-Clean) {
        Note ("all {0} channel(s) emit scancodes that are live in the keymap and reach a non-zero DOS scancode" -f $compatChannels.Count)
    }
}
Emit ""

# --- Check 4: the compiled module agrees with the source -------------------
Begin-Check "Check 4  the compiled module's contract output agrees with the source table"
$hostTu = Join-Path $portDir 'event_source.c'
$hostTest = Join-Path (Join-Path $Workspace 'refkeen\tests') $es.PortTest
$compiledContract = @{}
if ((-not (Test-Path -LiteralPath $HostGcc -PathType Leaf)) -or
    (-not (Test-Path -LiteralPath $hostTest -PathType Leaf))) {
    Skip "check 4 (compiled contract): -HostGcc not found or the host test is missing"
} else {
    $outDir = Join-Path $Workspace '_deps\_probe'
    if (-not (Test-Path -LiteralPath $outDir)) { [void](New-Item -ItemType Directory -Path $outDir -Force) }
    $exe = Join-Path $outDir ('s20c-event-source' + $(if ($RunId) { '-' + $RunId } else { '' }) + '.exe')
    $r = Invoke-Native $HostGcc @(
        '-std=c99', '-O1', '-Wall', '-Wextra', '-Werror',
        '-I', (Join-Path $Workspace 'compat'),
        '-I', (Join-Path $Workspace 'refkeen\host\shim'),
        '-I', $portDir,
        '-o', $exe, $hostTest, $hostTu,
        (Join-Path $portDir 'input_mapping.c')
    )
    if ($r.Rc -ne 0) {
        Fail ("check 4: the host test did not compile (rc={0}): {1}" -f $r.Rc, ($r.Out -replace "`r?`n", ' '))
    } else {
        $run = Invoke-Native $exe @()
        if ($run.Rc -ne 0) {
            Fail ("check 4: the host test exited {0}" -f $run.Rc)
        } elseif (-not $run.Out.Contains('REFKEEN EVENT SOURCE: PASS')) {
            Fail "check 4: the host test did not print its PASS marker"
        } else {
            foreach ($m in [regex]::Matches($run.Out, 'CONTRACT\s+channel=([A-Z]+)\s+scancode=([0-9]+)')) {
                $compiledContract[$m.Groups[1].Value] = [int]$m.Groups[2].Value
            }
            if ($compiledContract.Count -eq 0) {
                Fail "check 4: the host test printed no per-channel CONTRACT lines"
            } elseif ($compiledContract.Count -ne $portScancodes.Count) {
                Fail ("check 4: compiled contract has {0} channels, source table has {1}" -f `
                      $compiledContract.Count, $portScancodes.Count)
            } else {
                foreach ($channel in ($portScancodes.Keys | Sort-Object)) {
                    $shimName = $portScancodes[$channel]
                    if (-not $shimValues.ContainsKey($shimName)) { continue }
                    if ($compiledContract[$channel] -ne $shimValues[$shimName]) {
                        Fail ("check 4: channel {0}: compiled={1} but the source names SDL_SCANCODE_{2}={3}" -f `
                              $channel, $compiledContract[$channel], $shimName, $shimValues[$shimName])
                    }
                }
                if (Check-Clean) {
                    Note ("compiled contract and source table agree on all {0} channel(s), and the host test passes under -Werror" -f $compiledContract.Count)
                }
            }
        }
    }
}
Emit ""

# --- Check 5: the replacement compiles on the target ----------------------
Begin-Check "Check 5  the replacement compiles on the target, diagnostics equal the frozen set"
$shimUmbrella = Get-Text (Join-Path $Workspace $es.ShimUmbrella)
if ($null -eq $shimUmbrella) {
    Fail ("check 5: cannot read {0}" -f $es.ShimUmbrella)
} elseif ($shimUmbrella -notmatch ('#\s*include\s*<SDL3/SDL_scancode\.h>')) {
    Fail ("check 5: {0} no longer includes SDL_scancode.h; the keymap's array bound {1} would be undeclared again" -f `
          $es.ShimUmbrella, $es.KeymapArrayBound)
} else {
    Note ("{0} provides {1} transitively, so the keymap's declaration compiles" -f $es.ShimUmbrella, $es.KeymapArrayBound)
}

$upSrc = Join-Path $UpstreamRoot 'src'
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

if (-not (Test-Path -LiteralPath $TargetGcc -PathType Leaf)) {
    Skip "check 5 (target compile): -TargetGcc not found"
} else {
    $outDir = Join-Path $Workspace '_deps\_probe'
    if (-not (Test-Path -LiteralPath $outDir)) { [void](New-Item -ItemType Directory -Path $outDir -Force) }
    $suffix = if ($RunId) { '-' + $RunId } else { '' }

    $obj = Join-Path $outDir ('s20c-events-port' + $suffix + '.o')
    $base = Invoke-Native $TargetGcc (@($incs) + @($defs) + @(
        '-c', '-std=c99', '-fno-common', '-O0', '-fmessage-length=0',
        '-Wall', '-Wextra', '-o', $obj, $portTu))
    if ($base.Rc -ne 0) {
        Fail "check 5: the replacement does not compile on the target"
        Emit ("        " + (($base.Out -split "`r?`n" | Where-Object { $_ -match ':\d+:\d+: (error|fatal error)' } | Select-Object -First 4) -join "`n        "))
    } else {
        $got = Get-DiagnosticSet $base.Out
        $frozen = @($es.FrozenDiagnostics[$es.KeymapIncluder] | Sort-Object)
        $gotText = (@($got) -join '|')
        $expText = (@($frozen) -join '|')
        if ($gotText -cne $expText) {
            Fail "check 5: diagnostic set changed for the replacement"
            Emit ("        frozen  : " + $expText)
            Emit ("        measured: " + $gotText)
        } else {
            Note ("compiles on the target; diagnostics match the frozen set ({0})" -f $(if ($expText) { $expText } else { 'none' }))
        }

        # The port never tolerates this one class, upstream or not.
        $obj2 = Join-Path $outDir ('s20c-events-port' + $suffix + '-werror.o')
        $strict = Invoke-Native $TargetGcc (@($incs) + @($defs) + @(
            '-c', '-std=c99', '-fno-common', '-O0', '-fmessage-length=0',
            '-Wall', '-Wextra', '-Werror=implicit-function-declaration',
            '-o', $obj2, $portTu))
        if ($strict.Rc -ne 0) {
            Fail "check 5: the replacement has an implicit function declaration"
        } else {
            Note "-Werror=implicit-function-declaration changes nothing"
        }
    }
}
Emit ""

# --- Check 6: no shared state is forked ------------------------------------
Begin-Check "Check 6  the replacement owns no shared key-repeat state"
$portTuExists = Test-Path -LiteralPath $portTu -PathType Leaf
$portCode = ''
if (-not $portTuExists) {
    Fail ("check 6: the replacement TU is missing: {0}" -f $es.KeymapIncluder)
} else {
    # Code view: a symbol "defined" inside a comment is not defined.
    $portCode = Remove-CommentsAndStrings (Get-Content -LiteralPath $portTu -Raw)
    $portDefs = @(Get-TopLevelDefinitions $portCode | ForEach-Object { $_['Name'] })
    $forked = @($es.SharedRepeatSymbols | Where-Object { $portDefs -ccontains $_ })
    if ($forked.Count -gt 0) {
        Fail ("check 6: the replacement defines shared key-repeat state: " + ($forked -join ', '))
    } else {
        Note ("none of the {0} shared repeat symbol(s) is defined here; the state stays in {1}" -f `
              @($es.SharedRepeatSymbols).Count, $es.SharedRepeatDefinitionTu)
    }

    # Confirm the frozen "shared" half of that claim too: they must still be
    # defined where the facts say they are, or the note above describes nothing.
    $sharedTu = Get-Text (Join-Path $UpstreamRoot $es.SharedRepeatDefinitionTu)
    if ($null -eq $sharedTu) {
        Fail ("check 6: cannot read {0}" -f $es.SharedRepeatDefinitionTu)
    } else {
        $sharedDefs = @(Get-TopLevelDefinitions (Remove-CommentsAndStrings $sharedTu) | ForEach-Object { $_['Name'] })
        $missing = @($es.SharedRepeatSymbols | Where-Object { -not ($sharedDefs -ccontains $_) })
        if ($missing.Count -gt 0) {
            Fail ("check 6: {0} no longer defines: {1}" -f $es.SharedRepeatDefinitionTu, ($missing -join ', '))
        }
    }
}
Emit ""

# --- Check 7: the symbol ledger, in both directions ------------------------
Begin-Check "Check 7  the symbol ledger: nothing unlinkable is named, everything named exists"
if (-not $portTuExists) {
    Fail ("check 7: the replacement TU is missing: {0}" -f $es.KeymapIncluder)
} else {
    $index = Get-DefinitionIndex (Join-Path $UpstreamRoot 'src')
    $nonBuilt = @(Get-NonBuiltTus $facts)

    # A word-boundary pattern per entry. A trailing underscore is a prefix and
    # must NOT be closed by a boundary, or the family it stands for would not
    # match the entry.
    function Get-SymbolPattern([string]$sym) {
        $p = '\b' + [regex]::Escape($sym)
        if (-not $sym.EndsWith('_')) { $p += '\b' }
        return $p
    }

    # (a) the negative half: nothing the port cannot link may be named.
    $namedUnlinkable = New-Object System.Collections.Generic.List[string]
    foreach ($sym in @($es.NonBuiltSymbols.Keys | Sort-Object)) {
        if ([regex]::IsMatch($portCode, (Get-SymbolPattern $sym))) { [void]$namedUnlinkable.Add($sym) }
    }
    if ($namedUnlinkable.Count -gt 0) {
        Fail ("check 7: the replacement names symbol(s) owned by TUs the port does not build: " + ($namedUnlinkable -join ', '))
    } else {
        Note ("none of the {0} symbol(s) owned by non-built TUs is named" -f @($es.NonBuiltSymbols.Keys).Count)
    }

    # (b) and the premise of (a), measured rather than asserted. This is the
    # part that would have caught the first draft of the list: two of its eight
    # entries named symbols that a TU the port builds defines, and one of those
    # two had already been written into a comment as if it were a dropped one.
    $ownedElsewhere = New-Object System.Collections.Generic.List[string]
    $unowned = New-Object System.Collections.Generic.List[string]
    foreach ($sym in @($es.NonBuiltSymbols.Keys | Sort-Object)) {
        $recorded = $es.NonBuiltSymbols[$sym]
        $sites = @(Get-DefinitionSites $sym $index)
        if ($sites.Count -eq 0) {
            [void]$unowned.Add($sym)
            continue
        }
        if (-not ($sites -ccontains $recorded)) {
            Fail ("check 7: {0} is defined in {1}, not in the recorded owner {2}" -f $sym, ($sites -join ', '), $recorded)
            continue
        }
        $builtSites = @($sites | Where-Object { Test-TuBuilt $_ $facts $nonBuilt })
        if ($builtSites.Count -gt 0) {
            [void]$ownedElsewhere.Add($sym + ' -> ' + ($builtSites -join ', '))
        }
    }
    if ($unowned.Count -gt 0) {
        Fail ("check 7: nothing in the upstream tree defines {0}; if these are SDL/library names they belong in SdlApiSymbols" -f ($unowned -join ', '))
    }
    if ($ownedElsewhere.Count -gt 0) {
        Fail ("check 7: these are defined in TU(s) the port DOES build, so the replacement may use them and they do not belong in NonBuiltSymbols:")
        foreach ($item in $ownedElsewhere) { Emit ("        " + $item) }
    }
    if (Check-Clean) {
        Note ("every non-built entry's ownership was looked up and confirmed: defined in {0}, and in no built TU" -f `
              (@($es.NonBuiltSymbols.Values | Sort-Object -Unique) -join ', '))
    }

    # (c) SDL's own event API. The replacement has no reason to name any of it,
    # and the shim does not provide it either -- so if the shim ever grows one of
    # these names, that premise has changed and it is worth saying out loud.
    $namedSdl = New-Object System.Collections.Generic.List[string]
    foreach ($sym in @($es.SdlApiSymbols)) {
        if ([regex]::IsMatch($portCode, (Get-SymbolPattern $sym))) { [void]$namedSdl.Add($sym) }
    }
    if ($namedSdl.Count -gt 0) {
        Fail ("check 7: the replacement names SDL's own event API: " + ($namedSdl -join ', '))
    } else {
        Note ("neither of the {0} SDL event-API name(s) is mentioned" -f @($es.SdlApiSymbols).Count)
    }
    $shimProvides = New-Object System.Collections.Generic.List[string]
    foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen\host\shim') -Recurse -File)) {
        $shimBody = Remove-CommentsAndStrings (Get-Content -LiteralPath $f.FullName -Raw)
        foreach ($sym in @($es.SdlApiSymbols)) {
            if ([regex]::IsMatch($shimBody, (Get-SymbolPattern $sym))) {
                [void]$shimProvides.Add($sym + ' in ' + $f.Name)
            }
        }
    }
    if ($shimProvides.Count -gt 0) {
        Note ("the shim now declares {0} -- naming it would be a compile-time dependency rather than a link error; the rule above still stands" -f `
              ((@($shimProvides) | Sort-Object -Unique) -join ', '))
    } else {
        Note "the shim provides none of them, so naming one would be a link error, not a compile error"
    }

    # (d) the positive half. Without it the ledger could only ever catch a name
    # that must not appear, never one that is silently missing -- which is the
    # failure mode that would otherwise surface at link time in C13.
    foreach ($sym in @($es.NamedBuiltGlobals.Keys | Sort-Object)) {
        $recorded = $es.NamedBuiltGlobals[$sym]
        if (-not ([regex]::IsMatch($portCode, (Get-SymbolPattern $sym)))) {
            Fail ("check 7: the ledger records {0} as named by the replacement, but it is not named -- stale entry" -f $sym)
            continue
        }
        $sites = @(Get-DefinitionSites $sym $index)
        if (-not ($sites -ccontains $recorded)) {
            Fail ("check 7: {0} is named here but is not defined in the recorded {1} (sites: {2})" -f `
                  $sym, $recorded, $(if ($sites.Count) { $sites -join ', ' } else { 'none' }))
            continue
        }
        if (-not (Test-TuBuilt $recorded $facts $nonBuilt)) {
            Fail ("check 7: {0} is named here, but its owner {1} is not built by the port" -f $sym, $recorded)
        }
    }
    if (Check-Clean) {
        Note ("all {0} named upstream global(s) are named here and defined in built TUs" -f @($es.NamedBuiltGlobals.Keys).Count)
    }
}
Emit ""

Emit "---------------------------------------------------------------------------"
if ($script:Problems.Count -eq 0) {
    $suffix = if ($script:Skipped.Count -gt 0) { " -- $($script:Skipped.Count) check(s) SKIPPED, see above" } else { '' }
    Emit ("EVENT SOURCE: PASS (7 checks){0}" -f $suffix)
    Flush-Report
    exit 0
}
Emit ("EVENT SOURCE: FAILED ({0} problem(s))" -f $script:Problems.Count)
Flush-Report
exit 1
