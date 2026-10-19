<#
.SYNOPSIS
    S20-C frontend gate -- the contract the port's use of the shared public
    frontend has to satisfy.

.DESCRIPTION
    Answers one question: does the port adopt the shared frontend the way a
    single native game is supposed to, and only that way?

    Three shared components do the work (common/frontend/frontend_shell.c,
    native_game_settings.c, settings_host.c) and none of them may be edited by a
    port. So "the port adopted the frontend" is not a statement about files
    added; it is a statement about the calls the port makes, the symbols it
    defines, and the rules it must not break. All three are measurable:

      * the port's export surface: compiled on the target and read with nm;
      * the shared surface it uses: the undefined set of the same object, which
        is what the linker will have to resolve;
      * the rules the shared adoption gate (common/frontend/scripts/
        Test-FrontendAdoption.ps1) applies to every core, whose S20 arm is
        skipped until refkeen/pico/CMakeLists.txt exists -- so this gate applies
        the same two rules now, while the port can still be fixed cheaply.

    Checks:

      1. the adapter defines exactly the recorded symbols, no more and no fewer
      2. it compiles on the target: diagnostics equal the frozen set,
         -Werror=implicit-function-declaration changes nothing, and the section
         sizes equal the recorded ones
      3. the shared surface it uses is exactly the recorded one, and the single
         native-game rule holds: no rom_browser anywhere in the port
      4. the per-frame hook is actually wired: the C9 event TU calls it and
         consumes its SETTINGS_RAN bit with the documented reset
      5. the host test passes, and the contract it reports agrees with the
         record -- two independent derivations of one fact
      6. the symbol the quit path names is accounted for: it is defined only by
         a TU the port removes, so the port has to supply it
      7. the port satisfies the contracts the shared layer will apply: its
         runtime declarations agree with digger's and xrick's, and the adoption
         gate's own two prose/code rules already hold over this tree

    Checks 1, 2 and 6 need a compiler; check 5 needs a host compiler. Without
    -TargetGcc / -HostGcc they are SKIPPED loudly and named in the summary line
    rather than silently dropped.

.PARAMETER SelfTest
    Hand off to the companion script Test-FrontendAdapterSelfTest.ps1, which
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
    $companion = Join-Path $PSScriptRoot 'Test-FrontendAdapterSelfTest.ps1'
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
if (-not $ReportPath) { $ReportPath = Join-Path $Workspace '.emv2\logs\s20-frontend.txt' }

$script:Lines = New-Object System.Collections.Generic.List[string]
$script:Problems = New-Object System.Collections.Generic.List[string]
$script:Skipped = New-Object System.Collections.Generic.List[string]
$script:CheckBase = 0

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
# a terminating error, and a compiler that is *expected* to warn would abort the
# gate. Every compile and every nm/size call goes through here.
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

# Warning classes as a sorted multiset. Message wording is excluded on purpose,
# so a compiler-version wording change is not mistaken for a source change.
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

# Two views of one file, because "the port mentions X" and "the port can reach X"
# are different questions:
#
#   Remove-Comments               -> string literals survive, so an #include
#                                    directive and a printf() message are visible
#   Remove-CommentsAndStrings     -> what is left is code, so a symbol named here
#                                    is a symbol the linker will ask for
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

