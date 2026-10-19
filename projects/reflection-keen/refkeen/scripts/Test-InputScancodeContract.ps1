<#
.SYNOPSIS
    S20-C input scancode contract gate.

.DESCRIPTION
    Answers one question: can the port feed the engine the keys it expects?

    The answer is a chain, and every link sits in a different file owned by a
    different party. This gate walks the chain and refuses to pass unless all of
    it agrees:

      shim SDL_scancode.h          <- must equal a real SDL3 header
        |
      port channel table           <- compat channel -> SDL scancode
        |
      be_events_sdl_keymap.h       <- indexed BY SDL scancode (upstream)
        |
      be_st.h BE_ST_SC_*           <- DOS set-1 scancode (upstream)
        |
      id_in.c KbdDefs[0]           <- the engine's own default bindings

    The last hop is the one that matters: it proves the port's chosen scancodes
    land exactly on the bindings Keen Dreams ships with, so the game's control
    panel and the config round-trip stay the single source of truth.

    Four quadrants are exercised: known values, an independent implementation of
    the same facts (a real SDL3 header, and the SDL 2.0.2 enum upstream embeds),
    a negative path (-SelfTest, one injected defect per check), and a release
    path (the real tree).

.PARAMETER SelfTest
    Mirror the real sources into a scratch tree, confirm the clean copy passes,
    then inject one defect per check and require exactly that check to fire. A
    gate that cannot fail is worse than none. The host-test build (check 6) is
    not re-run here; it has no injected-defect path of its own.

.PARAMETER Sdl3Header
    Path to a real SDL3/SDL_scancode.h. Defaults to the S20-B baseline mirror.
    If absent, checks 1 and 5 are skipped *loudly* and named in the summary line
    rather than silently omitted.

