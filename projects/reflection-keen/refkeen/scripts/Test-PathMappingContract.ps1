<#
.SYNOPSIS
    S20-C path mapping contract gate.

.DESCRIPTION
    Answers one question: does the port put the engine's files where the port
    itself, the shared medium layout, and upstream all agree they should go?

    The port cannot test this on the target, so the gate does the next best
    thing: it walks the chain and refuses to pass unless every link agrees.

      upstream bounds (be_filesystem_len_bounds.h)
        |
      port constants (compiled truth, printed by the host test)
        |
      upstream per-version dirs (be_gamestr.h BE_STR_GAMEVER_KDREAMS*)
        |
      upstream required game files (be_gamedefs_kdreams.h)
        |
      the set of path roots upstream actually uses as a searchdir
        |
      the set of directories upstream actually calls BEL_Cross_mkdir on

    Two of those links are the ones that silently rot: a new root appearing in
    upstream (a fourth path variable nobody mapped), and a new mkdir target the
    port has to be able to create. Both are checked here, not assumed.

    Four quadrants are exercised: known values, independent implementations of
    the same facts (upstream's own headers, and the project's documented
    /data/refkeen convention), a negative path (-SelfTest, one injected defect
    per check), and a release path (the real tree plus a real host-test build).

.PARAMETER SelfTest
    Mirror the real sources into a scratch tree, confirm the clean copy passes,
    then inject one defect per check and require exactly that check to fire. A
    gate that cannot fail is worse than none.

.PARAMETER RunId
    Suffix for the scratch trees. Pass one to reproduce a specific run; a fresh
    one is generated otherwise. Nothing is ever deleted and re-created -- the
    project's sandbox redirects recursive deletes to a fail-closed trash, so
    every run gets a brand new directory instead.