function Get-Text([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    return Get-Content -LiteralPath $path -Raw
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

# ---------------------------------------------------------------------------

$facts = Import-PowerShellDataFile -LiteralPath (Join-Path $PSScriptRoot 'BoundaryFacts.psd1')
$fe = $facts.Frontend

$portDir = Join-Path $Workspace 'refkeen\platform\retropicoes'
$portTu = Join-Path $portDir $fe.PortTu
$portTest = Join-Path (Join-Path $Workspace 'refkeen\tests') $fe.PortTest
$callerTu = Join-Path $portDir $fe.CallerTu
$upSrc = Join-Path $UpstreamRoot 'src'

$gccDir = Split-Path -Parent $TargetGcc
$nmExe = Join-Path $gccDir 'arm-none-eabi-nm.exe'
$sizeExe = Join-Path $gccDir 'arm-none-eabi-size.exe'

# The adapter touches no upstream header at all -- it includes the two shared
# frontend headers plus its own -- so this list is short on purpose. A new entry
# here means the adapter grew a dependency, which is a decision, not an accident.
$incs = @(
    '-I', $portDir,
    '-I', (Join-Path $Workspace 'compat'),
    '-I', (Join-Path $Workspace 'common\frontend'),
    '-I', (Join-Path $Workspace 'common\menu')
)
# -mcpu matters and is not decoration: the same reason as every other seam. See
# Test-AltControlStubContract.ps1 for the measurement that established it.
$arch = @('-mcpu=cortex-m33', '-mthumb')
$cflags = @('-c', '-std=c99', '-fno-common', '-O0', '-fmessage-length=0')

$cacheKey = ((@($incs) + @($arch) + @($cflags)) -join '|') + '||' + $Workspace
$objRoot = Join-Path $Workspace ('_deps\_probe\frontend-' + (Get-StringHash $cacheKey))
if (-not (Test-Path -LiteralPath $objRoot)) {
    [void](New-Item -ItemType Directory -Path $objRoot -Force)
}

# Recorded once, at the top level, because three checks compare against it and a
# check that is SKIPPED must not leave the next one reading an unset variable --
# under Set-StrictMode that is an error, and a check that quietly reads $null is
# worse than one that says so.
$recorded = @($fe.ExportedSymbols | Sort-Object -Unique)
$rText = ($recorded -join '|')
$wantUndef = @($fe.SharedCalls | Sort-Object -Unique)

Emit "S20 FRONTEND ADOPTION GATE"
Emit ("workspace     : {0}" -f $Workspace)
Emit ("upstream      : {0}" -f $UpstreamRoot)
Emit ("target gcc    : {0}" -f $TargetGcc)
Emit ("host gcc      : {0}" -f $HostGcc)
if ($RunId) { Emit ("run id        : {0}" -f $RunId) }
Emit ""

$targetAvailable = Test-Path -LiteralPath $TargetGcc -PathType Leaf
$hostAvailable = Test-Path -LiteralPath $HostGcc -PathType Leaf

# --- Check 1: the adapter's export surface ----------------------------------
Begin-Check "Check 1  the adapter defines exactly the recorded symbol(s), no more and no fewer"
if (-not $targetAvailable) {
    Skip "check 1 (export ledger): -TargetGcc not found"
} elseif (-not (Test-Path -LiteralPath $portTu -PathType Leaf)) {
    Fail ("check 1: the adapter TU is missing: {0}" -f $fe.PortTu)
} else {
    $obj = Join-Path $objRoot ($fe.PortTu.Replace('.c', '') + '.o')
    $r = Invoke-Native $TargetGcc (@($incs) + @($arch) + @($cflags) + @('-o', $obj, $portTu))
    if ($r.Rc -ne 0) {
        Fail ("check 1: the adapter does not compile on the target (rc={0})" -f $r.Rc)
        Emit ("        " + (($r.Out -split "`r?`n" | Where-Object { $_ -match ':\d+:\d+: (error|fatal error)' } | Select-Object -First 4) -join "`n        "))
    } else {
        $measured = @(Get-DefinedSymbols $obj | Sort-Object -Unique)
        $mText = ($measured -join '|')
        $rText = ($recorded -join '|')
        if ($mText -cne $rText) {
            Fail "check 1: the adapter's exported set is not the recorded one"
            $extra = @($measured | Where-Object { $recorded -cnotcontains $_ })
            $missing = @($recorded | Where-Object { $measured -cnotcontains $_ })
            if ($extra.Count -gt 0) { Emit ("        extra   : " + ($extra -join ', ')) }
            if ($missing.Count -gt 0) { Emit ("        missing : " + ($missing -join ', ')) }
        } elseif ($measured.Count -ne $fe.ExportedCount) {
            Fail ("check 1: the adapter exports {0} symbols, recorded {1} -- one of the two is wrong" -f $measured.Count, $fe.ExportedCount)
        } else {
            Note ("the adapter exports exactly the {0} recorded symbol(s), no more and no fewer" -f $measured.Count)
            # Six callbacks are static by design; if one ever leaks it becomes
            # part of the port's ABI, which is not a decision to make by accident.
            $leaked = @($measured | Where-Object { $_ -like 'shell_*' })
            if ($leaked.Count -gt 0) {
                Fail ("check 1: a lifecycle callback is no longer static: " + ($leaked -join ', '))
            }
        }
    }
}
Emit ""

# --- Check 2: target compile, diagnostics and sizes -------------------------
Begin-Check "Check 2  the adapter compiles on the target; diagnostics and section sizes match the record"
if (-not $targetAvailable) {
    Skip "check 2 (target compile): -TargetGcc not found"
} elseif (-not (Test-Path -LiteralPath $portTu -PathType Leaf)) {
    Fail ("check 2: the adapter TU is missing: {0}" -f $fe.PortTu)
} else {
    $probeObj = Join-Path $objRoot ('check2_' + $fe.PortTu.Replace('.c', '') + '.o')
    $base = Invoke-Native $TargetGcc (@($incs) + @($arch) + @($cflags) + @('-Wall', '-Wextra', '-o', $probeObj, $portTu))
    if ($base.Rc -ne 0) {
        Fail "check 2: the adapter does not compile on the target"
        Emit ("        " + (($base.Out -split "`r?`n" | Where-Object { $_ -match ':\d+:\d+: (error|fatal error)' } | Select-Object -First 4) -join "`n        "))
    } else {
        $got = @(Get-DiagnosticSet $base.Out)
        $frozen = @($fe.FrozenDiagnostics | Sort-Object)
        $gotText = ($got -join '|')
        $expText = ($frozen -join '|')
        if ($gotText -cne $expText) {
            Fail "check 2: the adapter's diagnostic set changed"
            Emit ("        frozen  : " + $(if ($expText) { $expText } else { '(none)' }))
            Emit ("        measured: " + $(if ($gotText) { $gotText } else { '(none)' }))
        }
        # The port never tolerates this class, upstream or not.
        $strictObj = Join-Path $objRoot ('check2_' + $fe.PortTu.Replace('.c', '') + '-strict.o')
        $strict = Invoke-Native $TargetGcc (@($incs) + @($arch) + @($cflags) + @('-Wall', '-Wextra', '-Werror=implicit-function-declaration', '-o', $strictObj, $portTu))
        if ($strict.Rc -ne 0) {
            Fail "check 2: the adapter has an implicit function declaration"
        }

        $sizes = Get-SectionSizes $probeObj
        foreach ($pair in @(@('.text', 'TextBytes'), @('.data', 'DataBytes'), @('.bss', 'BssBytes'), @('.rodata', 'RodataBytes'))) {
            $name = $pair[0]
            $recordedSize = $fe.($pair[1])
            $gotSize = if ($sizes.ContainsKey($name)) { $sizes[$name] } else { 0 }
            if ($gotSize -ne $recordedSize) {
                Fail ("check 2: {0} is {1} B, recorded {2} B -- the adapter or the shared lifecycle struct changed" -f $name, $gotSize, $recordedSize)
            }
        }
        if (Check-Clean) {
            Note ("compiles on the target; diagnostics match the frozen set ({0})" -f $(if ($expText) { $expText } else { 'none' }))
            Note "-Werror=implicit-function-declaration changes nothing"
            Note ("section sizes: .text={0} .data={1} .bss={2} .rodata={3} (as recorded)" -f `
                  $sizes['.text'], $sizes['.data'], $sizes['.bss'], $sizes['.rodata'])
        }
    }
}
Emit ""

# --- Check 3: the shared surface used, and the native-game rule -------------
Begin-Check "Check 3  the shared surface used is exactly the recorded one, and no rom_browser is reachable"
if (-not $targetAvailable) {
    Skip "check 3 (shared surface): -TargetGcc not found"
} elseif (-not (Test-Path -LiteralPath $portTu -PathType Leaf)) {
    Fail ("check 3: the adapter TU is missing: {0}" -f $fe.PortTu)
} else {
    $obj = Join-Path $objRoot ($fe.PortTu.Replace('.c', '') + '.o')
    if (-not (Test-Path -LiteralPath $obj)) {
        $r = Invoke-Native $TargetGcc (@($incs) + @($arch) + @($cflags) + @('-o', $obj, $portTu))
        if ($r.Rc -ne 0) { Fail "check 3: the adapter does not compile, so its undefined set cannot be read" }
    }
    if (Test-Path -LiteralPath $obj) {
        $undef = @(Get-UndefinedSymbols $obj | Sort-Object -Unique)
        $uText = ($undef -join '|')
        $wText = ($wantUndef -join '|')
        if ($uText -cne $wText) {
            Fail "check 3: the shared surface the adapter uses is not the recorded one"
            $extra = @($undef | Where-Object { $wantUndef -cnotcontains $_ })
            $missing = @($wantUndef | Where-Object { $undef -cnotcontains $_ })
            if ($extra.Count -gt 0) { Emit ("        extra   : " + ($extra -join ', ')) }
            if ($missing.Count -gt 0) { Emit ("        missing : " + ($missing -join ', ')) }
        } else {
            $romNamed = @($undef | Where-Object { $_ -like 'retropicoes_rom_*' })
            if ($romNamed.Count -gt 0) {
                Fail ("check 3: the adapter asks the linker for a ROM browser symbol: " + ($romNamed -join ', '))
            }
        }
    }

    # The rule the shared adoption gate enforces for S20, applied over the whole
    # port tree now: a single native game must not link rom_browser.c, and its
    # adapter must not reach one either. Code view only -- naming the file in a
    # comment is documentation.
    $romHits = New-Object System.Collections.Generic.List[string]
    foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File |
                    Where-Object { $_.Extension -in @('.c', '.h', '.cpp') })) {
        $code = Remove-CommentsAndStrings (Get-Content -LiteralPath $f.FullName -Raw)
        if ($code -match 'retropicoes_rom_[a-z_]*\s*\(') { [void]$romHits.Add($f.Name) }
        if ([regex]::IsMatch($code, '\b(draw_rom_menu|scan_rom_menu)\s*\(')) { [void]$romHits.Add($f.Name) }
    }
    if ($romHits.Count -gt 0) {
        Fail ("check 3: a port file calls a ROM browser function: " + ((@($romHits) | Sort-Object -Unique) -join ', '))
    }
    if (Check-Clean) {
        Note ("the adapter uses exactly the {0} recorded shared call(s); none of them is a ROM browser" -f $wantUndef.Count)
        Note "no port file calls a ROM browser function (code view, comments and strings removed)"
    }
}
Emit ""

# --- Check 4: the hook is actually wired ------------------------------------
Begin-Check "Check 4  the per-frame hook is called from the C9 event TU, and its SETTINGS_RAN bit is consumed"
$callerCode = Get-Text $callerTu
if ($null -eq $callerCode) {
    Fail ("check 4: the calling TU is missing: {0}" -f $fe.CallerTu)
} else {
    $code = Remove-CommentsAndStrings $callerCode
    if (-not [regex]::IsMatch($code, ('\b' + [regex]::Escape($fe.PumpSymbol) + '\s*\('))) {
        Fail ("check 4: {0} does not call {1} -- the hook exists but nothing runs it" -f $fe.CallerTu, $fe.PumpSymbol)
    }
    if (-not ($code -match ('RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN'))) {
        Fail "check 4: the caller does not look at the SETTINGS_RAN bit, so the engine's stale keyboard state is never reseeded"
    }
    if (-not [regex]::IsMatch($code, ('\b' + [regex]::Escape($fe.ResetSymbol) + '\s*\('))) {
        Fail ("check 4: the caller does not call {0}" -f $fe.ResetSymbol)
    }
    # The hook header must stay dependency-free: it is reached from the event TU,
    # whose surface is a measured contract. If it grows an include, this is where
    # that shows up rather than three phases later as an include-list change.
    $hookPath = Join-Path $portDir $fe.HookHeader
    $hookText = Get-Text $hookPath
    if ($null -eq $hookText) {
        Fail ("check 4: the hook header is missing: {0}" -f $fe.HookHeader)
    } else {
        $hookIncludes = @([regex]::Matches((Remove-Comments $hookText), '(?m)^[ \t]*#[ \t]*include\b.*') |
                          ForEach-Object { $_.Value.Trim() })
        if ($hookIncludes.Count -ne 0) {
            Fail ("check 4: {0} grew an include, so the event TU would acquire it: {1}" -f `
                  $fe.HookHeader, ($hookIncludes -join ' '))
        }
    }
    $includers = New-Object System.Collections.Generic.List[string]
    foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File |
                    Where-Object { $_.Extension -in @('.c', '.h') })) {
        $raw = Get-Content -LiteralPath $f.FullName -Raw
        if ($raw -match ('#\s*include\s*"' + [regex]::Escape($fe.HookHeader) + '"')) {
            [void]$includers.Add($f.Name)
        }
    }
    $gotIncText = ((@($includers) | Sort-Object -Unique) -join '|')
    $wantIncText = ((@($fe.HookIncluders) | Sort-Object -Unique) -join '|')
    if ($gotIncText -cne $wantIncText) {
        Fail "check 4: the hook header's includers moved"
        Emit ("        recorded: " + $wantIncText)
        Emit ("        measured: " + $gotIncText)
    }
    if (Check-Clean) {
        Note ("{0} calls the hook and reseeds the engine's keyboard view from the SETTINGS_RAN bit" -f $fe.CallerTu)
        Note ("{0} is included by exactly {1}, and has no includes of its own" -f $fe.HookHeader, $gotIncText)
    }
}
Emit ""