.EXAMPLE
    & refkeen/scripts/Test-InputScancodeContract.ps1 `
        -Gcc E:\LVGL\...\gcc.exe -ReportPath .emv2\logs\s20-input-contract.txt

.EXAMPLE
    & refkeen/scripts/Test-InputScancodeContract.ps1 -SelfTest -RunId r4
#>
[CmdletBinding()]
param(
    [string]$Workspace = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$Gcc,
    [string]$Sdl3Header,
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

# "SDL_SCANCODE_RIGHT = 79," -> name -> value. Applies to the port shim, a real
# SDL3 header, and the SDL 2.0.2 enum upstream embeds: all three spell explicit
# values for the constants we care about.
function Get-ScancodeConstants([string]$text) {
    $map = @{}
    if ([string]::IsNullOrEmpty($text)) { return ,$map }
    foreach ($match in [regex]::Matches($text, 'SDL_SCANCODE_([A-Z0-9_]+)\s*=\s*([0-9]+)\b')) {
        $name = $match.Groups[1].Value
        $value = [int]$match.Groups[2].Value
        if ($map.ContainsKey($name)) {
            # A duplicated definition inside one file would make the source
            # ambiguous; report it rather than letting the last one win.
            Add-Failure "duplicate definition of SDL_SCANCODE_$name (values $($map[$name]) and $value)"
        } else {
            $map[$name] = $value
        }
    }
    return ,$map
}

# be_st.h's DOS set-1 enum mixes explicit ("X = 0x48,") and implicit (bare "X,")
# entries, so it has to be walked positionally. Comments carry stray numbers
# ("// 0Bh") and must be stripped first. Keys keep the BE_ST_SC_ prefix so they
# can be looked up straight from the keymap's own entries.
function Get-BeStScancodes([string]$text) {
    $map = @{}
    if ([string]::IsNullOrEmpty($text)) { return ,$map }
    $start = $text.IndexOf('typedef enum BE_ST_ScanCode_T')
    if ($start -lt 0) { return ,$map }
    $end = $text.IndexOf('} BE_ST_ScanCode_T;', $start)
    if ($end -lt 0) { return ,$map }
    $body = $text.Substring($start, $end - $start)

    $next = 0
    foreach ($rawLine in ($body -split "`r?`n")) {
        $line = $rawLine
        $comment = $line.IndexOf('//')
        if ($comment -ge 0) { $line = $line.Substring(0, $comment) }
        $line = $line.Trim()
        if ($line.Length -eq 0) { continue }
        if ($line -match '^typedef' -or $line -match 'BE_ST_ScanCode_T') { continue }
        foreach ($piece in ($line -split ',')) {
            $entry = $piece.Trim().Trim('{').Trim()
            if ($entry.Length -eq 0) { continue }
            if ($entry -match '^BE_ST_SC_([A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)$') {
                $literal = $matches[2]
                $value = if ($literal.StartsWith('0x') -or $literal.StartsWith('0X')) {
                    [Convert]::ToInt32($literal.Substring(2), 16)
                } else { [int]$literal }
                $map['BE_ST_SC_' + $matches[1]] = $value
                $next = $value + 1
            } elseif ($entry -match '^BE_ST_SC_([A-Z0-9_]+)$') {
                $map['BE_ST_SC_' + $matches[1]] = $next
                $next = $next + 1
            }
        }
    }
    return ,$map
}

# be_events_sdl_keymap.h: one initialiser per line, ordinal == SDL scancode.
# Returns ordinal -> @{ Special = bool; Name = 'BE_ST_SC_X' }. Only non-empty
# entries are recorded, so a lookup miss means "this scancode is unmapped".
function Get-KeymapOrdinals([string]$text) {
    $map = @{}
    if ([string]::IsNullOrEmpty($text)) { return ,$map }
    $ordinal = 0
    foreach ($rawLine in ($text -split "`r?`n")) {
        if ($rawLine -match '^\s*emptyDOSKeyEvent\s*,') { $ordinal++; continue }
        $m = [regex]::Match($rawLine, '^\s*\{(?<spec>true|false)\s*,\s*(?<name>BE_ST_SC_[A-Z0-9_]+)\s*\}')
        if ($m.Success) {
            $map[$ordinal] = @{
                Special = ($m.Groups['spec'].Value -ceq 'true')
                Name = $m.Groups['name'].Value
            }
            $ordinal++
        }
    }
    return ,$map
}

# The port's own table: "[COMPAT_KEY_UP] = SDL_SCANCODE_UP," -> channel -> SDL name.
function Get-PortChannelTable([string]$text) {
    $map = @{}
    if ([string]::IsNullOrEmpty($text)) { return ,$map }
    foreach ($match in [regex]::Matches($text, '\[COMPAT_KEY_([A-Z0-9_]+)\]\s*=\s*SDL_SCANCODE_([A-Z0-9_]+)')) {
        $map[$match.Groups[1].Value] = $match.Groups[2].Value
    }
    return ,$map
}

# The declared contract in the host test: "{COMPAT_KEY_UP, 82u, 0x48u},"
function Get-DeclaredContract([string]$text) {
    $map = @{}
    if ([string]::IsNullOrEmpty($text)) { return ,$map }
    foreach ($match in [regex]::Matches($text, '\{COMPAT_KEY_([A-Z0-9_]+),\s*([0-9]+)u,\s*(0x[0-9A-Fa-f]+)u\}')) {
        $map[$match.Groups[1].Value] = @{
            Sdl = [int]$match.Groups[2].Value
            Dos = [Convert]::ToInt32($match.Groups[3].Value.Substring(2), 16)
        }
    }
    return ,$map
}

# id_in.c's default KeyboardDef. Field order is fixed by the struct in id_in.h:
# button0, button1, upleft, up, upright, left, right, downleft, down, downright.
$script:DefaultFieldNames = @(
    'button0', 'button1', 'upleft', 'up', 'upright',
    'left', 'right', 'downleft', 'down', 'downright'
)

function Get-DefaultBindings([string]$text) {
    $map = @{}
    if ([string]::IsNullOrEmpty($text)) { return ,$map }
    $match = [regex]::Match($text, 'KbdDefs\[MaxKbds\]\s*=\s*\{\{([^}]*)\}\}')
    if (-not $match.Success) { return ,$map }
    $values = @([regex]::Matches($match.Groups[1].Value, '0x[0-9A-Fa-f]+') |
                ForEach-Object { [Convert]::ToInt32($_.Value.Substring(2), 16) })
    for ($i = 0; $i -lt $values.Count -and $i -lt $script:DefaultFieldNames.Count; $i++) {
        $map[$script:DefaultFieldNames[$i]] = $values[$i]
    }
    return ,$map
}

# ---------------------------------------------------------------------------
# The chain
# ---------------------------------------------------------------------------

# Which engine field each compat channel must land on. This pairing is the
# design statement being tested; the values on both sides are read from source.
$script:ChannelField = @{
    LEFT   = 'left'
    RIGHT  = 'right'
    UP     = 'up'
    DOWN   = 'down'
    JUMP   = 'button1'   # "Throwing", Alt
    ACTION = 'button0'   # "Jumping", Ctrl
    START  = 'enter'     # not a KeyboardDef field: the status/confirm key
    BACK   = 'esc'       # not a KeyboardDef field: the game's control panel
}

# SDL's mobile/Android keys moved between 2.0 and 3.x. Upstream straddles the
# change: key_scancode_converters.c embeds the SDL 2.0.2 enum, but
# be_events_sdl_keymap.h is laid out with SDL3 numbering (verified by check 5).
# Only these are allowed to disagree, and only with exactly the values below --
# so if either side ever moves again, the gate turns red instead of shrugging.
$script:MobileRenumber = @{
    AC_BACK = @{ Sdl2 = 270; Sdl3 = 282 }
}

function Invoke-Contract([hashtable]$sources) {
    $shim = Get-ScancodeConstants $sources.Shim
    if ($shim.Count -eq 0) { Add-Failure 'shim: no SDL_SCANCODE_* constants parsed'; return }

    # --- Check 1: shim values against a real SDL3 header -------------------
    if ($null -eq $sources.Sdl3) {
        Add-Skipped 'check 1 (shim vs real SDL3 header): baseline mirror not found'
    } else {
        $real = Get-ScancodeConstants $sources.Sdl3
        if ($real.Count -eq 0) {
            Add-Failure 'check 1: real SDL3 header parsed zero constants'
        } else {
            $mismatch = New-Object System.Collections.Generic.List[string]
            foreach ($name in ($shim.Keys | Sort-Object)) {
                if (-not $real.ContainsKey($name)) {
                    [void]$mismatch.Add("$name absent from the real header")
                } elseif ($real[$name] -ne $shim[$name]) {
                    [void]$mismatch.Add("$name shim=$($shim[$name]) real=$($real[$name])")
                }
            }
            if ($mismatch.Count -gt 0) {
                Add-Failure ("check 1: shim disagrees with real SDL3 on " +
                             "$($mismatch.Count) constant(s): " + ($mismatch -join '; '))
            } else {
                Add-Note "check 1 ok  $($shim.Count) constants match a real SDL3 header"
            }
        }
    }

    # --- Check 2: shim against the SDL 2.0.2 enum upstream embeds ----------
    $embedded = Get-ScancodeConstants $sources.Embedded
    if ($embedded.Count -eq 0) {
        Add-Failure 'check 2: could not parse the SDL 2.0.2 enum in key_scancode_converters.c'
    } else {
        $shared  = 0
        $bad     = New-Object System.Collections.Generic.List[string]
        $outside = New-Object System.Collections.Generic.List[string]
        $renumbered = New-Object System.Collections.Generic.List[string]
        foreach ($name in ($shim.Keys | Sort-Object)) {
            if ($embedded.ContainsKey($name)) {
                $shared++
                if ($embedded[$name] -ne $shim[$name]) {
                    if ($script:MobileRenumber.ContainsKey($name) -and
                        $script:MobileRenumber[$name].Sdl2 -eq $embedded[$name] -and
                        $script:MobileRenumber[$name].Sdl3 -eq $shim[$name]) {
                        [void]$renumbered.Add("$name 2015=$($embedded[$name]) SDL3=$($shim[$name])")
                    } else {
                        [void]$bad.Add("$name shim=$($shim[$name]) 2015=$($embedded[$name])")
                    }
                }
            } else {
                [void]$outside.Add($name)
            }
        }
        if ($bad.Count -gt 0) {
            Add-Failure ("check 2: shim disagrees with upstream's SDL 2.0.2 enum on " +
                         "$($bad.Count) constant(s): " + ($bad -join '; '))
        } elseif ($shared -lt 20) {
            Add-Failure "check 2: only $shared shared constants -- the comparison is not meaningful"
        } else {
            Add-Note "check 2 ok  $shared shared constants agree with upstream's SDL 2.0.2 enum"
        }
        foreach ($entry in $renumbered) {
            Add-Note ("check 2 note: mobile key renumbered between SDL 2.0.2 and SDL3, port follows " +
                      "the keymap: $entry")
        }
        if ($outside.Count -gt 0) {
            Add-Note ("check 2 note: $($outside.Count) shim constant(s) are outside the 2015 enum " +
                      "(referenced positionally by its table): " + ($outside -join ', '))
        }
    }

    # --- Check 3: port table agrees with the declared contract --------------
    $check3Before = $failures.Count
    $port     = Get-PortChannelTable $sources.Port
    $declared = Get-DeclaredContract $sources.Test
    if ($port.Count -eq 0) { Add-Failure 'check 3: port channel table parsed zero entries'; return }
    if ($declared.Count -eq 0) { Add-Failure 'check 3: host test declared zero contracts'; return }

    foreach ($channel in ($port.Keys | Sort-Object)) {
        if (-not $declared.ContainsKey($channel)) {
            Add-Failure "check 3: channel $channel is in the port table but has no declared contract"
        }
    }
    foreach ($channel in ($declared.Keys | Sort-Object)) {
        if (-not $port.ContainsKey($channel)) {
            Add-Failure "check 3: channel $channel is declared in the test but missing from the port table"
            continue
        }
        $name = $port[$channel]
        if (-not $shim.ContainsKey($name)) {
            Add-Failure "check 3: channel $channel maps to SDL_SCANCODE_$name, which the shim does not define"
            continue
        }
        if ($shim[$name] -ne $declared[$channel].Sdl) {
            Add-Failure ("check 3: channel $channel is SDL_SCANCODE_$name=$($shim[$name]) " +
                         "but the test declares $($declared[$channel].Sdl)")
        }
    }
    if ($failures.Count -eq $check3Before) {
        Add-Note "check 3 ok  all $($port.Count) channels agree between the port table and the declared contract"
    }

    # --- Check 4: the whole chain, down to the engine's default bindings ----
    $keymap   = Get-KeymapOrdinals $sources.Keymap
    $beSt     = Get-BeStScancodes $sources.BeSt
    $defaults = Get-DefaultBindings $sources.IdIn
    if ($keymap.Count -eq 0) { Add-Failure 'check 4: could not parse be_events_sdl_keymap.h'; return }
    if ($beSt.Count -eq 0)   { Add-Failure 'check 4: could not parse the BE_ST_SC_* enum in be_st.h'; return }
    if ($defaults.Count -lt 10) {
        Add-Failure "check 4: parsed only $($defaults.Count) of 10 default bindings from id_in.c"
        return
    }

    $check4Before = $failures.Count
    foreach ($channel in ($port.Keys | Sort-Object)) {
        $sdlName = $port[$channel]
        if (-not $shim.ContainsKey($sdlName)) { continue }
        $sdl = $shim[$sdlName]

        if (-not $keymap.ContainsKey($sdl)) {
            Add-Failure ("check 4: channel $channel uses SDL_SCANCODE_$sdlName=$sdl, " +
                         "which upstream's keymap does not map -- the key would be dead")
            continue
        }
        $entry = $keymap[$sdl]
        if (-not $beSt.ContainsKey($entry.Name)) {
            Add-Failure "check 4: keymap names $($entry.Name), which be_st.h does not define"
            continue
        }
        $dos = $beSt[$entry.Name]

        # Arrows and the navigation block reach the engine behind the 0xE0
        # prefix; the two game keys do not. Asserted explicitly so a future
        # table edit cannot quietly change the prefix state.
        $isNavigation = @('BE_ST_SC_UP', 'BE_ST_SC_DOWN', 'BE_ST_SC_LEFT', 'BE_ST_SC_RIGHT') -ccontains $entry.Name
        if ($isNavigation) {
            if (-not $entry.Special) {
                Add-Failure "check 4: channel $channel -> $($entry.Name) should be 0xE0-prefixed but is not"
            }
        }
        if (@('BE_ST_SC_RCTRL', 'BE_ST_SC_RALT') -ccontains $entry.Name -and $entry.Special) {
            Add-Failure "check 4: channel $channel -> $($entry.Name) must not be 0xE0-prefixed"
        }

        $field = $script:ChannelField[$channel]
        if ($null -eq $field) {
            Add-Failure "check 4: channel $channel has no engine role declared in ChannelField"
            continue
        }
        if ($field -ceq 'enter' -or $field -ceq 'esc') {
            $expectedName = if ($field -ceq 'enter') { 'BE_ST_SC_ENTER' } else { 'BE_ST_SC_ESC' }
            if ($entry.Name -cne $expectedName) {
                Add-Failure "check 4: channel $channel should reach $expectedName but the keymap gives $($entry.Name)"
            } elseif ($declared[$channel].Dos -ne $dos) {
                Add-Failure ("check 4: channel $channel declares DOS 0x$('{0:x2}' -f $declared[$channel].Dos) " +
                             "but the chain gives 0x$('{0:x2}' -f $dos)")
            }
            continue
        }
        if (-not $defaults.ContainsKey($field)) {
            Add-Failure "check 4: id_in.c has no default binding named $field"
            continue
        }
        if ($defaults[$field] -ne $dos) {
            Add-Failure ("check 4: channel $channel should land on KbdDefs[0].$field = " +
                         "0x$('{0:x2}' -f $defaults[$field]) but the chain gives 0x$('{0:x2}' -f $dos)")
            continue
        }
        if ($declared[$channel].Dos -ne $dos) {
            Add-Failure ("check 4: channel $channel declares DOS 0x$('{0:x2}' -f $declared[$channel].Dos) " +
                         "but the chain gives 0x$('{0:x2}' -f $dos)")
        }
    }
    if ($failures.Count -eq $check4Before) {
        Add-Note "check 4 ok  all $($port.Count) channels reach their engine default through the chain"
    }

    # --- Check 5: the keymap is still indexed by SDL scancode --------------
    # Every annotated entry's ordinal must equal the scancode named in its
    # comment. This is the structural fact the whole design rests on: the port
    # chooses a scancode and upstream looks it up positionally, so if the two
    # ever disagree the port would silently drive the wrong key.
    #
    # Compared against a real SDL3 header when one is available; otherwise it
    # falls back to the shim, which is weaker (shim-vs-keymap only) and says so.
    $alignment = if ($null -ne $sources.Sdl3) { Get-ScancodeConstants $sources.Sdl3 } else { $shim }
    $alignedBy = if ($null -ne $sources.Sdl3) { 'real SDL3' } else { 'shim, mirror absent' }
    $annotated = 0
    $drift = New-Object System.Collections.Generic.List[string]
    $ordinal = 0
    foreach ($rawLine in ($sources.Keymap -split "`r?`n")) {
        if ($rawLine -match '^\s*emptyDOSKeyEvent\s*,') { $ordinal++; continue }
        $m = [regex]::Match($rawLine, '^\s*\{(?:true|false)\s*,\s*BE_ST_SC_[A-Z0-9_]+\}\s*,\s*//\s*SDL_SCANCODE_([A-Z0-9_]+)')
        if ($m.Success) {
            $annotated++
            $commentName = $m.Groups[1].Value
            if ($alignment.ContainsKey($commentName)) {
                if ($alignment[$commentName] -ne $ordinal) {
                    [void]$drift.Add("$commentName ordinal=$ordinal $alignedBy=$($alignment[$commentName])")
                }
            } else {
                [void]$drift.Add("$commentName has no value in $alignedBy")
            }
            $ordinal++
            continue
        }
        if ($rawLine -match '^\s*\{(?:true|false)\s*,') { $ordinal++ }
    }
    if ($annotated -lt 50) {
        Add-Failure "check 5: only $annotated annotated keymap entries found -- parse is suspect"
    } elseif ($drift.Count -gt 0) {
        Add-Failure ("check 5: keymap is no longer indexed by SDL scancode for " +
                     "$($drift.Count) entry(ies): " + ($drift -join '; '))
    } else {
        Add-Note "check 5 ok  $annotated annotated keymap entries are scancode-aligned (vs $alignedBy)"
    }
}

# ---------------------------------------------------------------------------
# Source loading
# ---------------------------------------------------------------------------

$script:SourceFiles = @{
    Shim     = 'refkeen/host/shim/SDL3/SDL_scancode.h'
    Port     = 'refkeen/platform/retropicoes/input_mapping.c'
    Test     = 'refkeen/tests/test_input_mapping.c'
    Keymap   = '_deps/reflectionhle-upstream/src/backend/events/be_events_sdl_keymap.h'
    BeSt     = '_deps/reflectionhle-upstream/src/be_st.h'
    IdIn     = '_deps/reflectionhle-upstream/src/kdreams/id_in.c'
    Embedded = '_deps/reflectionhle-upstream/src/kdreams/key_scancode_converters.c'
}

function Get-Sources([string]$root, [string]$sdl3Path) {
    $text = @{}
    foreach ($key in $script:SourceFiles.Keys) {
        $text[$key] = Get-Text (Join-Path $root $script:SourceFiles[$key])
    }
    $text.Sdl3 = if ($null -ne $sdl3Path) { Get-Text $sdl3Path } else { $null }
    return $text
}

function Resolve-Sdl3Header([string]$explicit) {
    if (-not [string]::IsNullOrEmpty($explicit)) {
        if (Test-Path -LiteralPath $explicit -PathType Leaf) { return $explicit }
        return $null
    }
    $candidates = @(
        'E:\WorkSpace\WorkBuddy_Space\refkeen-baseline\SDL3-3.4.16\x86_64-w64-mingw32\include\SDL3\SDL_scancode.h'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    return $null
}

# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

# The scratch tree mirrors the real sources byte for byte rather than using
# hand-written miniatures. That matters: an earlier miniature version failed the
# clean case outright (its enum was too small to clear check 2's parse-sanity
# floor and its keymap placed Escape at the wrong ordinal), which made the
# mutated cases unattributable -- a FAIL could have come from the injection or
# from the fixture. Mirroring removes that ambiguity, so each case below flips
# exactly one check.
function New-MirrorTree([string]$sourceRoot, [string]$destRoot) {
    foreach ($key in $script:SourceFiles.Keys) {
        $relative = $script:SourceFiles[$key]
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
    $sdl3 = Resolve-Sdl3Header $Sdl3Header

    $cases = New-Object System.Collections.Generic.List[object]
    [void]$cases.Add(@{
        Name = 'C0 clean mirror'; Check = 'none'; Expect = 'PASS'; Apply = $null
    })
    [void]$cases.Add(@{
        Name = 'C1 shim value wrong'; Check = '1'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root 'refkeen/host/shim/SDL3/SDL_scancode.h' `
                'SDL_SCANCODE_UP = 82,' 'SDL_SCANCODE_UP = 83,'
        }
    })
    [void]$cases.Add(@{
        Name = 'C2 embedded 2.0.2 enum drift'; Check = '2'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root '_deps/reflectionhle-upstream/src/kdreams/key_scancode_converters.c' `
                'SDL_SCANCODE_UP = 82,' 'SDL_SCANCODE_UP = 84,'
        }
    })
    [void]$cases.Add(@{
        Name = 'C3 port table points elsewhere'; Check = '3'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root 'refkeen/platform/retropicoes/input_mapping.c' `
                '[COMPAT_KEY_UP] = SDL_SCANCODE_UP,' '[COMPAT_KEY_UP] = SDL_SCANCODE_DOWN,'
        }
    })
    [void]$cases.Add(@{
        Name = 'C4 channel dropped from table'; Check = '3'; Expect = 'FAIL'; Apply = {
            # Replace the entry with a same-shape non-entry: line-ending agnostic,
            # and it removes the channel from the table without touching layout.
            Edit-Relative $root 'refkeen/platform/retropicoes/input_mapping.c' `
                '    [COMPAT_KEY_UP] = SDL_SCANCODE_UP,' '    /* removed */'
        }
    })
    [void]$cases.Add(@{
        Name = 'C5 declared DOS contract wrong'; Check = '4'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root 'refkeen/tests/test_input_mapping.c' `
                '{COMPAT_KEY_UP, 82u, 0x48u}' '{COMPAT_KEY_UP, 82u, 0x49u}'
        }
    })
    [void]$cases.Add(@{
        Name = 'C6 keymap loses alignment'; Check = '5'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root '_deps/reflectionhle-upstream/src/backend/events/be_events_sdl_keymap.h' `
                '    {true,  BE_ST_SC_UP}, // SDL_SCANCODE_UP' '    /* entry removed */'
        }
    })
    [void]$cases.Add(@{
        Name = 'C7 engine default changed'; Check = '4'; Expect = 'FAIL'; Apply = {
            Edit-Relative $root '_deps/reflectionhle-upstream/src/kdreams/id_in.c' `
                '0x1d,0x38,0x47' '0x1d,0x39,0x47'
        }
    })

    $results = New-Object System.Collections.Generic.List[string]
    $selfFailed = $false
    $index = 0
    foreach ($case in $cases) {
        $index++
        # A fresh mirror per case, so no mutation can leak into the next one.
        $caseRoot = Join-Path ([IO.Path]::GetTempPath()) "refkeen-input-case-$RunId-$index"
        if (Test-Path -LiteralPath $caseRoot) { Remove-Item -LiteralPath $caseRoot -Recurse -Force }
        [void](New-MirrorTree -sourceRoot $Workspace -destRoot $caseRoot)

        $root = $caseRoot
        if ($null -ne $case.Apply) { & $case.Apply }

        $failures.Clear(); $notes.Clear(); $skipped.Clear()
        Invoke-Contract (Get-Sources -root $caseRoot -sdl3Path $sdl3)
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
        'REFKEEN INPUT CONTRACT SELF-TEST: FAIL'
    } else {
        "REFKEEN INPUT CONTRACT SELF-TEST: PASS ($($cases.Count)/$($cases.Count))"
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

$sdl3 = Resolve-Sdl3Header $Sdl3Header
Invoke-Contract (Get-Sources -root $Workspace -sdl3Path $sdl3)

# Build and run the host test: the gate must not be able to pass while the thing
# it describes fails to build or fails its own assertions.
$hostTestVerdict = 'not built'
if (-not [string]::IsNullOrEmpty($Gcc) -and (Test-Path -LiteralPath $Gcc -PathType Leaf)) {
    $outDir = Join-Path $Workspace '_deps/_probe'
    if (-not (Test-Path -LiteralPath $outDir)) { [void](New-Item -ItemType Directory -Path $outDir -Force) }
    $exe = Join-Path $outDir 's20c-input-test-gate.exe'
    $gccArgs = @(
        '-std=c99', '-O1', '-Wall', '-Wextra', '-Werror',
        '-I', (Join-Path $Workspace 'compat'),
        '-I', (Join-Path $Workspace 'refkeen/host/shim'),
        '-I', (Join-Path $Workspace 'refkeen/platform/retropicoes'),
        '-o', $exe,
        (Join-Path $Workspace 'refkeen/tests/test_input_mapping.c'),
        (Join-Path $Workspace 'refkeen/platform/retropicoes/input_mapping.c')
    )
    $buildLog = & $Gcc @gccArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Add-Failure "check 6: the host test did not compile: $($buildLog -join ' ')"
    } else {
        $runLog = & $exe 2>&1
        if ($LASTEXITCODE -ne 0) {
            Add-Failure "check 6: the host test exited $LASTEXITCODE"
        } elseif (-not (($runLog -join "`n").Contains('REFKEEN INPUT MAPPING: PASS'))) {
            Add-Failure 'check 6: the host test did not print its PASS marker'
        } else {
            $hostTestVerdict = 'built and passed'
            Add-Note 'check 6 ok  host test compiles under -Wall -Wextra -Werror and passes'
        }
    }
} else {
    Add-Skipped 'check 6 (host test build+run): -Gcc not supplied or not found'
}