.EXAMPLE
    & refkeen/scripts/Test-PathMappingContract.ps1 `
        -Gcc E:\LVGL\...\gcc.exe -ReportPath .emv2\logs\s20-path-mapping.txt

.EXAMPLE
    & refkeen/scripts/Test-PathMappingContract.ps1 -SelfTest -RunId r1
#>
[CmdletBinding()]
param(
    [string]$Workspace = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$Gcc,
    [string]$ReportPath,
    [switch]$SelfTest,
    [string]$RunId = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$failures = New-Object System.Collections.Generic.List[string]
$notes    = New-Object System.Collections.Generic.List[string]
$skipped  = New-Object System.Collections.Generic.List[string]

function Add-Failure([string]$message) { [void]$failures.Add($message) }
function Add-Note([string]$message)    { [void]$notes.Add($message) }
function Add-Skipped([string]$message) { [void]$skipped.Add($message) }

function Get-Text([string]$relativeOrAbsolute) {
    $path = if ([IO.Path]::IsPathRooted($relativeOrAbsolute)) {
        $relativeOrAbsolute
    } else {
        Join-Path $Workspace $relativeOrAbsolute
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    return Get-Content -LiteralPath $path -Raw
}

# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------

# "#define BE_CROSS_PATH_LEN_BOUND 256" -> 256
function Get-DefineInt([string]$text, [string]$name) {
    if ([string]::IsNullOrEmpty($text)) { return $null }
    $m = [regex]::Match($text, "#define\s+$([regex]::Escape($name))\s+([0-9]+)\b")
    if (-not $m.Success) { return $null }
    return [int]$m.Groups[1].Value
}

# "#define RETROPICOES_PATH_GAME_DATA_BASE \"/data/refkeen\"" -> /data/refkeen
function Get-DefineString([string]$text, [string]$name) {
    if ([string]::IsNullOrEmpty($text)) { return $null }
    $m = [regex]::Match($text, "#define\s+$([regex]::Escape($name))\s+`"([^`"]*)`"")
    if (-not $m.Success) { return $null }
    return $m.Groups[1].Value
}

# be_gamestr.h: BE_STR_GAMEVER_KDREAMSE100 "kdreamse100"
function Get-GameStrValues([string]$text) {
    $map = @{}
    if ([string]::IsNullOrEmpty($text)) { return ,$map }
    foreach ($m in [regex]::Matches($text, '#define\s+BE_STR_GAMEVER_(KDREAMS[A-Z0-9_]*)\s+"([^"]*)"')) {
        $map[$m.Groups[1].Value] = $m.Groups[2].Value
    }
    return ,$map
}

# The required-file tables in be_gamedefs_kdreams.h are the only entries shaped
# {"NAME", bytes, 0xCRC}. Returns the distinct set of names, and separately any
# entry that uses a "|" alias list, which this parser does not expand.
function Get-RequiredFileNames([string]$text) {
    $names = New-Object System.Collections.Generic.List[string]
    $aliases = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrEmpty($text)) { return @{ Names = $names; Aliases = $aliases } }
    foreach ($m in [regex]::Matches($text, '\{\s*"([^"]+)"\s*,\s*[0-9]+\s*,\s*0x[0-9A-Fa-f]+\s*\}')) {
        $value = $m.Groups[1].Value
        if ($value.Contains('|')) {
            if (-not $aliases.Contains($value)) { [void]$aliases.Add($value) }
        } elseif (-not $names.Contains($value)) {
            [void]$names.Add($value)
        }
    }
    $sorted = @($names | Sort-Object)
    return @{ Names = $sorted; Aliases = $aliases }
}

# Port host test: 'CONTRACT key=value' lines. Returns key -> list of values, so
# a repeated key (version_dir, game_file, save_name, role) accumulates.
function Get-ContractLines([string]$stdout) {
    $map = @{}
    if ([string]::IsNullOrEmpty($stdout)) { return ,$map }
    foreach ($rawLine in ($stdout -split "`r?`n")) {
        $m = [regex]::Match($rawLine.Trim(), '^CONTRACT\s+([a-z_]+)=(.*)$')
        if (-not $m.Success) { continue }
        $key = $m.Groups[1].Value
        $value = $m.Groups[2].Value
        if (-not $map.ContainsKey($key)) {
            $map[$key] = New-Object System.Collections.Generic.List[string]
        }
        [void]$map[$key].Add($value)
    }
    return ,$map
}

# Every path root an upstream file hands to the file action layer, as a
# searchdir. Two shapes matter and both are read here:
#
#   BEL_Cross_apply_file_action_in_dir(<name>, <request>, <ROOT>, <out>)   direct
#   BEL_Cross_OpenMatchingGameFileForReading(<names>, <ROOT>)              indirect
#
# The indirect form is how instPath reaches the same code, so a check that only
# looked at the direct calls would have a hole exactly where the game data
# lives.
#
# Function definitions and declarations match the same shape, so any captured
# text that reads like a parameter declaration is dropped. If that filter ever
# removes too much the caller's sanity floor turns the check red, rather than
# letting it pass on an empty set.
$script:TypeKeywords = 'const|volatile|static|struct|enum|union|unsigned|signed|TCHAR|char|short|int|long|float|double|void|bool|BE_FILE_T|BE_DIR_T'

function Test-LooksLikeDeclaration([string]$expr) {
    return ($expr -match "(^|\s)($($script:TypeKeywords))\b")
}

function Get-SearchDirExpressions([string]$text) {
    $found = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrEmpty($text)) { return ,$found }

    $patterns = @(
        'BEL_Cross_apply_file_action_in_dir\s*\(\s*[^,]+,\s*[^,]+,\s*([^,]+),',
        'BEL_Cross_OpenMatchingGameFileForReading\s*\(\s*[^,]+,\s*([^)]+)\)'
    )
    foreach ($pattern in $patterns) {
        foreach ($m in [regex]::Matches($text, $pattern)) {
            $expr = $m.Groups[1].Value.Trim()
            if ([string]::IsNullOrEmpty($expr)) { continue }
            if (Test-LooksLikeDeclaration $expr) { continue }
            if (-not $found.Contains($expr)) { [void]$found.Add($expr) }
        }
    }
    return ,$found
}

# Every BEL_Cross_mkdir(<arg>) argument in a file. The header's own definition
# is excluded by requiring ".c" as caller, which the caller loop guarantees.
function Get-MkdirTargets([string]$text) {
    $found = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrEmpty($text)) { return ,$found }
    foreach ($m in [regex]::Matches($text, 'BEL_Cross_mkdir\s*\(\s*([^)]+?)\s*\)\s*;')) {
        $target = $m.Groups[1].Value.Trim()
        if (-not $found.Contains($target)) { [void]$found.Add($target) }
    }
    return ,$found
}

# ---------------------------------------------------------------------------
# The chain
# ---------------------------------------------------------------------------

# Documented medium layout, and the only roots upstream is allowed to search.
# Every entry has to be reachable from the port's role table or be deliberately
# unmapped; check 5 fails on anything new.
$script:KnownSearchDirs = @(
    'g_be_selectedGameInstallation->writableFilesPath',
    'g_be_modPath',
    'g_be_appNewCfgPath',
    'g_be_selectedGameInstallation->instPath',
    # The last two are function parameters rather than globals, in
    # be_filesystem_gameinst.c. They are fed by the callers above, so they can
    # only carry a root the port has already seen -- but they are listed
    # explicitly so that a *new* global appearing there turns check 5 red.
    'path',
    'searchdir'
)

# Roots the port must be able to create (BEL_Cross_mkdir is NON-recursive, so
# each of these has to be creatable on its own -- see check 6).
$script:KnownMkdirTargets = @(
    'g_be_appDataPath',
    'gameInstallation->writableFilesPath',
    'g_be_appNewCfgPath'
)

# Android-only call sites are compiled out for this port and are named here so
# that check 6 stays honest about why they are not required.
$script:OutOfScopeMkdirTargets = @('externalStoragePath', 'path')

function Invoke-Contract([hashtable]$sources) {
    # The port values come from the compiled host test, not from re-evaluating
    # macros in PowerShell. An absent contract block is a hard failure: it would
    # otherwise turn every downstream comparison into a silent skip.
    $contract = Get-ContractLines $sources.HostStdout
    $haveContract = ($contract.Count -gt 0)
    if (-not $haveContract) {
        Add-Failure ('check 1: the host test produced no CONTRACT lines, so the port side ' +
                     'of every comparison is unavailable')
    }

    # --- Check 1: length bounds agree with upstream ------------------------
    $upstreamMedium = Get-DefineInt $sources.Bounds 'BE_CROSS_PATH_LEN_BOUND'
    $upstreamDos    = Get-DefineInt $sources.Bounds 'BE_CROSS_DOS_FILENAME_LEN_BOUND'
    if ($null -eq $upstreamMedium -or $null -eq $upstreamDos) {
        Add-Failure 'check 1: could not parse be_filesystem_len_bounds.h'
    } elseif (-not $haveContract) {
        Add-Skipped 'check 1 (bounds): port side unavailable'
    } else {
        $check1Before = $failures.Count
        $portMedium = [int]$contract['medium_bound'][0]
        $portDos    = [int]$contract['dos_name_bound'][0]
        $portDosMax = [int]$contract['dos_name_max'][0]

        if ($portMedium -ne $upstreamMedium) {
            Add-Failure "check 1: port medium bound $portMedium, upstream $upstreamMedium"
        }
        if ($portDos -ne $upstreamDos) {
            Add-Failure "check 1: port DOS name bound $portDos, upstream $upstreamDos"
        }
        if ($portDosMax -ne ($portDos - 1)) {
            Add-Failure ("check 1: dos_name_max $portDosMax is not dos_name_bound - 1 " +
                         "($portDos) -- the two conventions have been mixed up again")
        }
        $longMax = [int]$contract['long_name_max'][0]
        if (($longMax + 1) -ge $portMedium) {
            Add-Failure ("check 1: long_name_max $longMax + the NUL cannot be squeezed " +
                         "into the medium bound $portMedium")
        }
        if ($failures.Count -eq $check1Before) {
            Add-Note ("check 1 ok  bounds agree with upstream: medium $portMedium, " +
                      "DOS name $portDos (max $portDosMax), long name $longMax")
        }
    }

    # --- Check 2: medium layout, against the project's own convention ------
    if (-not $haveContract) {
        Add-Skipped 'check 2 (medium layout): port side unavailable'
    } else {
        $check2Before = $failures.Count
        $gameData = $contract['game_data_base'][0]
        $saveRoot = $contract['save_root_base'][0]

        # The data root convention is independently recorded by the data
        # pre-flight gate; tying the two together means the port constant and
        # the SD layout instruction cannot drift apart.
        if (-not [string]::IsNullOrEmpty($sources.Preflight)) {
            if (-not $sources.Preflight.Contains($gameData + '/')) {
                Add-Failure ("check 2: the port game data base '$gameData' is not the " +
                             "'$gameData/' convention the data pre-flight gate documents")
            }
        } else {
            Add-Skipped 'check 2 (data root convention): Preflight-KeenData.ps1 not found'
        }

        if ($gameData -ceq $saveRoot) {
            Add-Failure 'check 2: the read-only game data root and the writable root are the same'
        }
        foreach ($base in @($gameData, $saveRoot)) {
            if ($base.EndsWith('/')) {
                Add-Failure "check 2: base '$base' has a trailing separator; the composer adds one"
            }
            if (-not $base.StartsWith('/')) {
                Add-Failure "check 2: base '$base' is not absolute"
            }
        }

        # writableFilesPath is upstream's appDataPath plus the version dir, so
        # the save files must NOT live directly under the save root.
        $writableRole = @($contract['role'] | Where-Object { $_ -match 'writable=1' })
        if ($writableRole.Count -ne 1) {
            Add-Failure "check 2: expected exactly one writable role, found $($writableRole.Count)"
        } else {
            $roleBase = ([regex]::Match($writableRole[0], 'base=(\S+)')).Groups[1].Value
            if ($roleBase -cne $saveRoot) {
                Add-Failure "check 2: the writable role's base '$roleBase' is not the save root '$saveRoot'"
            }
        }
        if ($failures.Count -eq $check2Before) {
            Add-Note "check 2 ok  layout: game data '$gameData' (read-only), writable '$saveRoot'"
        }
    }

    # --- Check 3: per-version save directories ----------------------------
    $upstreamDirs = Get-GameStrValues $sources.GameStr
    if ($upstreamDirs.Count -eq 0) {
        Add-Failure 'check 3: could not parse BE_STR_GAMEVER_KDREAMS* from be_gamestr.h'
    } elseif (-not $haveContract) {
        Add-Skipped 'check 3 (version dirs): port side unavailable'
    } else {
        $check3Before = $failures.Count
        $portedDirs = @($contract['version_dir'] | Sort-Object)
        $expectedDirs = @($upstreamDirs.Values | Sort-Object)

        foreach ($dir in $expectedDirs) {
            if ($portedDirs -cnotcontains $dir) {
                Add-Failure "check 3: upstream version dir '$dir' is not declared by the host test"
            }
        }
        foreach ($dir in $portedDirs) {
            if ($expectedDirs -cnotcontains $dir) {
                Add-Failure "check 3: host test declares version dir '$dir', upstream has no such version"
            }
        }
        $saveRoot = if ($haveContract) { $contract['save_root_base'][0] } else { '' }
        $longMax  = if ($haveContract) { [int]$contract['long_name_max'][0] } else { 0 }
        $medium   = if ($haveContract) { [int]$contract['medium_bound'][0] } else { 0 }
        foreach ($dir in $portedDirs) {
            $full = "$saveRoot/$dir"
            if ($full.Length -gt $medium) {
                Add-Failure "check 3: '$full' exceeds the medium bound $medium"
            }
            if ($dir.Length -gt $longMax) {
                Add-Failure "check 3: version dir '$dir' exceeds the save-root name bound $longMax"
            }
        }
        if ($failures.Count -eq $check3Before) {
            Add-Note ("check 3 ok  all $($expectedDirs.Count) version dirs agree and compose " +
                      "under $saveRoot")
        }
    }

    # --- Check 4: the required game file names ----------------------------
    $parsed = Get-RequiredFileNames $sources.KdGameDefs
    $upstreamFiles = @($parsed.Names)
    if ($parsed.Aliases.Count -gt 0) {
        Add-Failure ("check 4: be_gamedefs_kdreams.h now uses '|' alias lists " +
                     "($($parsed.Aliases -join ', ')); this parser does not expand them and " +
                     'must not be trusted until it does')
    }
    if ($upstreamFiles.Count -eq 0) {
        Add-Failure 'check 4: could not parse any required Keen Dreams file name'
    } elseif (-not $haveContract) {
        Add-Skipped 'check 4 (game file names): port side unavailable'
    } else {
        $check4Before = $failures.Count
        $portedFiles = @($contract['game_file'] | Sort-Object)
        $dosMax = if ($haveContract) { [int]$contract['dos_name_max'][0] } else { 0 }

        foreach ($name in $upstreamFiles) {
            if ($portedFiles -cnotcontains $name) {
                Add-Failure "check 4: upstream requires '$name', which the host test does not declare"
            }
            if ($name.Length -gt $dosMax) {
                Add-Failure "check 4: required file '$name' exceeds the DOS name bound $dosMax"
            }
        }
        foreach ($name in $portedFiles) {
            if ($upstreamFiles -cnotcontains $name) {
                Add-Failure "check 4: host test declares game file '$name', upstream requires no such file"
            }
        }
        if ($failures.Count -eq $check4Before) {
            Add-Note ("check 4 ok  all $($upstreamFiles.Count) required file names agree and fit " +
                      "the DOS name bound")
        }
    }

    # --- Check 5: no unmapped search root ---------------------------------
    $searchDirs = New-Object System.Collections.Generic.List[string]
    foreach ($key in @('FileOps', 'GameInst')) {
        foreach ($expr in (Get-SearchDirExpressions $sources[$key])) {
            if (-not $searchDirs.Contains($expr)) { [void]$searchDirs.Add($expr) }
        }
    }
    # Parse sanity floor: upstream uses six distinct expressions today (four
    # globals and two function parameters). An empty or shrunken set means the
    # parser stopped matching, which must not read as "nothing unmapped".
    if ($searchDirs.Count -lt 5) {
        Add-Failure ("check 5: parsed only $($searchDirs.Count) search root expression(s); " +
                     'the parser has stopped keeping up with upstream and cannot be trusted')
    } else {
        $unknown = New-Object System.Collections.Generic.List[string]
        foreach ($expr in $searchDirs) {
            if ($script:KnownSearchDirs -cnotcontains $expr) {
                [void]$unknown.Add($expr)
            }
        }
        if ($unknown.Count -gt 0) {
            Add-Failure ("check 5: the file system layer searches " +
                         "'$($unknown -join "', '")', which the port has not mapped")
        } else {
            Add-Note ("check 5 ok  all $($searchDirs.Count) search root expressions the engine " +
                      'uses are accounted for')
        }
    }

    # --- Check 6: every directory the port must be able to create ----------
    $check6Before = $failures.Count
    $mkdirTargets = New-Object System.Collections.Generic.List[string]
    foreach ($key in @('FileOps', 'GameInst', 'AppPaths')) {
        foreach ($target in (Get-MkdirTargets $sources[$key])) {
            if ($script:OutOfScopeMkdirTargets -ccontains $target) { continue }
            if (-not $mkdirTargets.Contains($target)) { [void]$mkdirTargets.Add($target) }
        }
    }
    if ($mkdirTargets.Count -eq 0) {
        Add-Failure 'check 6: found no BEL_Cross_mkdir call sites -- parse is suspect'
    } else {
        # Set equality, not a subset test. Only checking for *unknown* targets
        # would miss a directory upstream stopped creating (or that the parser
        # stopped seeing), which is precisely the case an injected-removal
        # self-test exercises. Both directions are therefore required to agree.
        $unknown = New-Object System.Collections.Generic.List[string]
        foreach ($target in $mkdirTargets) {
            if ($script:KnownMkdirTargets -cnotcontains $target) { [void]$unknown.Add($target) }
        }
        $missing = New-Object System.Collections.Generic.List[string]
        foreach ($target in $script:KnownMkdirTargets) {
            if (-not $mkdirTargets.Contains($target)) { [void]$missing.Add($target) }
        }
        if ($unknown.Count -gt 0) {
            Add-Failure ("check 6: upstream calls BEL_Cross_mkdir on " +
                         "'$($unknown -join "', '")', which the port has not planned for")
        }
        if ($missing.Count -gt 0) {
            Add-Failure ("check 6: expected BEL_Cross_mkdir on " +
                         "'$($missing -join "', '")' but upstream no longer creates it -- the " +
                         'port plan and upstream have drifted apart in either direction')
        }
        if ($failures.Count -eq $check6Before) {
            Add-Note ("check 6 ok  all $($mkdirTargets.Count) directory targets are non-recursive " +
                      'and exactly match the port plan')
        }
    }
}

# ---------------------------------------------------------------------------
# Source loading
# ---------------------------------------------------------------------------

$script:SourceFiles = @{
    Bounds     = '_deps/reflectionhle-upstream/src/backend/filesystem/be_filesystem_len_bounds.h'
    GameStr    = '_deps/reflectionhle-upstream/src/be_gamestr.h'
    KdGameDefs = '_deps/reflectionhle-upstream/src/backend/gamedefs/be_gamedefs_kdreams.h'
    FileOps    = '_deps/reflectionhle-upstream/src/backend/filesystem/be_filesystem_file_ops.c'
    GameInst   = '_deps/reflectionhle-upstream/src/backend/filesystem/be_filesystem_gameinst.c'
    AppPaths   = '_deps/reflectionhle-upstream/src/backend/filesystem/be_filesystem_app_paths.c'
}

function Get-Sources([string]$root, [string]$hostStdout) {
    $text = @{}
    foreach ($key in $script:SourceFiles.Keys) {
        $text[$key] = Get-Text (Join-Path $root $script:SourceFiles[$key])
    }
    $text.HostStdout = $hostStdout
    $text.Preflight = Get-Text (Join-Path $root 'refkeen/scripts/Preflight-KeenData.ps1')
    return $text
}

# ---------------------------------------------------------------------------
# Host test build (the only way to obtain the port's compiled numbers)
# ---------------------------------------------------------------------------

function Invoke-HostTest([string]$root) {
    if ([string]::IsNullOrEmpty($Gcc) -or -not (Test-Path -LiteralPath $Gcc -PathType Leaf)) {
        return @{ Stdout = $null; Error = '-Gcc not supplied or not found' }
    }
    $outDir = Join-Path $root '_deps/_probe'
    if (-not (Test-Path -LiteralPath $outDir)) {
        [void](New-Item -ItemType Directory -Path $outDir -Force)
    }
    $exe = Join-Path $outDir 's20c-path-test-gate.exe'
    $gccArgs = @(
        '-std=c99', '-O1', '-Wall', '-Wextra', '-Werror',
        '-I', (Join-Path $root 'refkeen/platform/retropicoes'),
        '-o', $exe,
        (Join-Path $root 'refkeen/tests/test_path_mapping.c'),
        (Join-Path $root 'refkeen/platform/retropicoes/path_mapping.c')
    )
    $buildLog = & $Gcc @gccArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        return @{ Stdout = $null; Error = "the host test did not compile: $($buildLog -join ' ')" }
    }
    $runLog = & $exe 2>&1
    $stdout = $runLog -join "`n"
    if ($LASTEXITCODE -ne 0) {
        return @{ Stdout = $stdout; Error = "the host test exited $LASTEXITCODE" }
    }
    if (-not $stdout.Contains('REFKEEN PATH MAPPING: PASS')) {
        return @{ Stdout = $stdout; Error = 'the host test did not print its PASS marker' }
    }
    return @{ Stdout = $stdout; Error = $null }
}

# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

# The scratch tree mirrors the real sources byte for byte rather than using
# hand-written miniatures. A miniature fixture makes a FAIL unattributable --
# it could come from the injection or from the fixture itself -- which is the
# same mistake that was caught the hard way on the input contract gate.
function New-MirrorTree([string]$sourceRoot, [string]$destRoot) {
    $relativePaths = New-Object System.Collections.Generic.List[string]
    foreach ($key in $script:SourceFiles.Keys) { [void]$relativePaths.Add($script:SourceFiles[$key]) }
    [void]$relativePaths.Add('refkeen/scripts/Preflight-KeenData.ps1')
    [void]$relativePaths.Add('refkeen/platform/retropicoes/path_mapping.h')
    [void]$relativePaths.Add('refkeen/tests/test_path_mapping.c')

    # The self-test never builds the host test (its numbers are supplied by the
    # release path), so only the header and the test need to be present for the
    # injections below to land.
    foreach ($relative in $relativePaths) {
        $source = Join-Path $sourceRoot $relative
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "self-test needs the real source to mirror: $relative"
        }
        $dest = Join-Path $destRoot $relative
        $destDir = Split-Path -Parent $dest
        if (-not (Test-Path -LiteralPath $destDir)) {
            [void](New-Item -ItemType Directory -Path $destDir -Force)
        }
        Copy-Item -LiteralPath $source -Destination $dest -Force
    }
    return $destRoot
}