# --- Check 5: the host test, against the record -----------------------------
Begin-Check "Check 5  the host test passes and the contract it reports equals the recorded one"
if (-not $hostAvailable) {
    Skip "check 5 (host test): -HostGcc not found"
} elseif (-not (Test-Path -LiteralPath $portTest -PathType Leaf)) {
    Fail ("check 5: the host test is missing: {0}" -f $fe.PortTest)
} else {
    $outDir = Join-Path $Workspace '_deps\_probe'
    if (-not (Test-Path -LiteralPath $outDir)) { [void](New-Item -ItemType Directory -Path $outDir -Force) }
    $exe = Join-Path $outDir ('s20c-frontend' + $(if ($RunId) { '-' + $RunId } else { '' }) + '.exe')
    $hostIncs = @(
        '-I', $portDir,
        '-I', (Join-Path $Workspace 'compat'),
        '-I', (Join-Path $Workspace 'common\frontend'),
        '-I', (Join-Path $Workspace 'common\menu')
    )
    # frontend_shell.c is the REAL shared component: the test links it rather
    # than a copy, so the screen transitions it asserts are the board's.
    $shellSrc = Join-Path $Workspace 'common\frontend\frontend_shell.c'
    $build = Invoke-Native $HostGcc (@('-std=c99', '-O1', '-Wall', '-Wextra', '-Werror') + $hostIncs + `
        @('-o', $exe, $portTest, $portTu, $shellSrc))
    if ($build.Rc -ne 0) {
        Fail ("check 5: the host test did not compile (rc={0})" -f $build.Rc)
        Emit ("        " + (($build.Out -split "`r?`n" | Where-Object { $_ -match 'error' } | Select-Object -First 4) -join "`n        "))
    } else {
        $hostDiag = @(Get-DiagnosticSet $build.Out)
        $hostFrozen = @($fe.HostDiagnostics | Sort-Object)
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
        } elseif (-not $run.Out.Contains('REFKEEN FRONTEND ADAPTER: PASS')) {
            Fail "check 5: the host test did not print its PASS marker"
        } else {
            $failLines = @($run.Out -split "`r?`n" | Where-Object { $_ -like 'FAIL *' })
            if ($failLines.Count -gt 0) {
                Fail ("check 5: the host test reported {0} failed assertion(s)" -f $failLines.Count)
                foreach ($l in ($failLines | Select-Object -First 4)) { Emit ("        " + $l) }
            }

            # The test's own list of the adapter's symbols, taken by address --
            # independent of nm, which is what makes this two derivations.
            $testSymbols = New-Object System.Collections.Generic.List[string]
            foreach ($m in [regex]::Matches($run.Out, 'CONTRACT\s+symbol=([A-Za-z_][A-Za-z0-9_]*)')) {
                [void]$testSymbols.Add($m.Groups[1].Value)
            }
            if ($testSymbols.Count -eq 0) {
                Fail "check 5: the host test printed no per-symbol CONTRACT lines"
            } else {
                $tText = ((@($testSymbols) | Sort-Object -Unique) -join '|')
                if ($tText -cne $rText) {
                    Fail "check 5: the host test's symbol list disagrees with the record"
                    Emit ("        recorded: " + $rText)
                    Emit ("        test    : " + $tText)
                }
            }

            # Scalar contract values. Each of these would change if the shared
            # layer moved under the port; the gate's job is to say so instead of
            # letting the port drift with it.
            $scalars = @{}
            foreach ($m in [regex]::Matches($run.Out, 'CONTRACT\s+([a-z_]+)=(\d+)')) {
                $scalars[$m.Groups[1].Value] = [int]$m.Groups[2].Value
            }
            foreach ($pair in @(
                @('symbols', 'ExportedCount'),
                @('lifecycle_version', 'LifecycleVersion'),
                @('lifecycle_size', 'LifecycleSizeHost'),
                @('boot_stage_ok', 'BootStageOk'),
                @('map_resume', 'MapResume'),
                @('map_quit', 'MapQuit'),
                @('map_loader', 'MapLoader'),
                @('bit_idle', 'BitsIdle'),
                @('bit_settings', 'BitsSettings'),
                @('bit_quit', 'BitsQuit'),
                @('bit_loader', 'BitsLoader')
            )) {
                $key = $pair[0]
                $want = $fe.($pair[1])
                if (-not $scalars.ContainsKey($key)) {
                    Fail ("check 5: the host test printed no CONTRACT {0} line" -f $key)
                } elseif ($scalars[$key] -ne $want) {
                    Fail ("check 5: CONTRACT {0}={1}, recorded {2}" -f $key, $scalars[$key], $want)
                }
            }
            $mPath = [regex]::Match($run.Out, 'CONTRACT\s+settings_path=(\S+)')
            if (-not $mPath.Success) {
                Fail "check 5: the host test printed no settings_path line"
            } elseif ($mPath.Groups[1].Value -cne $fe.SettingsPath) {
                Fail ("check 5: the settings path is {0}, recorded {1}" -f $mPath.Groups[1].Value, $fe.SettingsPath)
            }
            $mAssert = [regex]::Match($run.Out, 'PASS \((\d+) assertions\)')
            if ($mAssert.Success -and [int]$mAssert.Groups[1].Value -ne $fe.HostAssertions) {
                Fail ("check 5: the host test made {0} assertions, recorded {1}" -f $mAssert.Groups[1].Value, $fe.HostAssertions)
            }
            if (Check-Clean) {
                Note ("the host test passes with {0} assertion(s) under -Werror, and its contract matches the record" -f $fe.HostAssertions)
                Note ("its symbol list is taken by address, so it is a second derivation of check 1's nm reading")
            }
        }
    }
}
Emit ""

