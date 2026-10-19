<#
.SYNOPSIS
  Probe: does the "`-fdata-sections` + linker script" plan in
  platform-boundary.md section 11.2 actually hold -- verified on the REAL
  target toolchain?

.DESCRIPTION
  Section 11.2 claims the port can move upstream's big static buffers to PSRAM
  WITHOUT touching upstream source, by compiling with -fdata-sections and
  placing the resulting sections in a linker script. The linker script names
  sections by literal string, so the string matters.

  That is why this probe uses arm-none-eabi-gcc and not the host MinGW gcc: the
  generated section name is OBJECT FORMAT specific.

      MinGW  (PE/COFF)  ->  .data$g_be_emulatedMemSpace
      arm-none-eabi (ELF) ->  .bss.g_be_emulatedMemSpace  (or .data.g_...)

  A host-only check would happily validate the PE spelling, which is the wrong
  string for the Pico build. This probe reports which spelling the target
  toolchain actually emits, and then links with a script naming that spelling.

  Checks:
    A1  synthetic buffer, target toolchain: which section spelling appears?
    A2  the real upstream TU (src/be_cross_mem.c) with -fdata-sections
    B   link with a script that names the section -> symbol must move into the
        fake PSRAM region; and with the placement rule REMOVED -> the symbol
        must fall back to ordinary .bss. A mechanism that only ever says "yes"
        is not evidence.

  Read-only with respect to upstream.