function Edit-Relative([string]$root, [string]$relative, [string]$old, [string]$new) {
    $path = Join-Path $root $relative
    $body = Get-Content -LiteralPath $path -Raw
    if (-not $body.Contains($old)) {
        throw "self-test injection target not found in ${relative}: $old"
    }
    Set-Content -LiteralPath $path -Value ($body.Replace($old, $new))
}

if ($SelfTest) {
    if ([string]::IsNullOrEmpty($RunId)) { $RunId = [Guid]::NewGuid().ToString('N').Substring(0, 8) }

    # The self-test drives the checks that do not need a compiler, with the port
    # side supplied as a fixture derived from the real header. That keeps the
    # injected defects in *source* form, which is what the injections below edit.
    function Get-FixtureHostStdout([string]$root) {
        $header = Get-Content -LiteralPath (Join-Path $root 'refkeen/platform/retropicoes/path_mapping.h') -Raw
        $medium   = Get-DefineInt $header 'RETROPICOES_PATH_MEDIUM_BOUND'
        $dosBound = Get-DefineInt $header 'RETROPICOES_PATH_DOS_NAME_BOUND'
        $longMax  = Get-DefineInt $header 'RETROPICOES_PATH_LONG_NAME_MAX'
        $gameData = Get-DefineString $header 'RETROPICOES_PATH_GAME_DATA_BASE'
        $saveRoot = Get-DefineString $header 'RETROPICOES_PATH_SAVE_ROOT_BASE'
        if ($null -eq $medium) { $medium = -1 }
        if ($null -eq $dosBound) { $dosBound = -1 }
        if ($null -eq $longMax) { $longMax = -1 }
        if ($null -eq $gameData) { $gameData = '(missing)' }
        if ($null -eq $saveRoot) { $saveRoot = '(missing)' }

        $lines = New-Object System.Collections.Generic.List[string]
        [void]$lines.Add("CONTRACT medium_bound=$medium")
        [void]$lines.Add("CONTRACT dos_name_bound=$dosBound")
        [void]$lines.Add("CONTRACT dos_name_max=$($dosBound - 1)")
        [void]$lines.Add("CONTRACT long_name_max=$longMax")
        [void]$lines.Add("CONTRACT game_data_base=$gameData")
        [void]$lines.Add("CONTRACT save_root_base=$saveRoot")
        [void]$lines.Add("CONTRACT role=0 base=$gameData name_max=$($dosBound - 1) writable=0")
        [void]$lines.Add("CONTRACT role=1 base=$saveRoot name_max=$longMax writable=1")

        $test = Get-Content -LiteralPath (Join-Path $root 'refkeen/tests/test_path_mapping.c') -Raw
        # Extract from the array block, not line by line: the entries are packed
        # several per line, so a line-anchored pattern would silently capture
        # only the first one on each line and the fixture would look clean.
        $versionStart = $test.IndexOf('g_keen_version_dirs[]')
        if ($versionStart -ge 0) {
            $versionEnd = $test.IndexOf('};', $versionStart)
            if ($versionEnd -gt $versionStart) {
                $versionBlock = $test.Substring($versionStart, $versionEnd - $versionStart)
                foreach ($m in [regex]::Matches($versionBlock, '"(kdreams[a-z0-9]+)"')) {
                    [void]$lines.Add("CONTRACT version_dir=$($m.Groups[1].Value)")
                }
            }
        }
        $gameFilesStart = $test.IndexOf('g_keen_required_files[]')
        if ($gameFilesStart -ge 0) {
            $gameFilesEnd = $test.IndexOf('};', $gameFilesStart)
            if ($gameFilesEnd -gt $gameFilesStart) {
                $block = $test.Substring($gameFilesStart, $gameFilesEnd - $gameFilesStart)
                foreach ($m in [regex]::Matches($block, '"([A-Z0-9_]+\.[A-Z0-9_]+)"')) {
                    [void]$lines.Add("CONTRACT game_file=$($m.Groups[1].Value)")
                }
            }
        }
        return ($lines -join "`n")
    }

    $cases = New-Object System.Collections.Generic.List[object]
    [void]$cases.Add(@{
        Name = 'C0 clean mirror'; Check = 'none'; Expect = 'PASS'; Apply = $null
    })
    [void]$cases.Add(@{
        Name = 'C1 port medium bound moved'; Check = '1'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root 'refkeen/platform/retropicoes/path_mapping.h' `
                '#define RETROPICOES_PATH_MEDIUM_BOUND   256' `
                '#define RETROPICOES_PATH_MEDIUM_BOUND   255'
        }
    })
    [void]$cases.Add(@{
        Name = 'C2 game data base moved'; Check = '2'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root 'refkeen/platform/retropicoes/path_mapping.h' `
                '#define RETROPICOES_PATH_GAME_DATA_BASE "/data/refkeen"' `
                '#define RETROPICOES_PATH_GAME_DATA_BASE "/data/ref"'
        }
    })
    [void]$cases.Add(@{
        Name = 'C3 upstream version dir drift'; Check = '3'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root '_deps/reflectionhle-upstream/src/be_gamestr.h' `
                '"kdreamse193"' '"kdreams193"'
        }
    })
    [void]$cases.Add(@{
        Name = 'C4 required game file drift'; Check = '4'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root '_deps/reflectionhle-upstream/src/backend/gamedefs/be_gamedefs_kdreams.h' `
                '{"AUDIO.KDR", 3498, 0x80ac85e5}' '{"AUDIO.KD", 3498, 0x80ac85e5}'
        }
    })
    [void]$cases.Add(@{
        Name = 'C5 new search root appears'; Check = '5'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root '_deps/reflectionhle-upstream/src/backend/filesystem/be_filesystem_file_ops.c' `
                'BE_FILE_REQUEST_READ, g_be_modPath, NULL' `
                'BE_FILE_REQUEST_READ, g_be_unknownPath, NULL'
        }
    })
    [void]$cases.Add(@{
        Name = 'C6 mkdir target disappears'; Check = '6'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root '_deps/reflectionhle-upstream/src/backend/filesystem/be_filesystem_gameinst.c' `
                'BEL_Cross_mkdir(g_be_appDataPath); // Non-recursive' `
                '/* mkdir removed by the self-test */'
        }
    })
    [void]$cases.Add(@{
        Name = 'C7 port version dir dropped'; Check = '3'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root 'refkeen/tests/test_path_mapping.c' `
                '"kdreamse120"' '"kdreamse121"'
        }
    })

    $results = New-Object System.Collections.Generic.List[string]
    $selfFailed = $false
    $index = 0
    foreach ($case in $cases) {
        $index++
        # A fresh mirror per case, so no mutation can leak into the next one.
        $caseRoot = Join-Path ([IO.Path]::GetTempPath()) "refkeen-path-case-$RunId-$index"
        if (Test-Path -LiteralPath $caseRoot) { Remove-Item -LiteralPath $caseRoot -Recurse -Force }
        [void](New-MirrorTree -sourceRoot $Workspace -destRoot $caseRoot)

        $root = $caseRoot
        if ($null -ne $case.Apply) { & $case.Apply }

        $failures.Clear(); $notes.Clear(); $skipped.Clear()
        Invoke-Contract (Get-Sources -root $caseRoot -hostStdout (Get-FixtureHostStdout $caseRoot))
        $verdict = if ($failures.Count -eq 0) { 'PASS' } else { 'FAIL' }
        $ok = ($verdict -ceq $case.Expect)
        if (-not $ok) { $selfFailed = $true }
        [void]$results.Add(('  {0,-34} check={1,-4} expect={2} got={3} {4}' -f `
            $case.Name, $case.Check, $case.Expect, $verdict,
            $(if ($ok) { 'ok' } else { '<-- WRONG' })))
        # When a case does not behave, the failure text is the only way to tell
        # "check did not fire" from "an unrelated check fired".
        if (-not $ok -or $case.Check -ceq 'none') {
            if ($failures.Count -gt 0) {
                [void]$results.Add('      failures: ' + ($failures -join ' | '))
            } else {
                [void]$results.Add('      failures: none')
            }
        }
    }

    $summary = if ($selfFailed) {
        'REFKEEN PATH CONTRACT SELF-TEST: FAIL'
    } else {
        "REFKEEN PATH CONTRACT SELF-TEST: PASS ($($cases.Count)/$($cases.Count))"
    }
    [void]$results.Add($summary)
    Write-Output ($results -join "`n")
    if (-not [string]::IsNullOrEmpty($ReportPath)) {
        $results | Set-Content -LiteralPath $ReportPath
    }
    if ($selfFailed) { exit 1 }
    exit 0
}