$lines = New-Object System.Collections.Generic.List[string]
[void]$lines.Add('S20-C INPUT SCANCODE CONTRACT')
[void]$lines.Add("workspace: $Workspace")
[void]$lines.Add("sdl3 header: $(if ($null -ne $sdl3) { $sdl3 } else { '<not found>' })")
[void]$lines.Add("host test: $hostTestVerdict")
[void]$lines.Add('')
foreach ($note in $notes)       { [void]$lines.Add("  $note") }
foreach ($skip in $skipped)     { [void]$lines.Add("  SKIP  $skip") }
foreach ($failure in $failures) { [void]$lines.Add("  FAIL  $failure") }
[void]$lines.Add('')

if ($failures.Count -eq 0) {
    $suffix = if ($skipped.Count -gt 0) { " -- $($skipped.Count) check(s) SKIPPED, see above" } else { '' }
    [void]$lines.Add("REFKEEN INPUT CONTRACT: PASS ($($notes.Count) checks)$suffix")
    $text = $lines -join "`n"
    Write-Output $text
    if (-not [string]::IsNullOrEmpty($ReportPath)) { $text | Set-Content -LiteralPath $ReportPath }
    exit 0
} else {
    [void]$lines.Add("REFKEEN INPUT CONTRACT: FAIL ($($failures.Count) problem(s))")
    $text = $lines -join "`n"
    Write-Output $text
    if (-not [string]::IsNullOrEmpty($ReportPath)) { $text | Set-Content -LiteralPath $ReportPath }
    exit 1
}