#>
[CmdletBinding()]
param(
    [string]$Workspace,
    [string]$TargetGcc,
    [string]$ReportPath
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

if (-not $Workspace) {
    $Workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
}
if (-not $TargetGcc) {
    $TargetGcc = 'F:\Download\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe'
}
if (-not $ReportPath) {
    $ReportPath = Join-Path $Workspace '_deps\_probe\datasections-target.txt'
}

$tbin = Split-Path -Parent $TargetGcc
$nm = Join-Path $tbin 'arm-none-eabi-nm.exe'
$objdump = Join-Path $tbin 'arm-none-eabi-objdump.exe'
$upstream = Join-Path $Workspace '_deps\reflectionhle-upstream'
$work = Join-Path $Workspace ('_deps\_probe\datasec-t-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))

New-Item -ItemType Directory -Force -Path $work | Out-Null

$script:Lines = New-Object 'System.Collections.Generic.List[string]'
$script:Bad = 0
function Say { param([string]$T) $script:Lines.Add($T) }
function Fail { param([string]$T) $script:Bad++; Say ("  FAIL  " + $T) }
function Ok { param([string]$T) Say ("  ok    " + $T) }
function Note { param([string]$T) Say ("  note  " + $T) }

function Invoke-Native {
    param([string]$Exe, [string[]]$Arguments)
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = (& $Exe @Arguments 2>&1 | Out-String -Width 4096)
        $rc = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    return [pscustomobject]@{ Rc = $rc; Out = $out }
}

# Lists the SECTIONS (not symbols) whose name ends in the given array name.
#
# This reads the section table with objdump -h instead of hunting a section
# symbol with nm, and the distinction cost a debugging round: `-fdata-sections`
# really did create the sections, but a section symbol is STT_SECTION, which nm
# HIDES BY DEFAULT. nm showed nothing and the probe wrongly concluded the
# mechanism was absent. Reading the section table is the direct evidence; it
# does not depend on symbol visibility, nor on the separator character, which is
# object-format specific (PE uses `$`, ELF uses `.`).
function Get-SectionSymbol {
    param([string]$ObjPath, [string]$ArrayName)
    $r = Invoke-Native -Exe $objdump -Arguments @('-h', $ObjPath)
    $hits = @()
    foreach ($l in ($r.Out -split "`r?`n")) {
        # objdump -h line: "  3 .bss.g_x   00001000 00000000 00000000 00000000 2**2"
        if ($l -match '^\s*\d+\s+(\S+)\s+[0-9A-Fa-f]{8}\s') {
            $name = $Matches[1]
            if ($name -match ('[.\$]' + [regex]::Escape($ArrayName) + '$') -and $name -ne $ArrayName) {
                $hits += $name
            }
        }
    }
    # `,` keeps a single-element result from unwrapping into a bare string, which
    # would make $s[0] index a CHARACTER instead of the array's first entry.
    return ,$hits
}

Say "S20-B / 11.2 -- '-fdata-sections + linker script' probe on the TARGET toolchain"
Say ("Target : {0}" -f $TargetGcc)
Say ("Work   : {0}" -f $work)
Say ""

if (-not (Test-Path -LiteralPath $TargetGcc)) {
    Fail "target compiler not found: $TargetGcc"
    $script:Lines | Set-Content -LiteralPath $ReportPath -Encoding UTF8
    exit 2
}

$arrayName = 'g_be_emulatedMemSpace'
$sectOn = $null   # the exact spelling to hand to the linker script

# --- A1: synthetic, to learn the target's section spelling ------------------

Say "A1 synthetic buffer -> which section spelling does the target emit?"
$synC = Join-Path $work 'syn.c'
@'
/* Mirrors the shape of upstream's buffer: file scope, writable. */
unsigned char g_be_emulatedMemSpace[4096];
unsigned char g_other[64];
'@ | Set-Content -LiteralPath $synC -Encoding ASCII

$synObj = Join-Path $work 'syn.o'
$r = Invoke-Native -Exe $TargetGcc -Arguments @('-c', '-std=c99', '-fno-common', '-fdata-sections',
    '-O0', '-o', $synObj, $synC)
if ($r.Rc -ne 0) {
    Fail "synthetic compile failed (rc=$($r.Rc))"
    foreach ($l in (($r.Out -split "`r?`n") | Where-Object { $_ -match 'error' } | Select-Object -First 5)) {
        Say ("        " + $l.Trim())
    }
} else {
    $s = Get-SectionSymbol -ObjPath $synObj -ArrayName $arrayName
    if ($s.Count -eq 0) {
        Fail "-fdata-sections did NOT isolate the array on the target toolchain"
    } else {
        $sectOn = $s[0]
        Ok ("array got its own section: {0}" -f $sectOn)
        $sep = if ($sectOn -match '\.bss\.') { '.' } elseif ($sectOn -match '\.bss\$') { '$' } else { '?' }
        Note ("separator character is '{0}' -- linker scripts must use this literal spelling" -f $sep)
        $s2 = Get-SectionSymbol -ObjPath $synObj -ArrayName 'g_other'
        if ($s2.Count -gt 0) { Ok ("a second array is also isolated ({0})" -f $s2[0]) }
        else { Fail "g_other was not isolated: -fdata-sections is not applied per symbol" }
    }
}
Say ""

# --- A2: the real upstream TU ----------------------------------------------

Say "A2 upstream src/be_cross_mem.c with -fdata-sections"
$srcMem = Join-Path $upstream 'src\be_cross_mem.c'
if (-not (Test-Path -LiteralPath $srcMem)) {
    Fail "upstream file not found: $srcMem"
} else {
    $obj = Join-Path $work 'be_cross_mem.o'
    $incs = @(
        '-I', (Join-Path $Workspace 'refkeen\host\shim'),
        '-I', (Join-Path $Workspace 'refkeen\host\include'),
        '-I', (Join-Path $upstream 'src'),
        '-I', (Join-Path $upstream 'src\crc32'),
        '-I', (Join-Path $upstream 'src\depklite'),
        '-I', (Join-Path $upstream 'src\unlzexe'),
        '-I', (Join-Path $upstream 'src\kdreams'),
        '-I', (Join-Path $upstream 'src\kdreams\lscr')
    )
    $defs = @('-DREFKEEN_VER_KDREAMS=1', '-DREFKEEN_HAS_VER_KDREAMS=1', '-DREFKEEN_PLATFORM_PICO=1',
        '-DREFKEEN_ARCH_LITTLE_ENDIAN=1')
    $r = Invoke-Native -Exe $TargetGcc -Arguments (@($incs) + @($defs) + @(
            '-c', '-std=c99', '-fno-common', '-fdata-sections', '-O0',
            '-fmessage-length=0', '-o', $obj, $srcMem))
    if ($r.Rc -ne 0) {
        Note "the real TU does not compile standalone on the target toolchain yet"
        Note "(expected at S20-B: the port config/shim set is not complete for arm-none-eabi)"
        foreach ($l in (($r.Out -split "`r?`n") | Where-Object { $_ -match ' error' } | Select-Object -First 4)) {
            Say ("        " + $l.Trim())
        }
        # fall back to the synthetic object for part B; the mechanism is what is
        # under test here, not this TU's portability.
        $obj = $synObj
    } else {
        Ok "upstream TU compiles with -fdata-sections on the target toolchain"
        $s = Get-SectionSymbol -ObjPath $obj -ArrayName $arrayName
        if ($s.Count -eq 0) {
            Fail "upstream TU compiled but the array did not get its own section"
        } else {
            Ok ("upstream array section: {0}" -f $s[0])
            if ($sectOn -and $s[0] -ne $sectOn) {
                Note ("spelling differs from the synthetic case ({0} vs {1})" -f $s[0], $sectOn)
            }
        }
    }
}
Say ""

# --- B: does a linker script naming that section really relocate it? --------

if (-not $sectOn) {
    Say "B  skipped: no section spelling was established in A1"
    $script:Bad++
} else {
    Say "B  linker script placement -- on and off"
    $mainC = Join-Path $work 'main.c'
    @'
/* No libc: keeps the link to our own script self-contained. */
unsigned char g_be_emulatedMemSpace[4096];
unsigned char g_other[64];
void _start(void) {
    g_be_emulatedMemSpace[0] = 1;
    g_other[0] = 2;
    for (;;) { }
}
'@ | Set-Content -LiteralPath $mainC -Encoding ASCII

    $sectEsc = $sectOn
    $onLd = Join-Path $work 'place_on.ld'
    # The placement rule for the named section MUST come before the catch-all
    # `*(.bss*)` rule: ld assigns each input section to the first output section
    # that matches it. Put the catch-all first and the buffer silently lands in
    # SRAM -- which is exactly the failure mode section 11.2 warns about.
    @"
MEMORY
{
  PSRAM (rwx) : ORIGIN = 0x11000000, LENGTH = 2M
  SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}
SECTIONS
{
  .text : { *(.text*) *(.rodata*) } > SRAM
  .psram_bss (NOLOAD) : { *($sectEsc) } > PSRAM
  .sram_bss  (NOLOAD) : { *(.bss*) *(COMMON) } > SRAM
}
"@ | Set-Content -LiteralPath $onLd -Encoding ASCII

    $offLd = Join-Path $work 'place_off.ld'
    @"
MEMORY
{
  PSRAM (rwx) : ORIGIN = 0x11000000, LENGTH = 2M
  SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}
SECTIONS
{
  .text : { *(.text*) *(.rodata*) } > SRAM
  .sram_bss  (NOLOAD) : { *(.bss*) *(COMMON) } > SRAM
}
"@ | Set-Content -LiteralPath $offLd -Encoding ASCII

    $obj = Join-Path $work 'main.o'
    $r = Invoke-Native -Exe $TargetGcc -Arguments @('-c', '-std=c99', '-fno-common', '-fdata-sections',
        '-ffunction-sections', '-O0', '-o', $obj, $mainC)
    if ($r.Rc -ne 0) {
        Fail "could not compile main.c for the target"
        foreach ($l in (($r.Out -split "`r?`n") | Where-Object { $_ -match 'error' } | Select-Object -First 5)) {
            Say ("        " + $l.Trim())
        }
    } else {
        foreach ($case in @(
                @{ Tag = 'on';  Ld = $onLd;  ExpectPsram = $true },
                @{ Tag = 'off'; Ld = $offLd; ExpectPsram = $false }
            )) {
            $elf = Join-Path $work ("probe-$($case.Tag).elf")
            $r = Invoke-Native -Exe $TargetGcc -Arguments @(
                '-nostdlib', '-Wl,--entry=_start', ("-Wl,-T,{0}" -f $case.Ld), '-o', $elf, $obj)
            if ($r.Rc -ne 0) {
                Fail ("link failed with placement {0} (rc={1})" -f $case.Tag, $r.Rc)
                foreach ($l in (($r.Out -split "`r?`n") | Where-Object { $_.Trim() -ne '' } | Select-Object -First 5)) {
                    Say ("        " + $l.Trim())
                }
                continue
            }
            $r2 = Invoke-Native -Exe $nm -Arguments @('--print-size', $elf)
            $line = @(($r2.Out -split "`r?`n") | Where-Object { $_ -match ('\b' + [regex]::Escape($arrayName) + '\s*$') }) |
                Select-Object -First 1
            if (-not $line) {
                Fail ("symbol not found in linked image ({0})" -f $case.Tag)
                continue
            }
            $m = [regex]::Match($line, '^\s*([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+(\S)\s+(\S+)\s*$')
            if (-not $m.Success) {
                Fail ("could not parse nm line ({0}): {1}" -f $case.Tag, $line.Trim())
                continue
            }
            $addr = [Convert]::ToUInt32($m.Groups[1].Value, 16)
            $inPsram = ($addr -ge 0x11000000 -and $addr -lt 0x11200000)
            if ($case.ExpectPsram) {
                if ($inPsram) { Ok ("placement ON  -> 0x{0:X8}  inside the fake PSRAM region" -f $addr) }
                else { Fail ("placement ON  -> 0x{0:X8}  expected inside 0x11000000..0x111FFFFF" -f $addr) }
            } else {
                if (-not $inPsram) { Ok ("placement OFF -> 0x{0:X8}  ordinary .bss, as expected" -f $addr) }
                else { Fail ("placement OFF -> 0x{0:X8}  still in PSRAM: the rule is not what moved it" -f $addr) }
            }
        }
    }
}
Say ""

if ($script:Bad -eq 0) {
    Say "MECHANISM PROBE: PASS"
    Say "  -fdata-sections + linker-script placement works on the target toolchain,"
    Say "  with upstream source left untouched."
    $code = 0
} else {
    Say ("MECHANISM PROBE: FAILED ({0} problem(s))" -f $script:Bad)
    $code = 1
}

$script:Lines | Set-Content -LiteralPath $ReportPath -Encoding UTF8
exit $code