# ---------------------------------------------------------------------------
# Release path
# ---------------------------------------------------------------------------

$hostTest = Invoke-HostTest $Workspace
Invoke-Contract (Get-Sources -root $Workspace -hostStdout $hostTest.Stdout)

if ($null -ne $hostTest.Error) {
    Add-Failure "check 7: $($hostTest.Error)"
} else {
    Add-Note 'check 7 ok  host test compiles under -Wall -Wextra -Werror and passes'
}

$hostTestVerdict = if ($null -eq $hostTest.Error) { 'built and passed' } else { $hostTest.Error }

$lines = New-Object System.Collections.Generic.List[string]
[void]$lines.Add('S20-C PATH MAPPING CONTRACT')
[void]$lines.Add("workspace: $Workspace")
[void]$lines.Add("host test: $hostTestVerdict")
[void]$lines.Add('')
foreach ($note in $notes)       { [void]$lines.Add("  $note") }
foreach ($skip in $skipped)     { [void]$lines.Add("  SKIP  $skip") }
foreach ($failure in $failures) { [void]$lines.Add("  FAIL  $failure") }
[void]$lines.Add('')

if ($failures.Count -eq 0) {
    $suffix = if ($skipped.Count -gt 0) { " -- $($skipped.Count) check(s) SKIPPED, see above" } else { '' }
    [void]$lines.Add("REFKEEN PATH CONTRACT: PASS ($($notes.Count) checks)$suffix")
    $text = $lines -join "`n"
    Write-Output $text
    if (-not [string]::IsNullOrEmpty($ReportPath)) { $text | Set-Content -LiteralPath $ReportPath }
    exit 0
} else {
    [void]$lines.Add("REFKEEN PATH CONTRACT: FAIL ($($failures.Count) problem(s))")
    $text = $lines -join "`n"
    Write-Output $text
    if (-not [string]::IsNullOrEmpty($ReportPath)) { $text | Set-Content -LiteralPath $ReportPath }
    exit 1
}