# --- Check 6: the symbol the quit path names must be supplyable -------------
Begin-Check "Check 6  the quit path's primitive is named by the port and is defined only by a removed TU"
$quitSym = $fe.QuitPrimitive
$namedInPort = New-Object System.Collections.Generic.List[string]
foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File |
                Where-Object { $_.Extension -in @('.c', '.h') })) {
    $code = Remove-CommentsAndStrings (Get-Content -LiteralPath $f.FullName -Raw)
    if ([regex]::IsMatch($code, ('\b' + [regex]::Escape($quitSym) + '\s*\('))) {
        [void]$namedInPort.Add($f.Name)
    }
}
$namedText = ((@($namedInPort) | Sort-Object -Unique) -join '|')
$wantNamed = ((@($fe.QuitPrimitiveNamingTus) | Sort-Object -Unique) -join '|')
if ($namedText -cne $wantNamed) {
    Fail "check 6: the set of port files naming the quit primitive moved"
    Emit ("        recorded: " + $wantNamed)
    Emit ("        measured: " + $namedText)
}

# Definition index, by name and shape, over upstream's .c files. A declaration
# (line ends in ';') is not a definition -- the distinction the C12 round got
# wrong by reading text.
$definers = New-Object System.Collections.Generic.List[string]
foreach ($f in (Get-ChildItem -LiteralPath $upSrc -Recurse -File -Filter '*.c')) {
    foreach ($line in (Get-Content -LiteralPath $f.FullName)) {
        if ($line -match ('^\s*(void|int|bool|static)\s+' + [regex]::Escape($quitSym) + '\s*\(') -and
            $line -notmatch ';\s*$') {
            # Same trap as the message formatting above: an operator that takes
            # its own comma-separated arguments, written inside Add(), is read as
            # Add()'s argument list. Computed first, then added.
            #
            # Reported relative to the UPSTREAM ROOT, not to src/, so the string
            # is the same shape as the PortRemovedTus keys it is compared with --
            # one fact, one form, no translation for a reader to get wrong.
            $relTu = $f.FullName.Substring($upSrc.Length).TrimStart('\', '/')
            $relTu = 'src/' + ($relTu -replace '\\', '/')
            [void]$definers.Add($relTu)
        }
    }
}
$definers = @($definers | Sort-Object -Unique)
if ($definers.Count -ne 1) {
    Fail ("check 6: {0} has {1} defining TU(s) in upstream's src; expected exactly 1" -f $quitSym, $definers.Count)
    foreach ($d in $definers) { Emit ("        " + $d) }
} elseif ($definers[0] -cne $fe.QuitPrimitiveTu) {
    Fail ("check 6: {0} is defined in {1}, recorded {2}" -f $quitSym, $definers[0], $fe.QuitPrimitiveTu)
} else {
    $removed = @($facts.AltControlStub.PortRemovedTus.Keys)
    if ($removed -cnotcontains $definers[0]) {
        Fail ("check 6: {0} is defined by a TU the port still builds, so naming it is a redefinition" -f $definers[0])
    }
    if (-not $fe.PortSuppliedSymbols.Contains($quitSym)) {
        Fail ("check 6: the port names {0} but does not record itself as its supplier" -f $quitSym)
    }
    # And the authoritative half: the removed TU really does define it, read off
    # a compiled object rather than off the text above.
    #
    # The HOST compiler with the pinned real SDL3 is what does it, and the reason
    # is not convenience: be_startup_sdl.c is a full SDL platform TU, and the
    # port's shim is a minimal replacement for the handful of SDL entry points
    # the portable layer uses -- it cannot compile this file, and it is not
    # supposed to. The host baseline (Build-HostBaseline.ps1) builds it against
    # real SDL3, so that is the toolchain where the definition can be observed.
    # The claim being checked is a NAME, not a layout, so the host's pointer
    # width is irrelevant here.
    if (-not $hostAvailable) {
        Skip "check 6 (definition in the object): -HostGcc not found"
    } else {
        $sdlInc = Join-Path $Workspace $fe.Sdl3HostInclude
        if (-not (Test-Path -LiteralPath $sdlInc -PathType Container)) {
            Skip ("check 6 (definition in the object): the pinned SDL3 include dir is missing: {0}" -f $fe.Sdl3HostInclude)
        } else {
            $tuPath = Join-Path $UpstreamRoot ($definers[0] -replace '/', '\')
            $qObj = Join-Path $objRoot 'check6_quittu.o'
            $hostNm = Join-Path (Split-Path -Parent $HostGcc) 'nm.exe'
            $qr = Invoke-Native $HostGcc (@(
                '-I', $sdlInc,
                '-I', (Join-Path $Workspace 'refkeen\host\include'),
                '-I', $upSrc,
                '-I', (Join-Path $upSrc 'crc32'),
                '-I', (Join-Path $upSrc 'depklite'),
                '-I', (Join-Path $upSrc 'unlzexe'),
                '-I', (Join-Path $upSrc 'kdreams'),
                '-I', (Join-Path $upSrc 'kdreams\lscr')
            ) + @($fe.QuitTuDefines) + @('-include', 'string.h', '-c', '-std=c99', '-fno-common',
                                       '-O0', '-fmessage-length=0', '-o', $qObj, $tuPath))
            if ($qr.Rc -ne 0) {
                Fail ("check 6: the defining TU does not compile on the host against real SDL3: {0}" -f $definers[0])
                Emit ("        " + (($qr.Out -split "`r?`n" | Where-Object { $_ -match 'error' } | Select-Object -First 3) -join "`n        "))
            } else {
                $rq = Invoke-Native $hostNm @('--defined-only', '-g', $qObj)
                $defined = @($rq.Out -split "`r?`n" | ForEach-Object {
                    $parts = @($_.Trim() -split '\s+' | Where-Object { $_ -ne '' })
                    if ($parts.Count -ge 2) { $parts[$parts.Count - 1] }
                })
                if ($defined -cnotcontains $quitSym) {
                    Fail ("check 6: {0} is not among the defining TU's exports, so the port has nothing to replace" -f $quitSym)
                }
            }
        }
    }
    if (Check-Clean) {
        Note ("{0} is named by {1} and defined only in {2}, which the port removes -- so the port must supply it" -f `
              $quitSym, $namedText, $definers[0])
        Note ("the port's supplier obligation is recorded in Frontend.PortSuppliedSymbols, for C13 to satisfy at link time")
    }
}
Emit ""

# --- Check 7: the contracts the shared layer will apply --------------------
Begin-Check "Check 7  the runtime declarations agree with the sibling ports, and the adoption gate's rules already hold"
$mismatched = New-Object System.Collections.Generic.List[string]
$missing = New-Object System.Collections.Generic.List[string]
foreach ($decl in $fe.RuntimeDecls.Keys) {
    $signature = $fe.RuntimeDecls[$decl]
    $found = 0
    foreach ($header in $fe.SiblingRuntimeHeaders) {
        $hp = Join-Path $Workspace $header
        $text = Get-Text $hp
        if ($null -eq $text) {
            [void]$missing.Add($header)
            continue
        }
        $code = Remove-Comments $text
        foreach ($line in ($code -split "`r?`n")) {
            if ($line -match ('\b' + [regex]::Escape($decl) + '\s*\(')) {
                $norm = ($line -replace '\s+', ' ').Trim()
                if ($norm -notmatch [regex]::Escape($signature)) {
                    # Built in its own statement, not inline in Add(): with the
                    # format expression inside the call, the commas are read as
                    # Add()'s own argument separators and the call fails with
                    # "no overload for Add with 2 arguments" -- a message that
                    # says nothing about the actual mistake (MEMORY.md, S20-B).
                    $mismatchText = "{0}: {1} says [{2}], this port says [{3}]" -f `
                        $header, $decl, $norm, $signature
                    [void]$mismatched.Add($mismatchText)
                } else {
                    $found++
                }
            }
        }
    }
    if ($found -eq 0 -and $missing.Count -eq 0) {
        [void]$missing.Add($decl)
    }
}
if ($mismatched.Count -gt 0) {
    foreach ($m in $mismatched) { Fail ("check 7: " + $m) }
}
if ($missing.Count -gt 0) {
    # Known limitation, and it is about attribution rather than detection: when a
    # sibling header file is not there at all, this single list ends up holding
    # file paths instead of the names those files could not declare, so the
    # per-declaration answer is suppressed. The check still fails, so the defect
    # is reported; only the wording is less precise than it could be. Named here
    # rather than left to be rediscovered, and carried as a follow-up item in
    # the round's notes.
    Fail ("check 7: no sibling declaration found for: " + ((@($missing) | Sort-Object -Unique) -join ', '))
}
# The port's own header has to declare them too -- and with the SAME signature,
# which is the half the sibling cross-check above cannot cover. The C linker
# does not compare signatures: a port that declared
# `retropicoes_video_set_scanlines(long)` would compile, link, and hand the
# runtime a value it never reads the way this port believes it does. Before
# this loop was written, the port's header was only searched for the NAME, so
# that class of drift was a false green -- found by writing the negative-path
# self test, where the case that drifts this declaration must be able to fail.
$adapterHeader = Get-Text (Join-Path $portDir 'frontend.h')
foreach ($decl in $fe.RuntimeDecls.Keys) {
    $signature = $fe.RuntimeDecls[$decl]
    if ($null -eq $adapterHeader) {
        Fail ("check 7: the port has no frontend.h, so it cannot declare {0}" -f $decl)
        continue
    }
    # Last match wins: a header that declares the same name twice is a defect of
    # its own, and the later declaration is the one the compiler would use.
    $declared = $null
    foreach ($line in ((Remove-Comments $adapterHeader) -split "`r?`n")) {
        if ([regex]::IsMatch($line, ('\b' + [regex]::Escape($decl) + '\s*\('))) {
            $declared = ($line -replace '\s+', ' ').Trim()
        }
    }
    if ($null -eq $declared) {
        Fail ("check 7: the port does not declare {0} at all" -f $decl)
    } elseif ($declared -notmatch [regex]::Escape($signature)) {
        # Own statement first: with the format expression inline in Fail(), the
        # commas are read as Fail()'s argument separators (MEMORY.md, S20-B).
        $driftText = "the port declares [{0}], the recorded interface is [{1}]" -f $declared, $signature
        Fail ("check 7: " + $driftText)
    }
}

# The shared adoption gate's own two rules, applied over this tree now. Its S20
# arm is skipped until refkeen/pico/CMakeLists.txt exists, so a violation would
# otherwise be found only after C13 -- by which time the adapter is written.
$violations = New-Object System.Collections.Generic.List[string]
foreach ($f in (Get-ChildItem -LiteralPath (Join-Path $Workspace 'refkeen') -Recurse -File |
                Where-Object { $_.Name -match '\.(c|cpp|h|tmpl)$' })) {
    $raw = Get-Content -LiteralPath $f.FullName -Raw
    if ($raw -match '(?i)select\s*\+\s*start|start\s*\+\s*select|long.?press.{0,20}start|hold.{0,20}start') {
        [void]$violations.Add('legacy menu gesture in ' + $f.Name)
    }
    if ($raw -match '\b(draw_rom_menu|scan_rom_menu)\s*\(') {
        [void]$violations.Add('private ROM browser in ' + $f.Name)
    }
}
foreach ($v in ($violations | Sort-Object -Unique)) { Fail ("check 7: " + $v) }

# The CMake contract, if C13 has landed. Skipped loudly while it has not, exactly
# as the shared gate does -- not silently passed.
$cmakeRel = 'refkeen/pico/CMakeLists.txt'
$cmakePath = Join-Path $Workspace $cmakeRel
if (-not (Test-Path -LiteralPath $cmakePath -PathType Leaf)) {
    Skip ("check 7 (CMake contract): {0} does not exist yet -- C13" -f $cmakeRel)
} else {
    $body = Get-Content -LiteralPath $cmakePath -Raw
    foreach ($tok in $fe.CmakeTokens) {
        if (-not $body.Contains($tok)) { Fail ("check 7: {0} must include {1}" -f $cmakeRel, $tok) }
    }
    if ($body.Contains('rom_browser.c')) {
        Fail ("check 7: {0} links rom_browser.c, which a single native game must not" -f $cmakeRel)
    }
    if ($body.Contains('pico_shared/menu.cpp')) {
        Fail ("check 7: {0} re-links the unused legacy pico_shared menu" -f $cmakeRel)
    }
}
if (Check-Clean) {
    Note ("the {0} runtime declaration(s) match digger's and xrick's copies, and the port's own header declares the recorded signatures" -f @($fe.RuntimeDecls.Keys).Count)
    Note "no legacy menu gesture and no private ROM browser in the port tree"
}
Emit ""

Emit "---------------------------------------------------------------------------"
if ($script:Problems.Count -eq 0) {
    $suffix = if ($script:Skipped.Count -gt 0) { " -- $($script:Skipped.Count) check(s) SKIPPED, see above" } else { '' }
    Emit ("FRONTEND ADAPTER: PASS (7 checks){0}" -f $suffix)
    Flush-Report
    exit 0
}
Emit ("FRONTEND ADAPTER: FAILED ({0} problem(s))" -f $script:Problems.Count)
Flush-Report
exit 1
