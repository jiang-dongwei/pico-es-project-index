#Requires -Version 5.1
<#
.SYNOPSIS
    Pre-flight gate for Keen Dreams runtime data (S20 / ReflectionHLE port).

.DESCRIPTION
    Verifies that at least one *fully supported* Keen Dreams data set is present
    on the target medium, using the exact file name / byte size / CRC-32 triples
    that upstream encodes in

        src/backend/gamedefs/be_gamedefs_kdreams.h

    (ReflectionHLE release-20260606, commit 74f07efcc40142a5727caacebec848c48b141eca).

    This implements the RUNTIME-DATA-GATE requirement "check at least one
    bootable object, not just that a directory exists" at the strongest level
    available for this port: byte-size plus CRC-32 per required file.

    Emits the two mandatory status lines:

        CONTENT INSTALL: PASS|FAILED
        HARDWARE READINESS: READY|BLOCKED

    Only READY means it is safe to tell the user to eject the card and test.

    Nothing is written, moved or deleted on the scanned medium. The only file
    this script writes is the optional -ReportPath handoff report.

.PARAMETER DataRoot
    Directory to scan. The directory itself and each of its immediate
    subdirectories are treated as candidate game installations.
    Convention used by this project: /data/refkeen/ on the SD card.

.PARAMETER SkipCrc
    Fast pass: check file presence and exact byte size only. Always reports
    HARDWARE READINESS: BLOCKED, because CRC verification was skipped.

.PARAMETER ReportPath
    Optional path for a plain-text copy of the whole report. Useful on hosts
    where a child PowerShell process' stdout cannot be captured, and as
    handoff / acceptance evidence.

.PARAMETER SelfTest
    Verify the CRC-32 implementation against published test vectors and check
    the embedded version table for consistency. Runs without any game data.

.PARAMETER CrcFile
    Compute the CRC-32 of one arbitrary file and exit. This exists so the
    implementation above can be cross-checked against an independent tool,
    e.g.

        python -c "import zlib,sys;print(hex(zlib.crc32(open(sys.argv[1],'rb').read())))" <file>

    A published-vector self test alone cannot prove the streaming/file path
    (buffer boundaries, >64 KiB files) is correct; a second implementation can.

.EXAMPLE
    & refkeen/scripts/Preflight-KeenData.ps1 -SelfTest

.EXAMPLE
    & refkeen/scripts/Preflight-KeenData.ps1 -CrcFile 'C:\tmp\sample.bin'

.EXAMPLE
    & refkeen/scripts/Preflight-KeenData.ps1 -DataRoot 'H:\data\refkeen' `
        -ReportPath '.emv2\logs\keen-datagate.txt'
#>
[CmdletBinding()]
param(
    [string]$DataRoot,
    [string]$ReportPath,
    [string]$CrcFile,
    [switch]$SkipCrc,
    [switch]$SelfTest
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Report plumbing. Output always goes to the pipeline; -ReportPath additionally
# mirrors it to disk so it survives hosts that swallow child stdout.
# ---------------------------------------------------------------------------
$script:Report = New-Object 'System.Collections.Generic.List[string]'

function Emit {
    param([string]$Line)
    $script:Report.Add($Line) | Out-Null
    Write-Output $Line
}

function Flush-Report {
    if (-not $ReportPath) { return }
    $dir = Split-Path -Parent $ReportPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    Set-Content -LiteralPath $ReportPath -Value $script:Report -Encoding UTF8
}

# ---------------------------------------------------------------------------
# Required data, transcribed from be_gamedefs_kdreams.h (g_be_reqgameverfiles_*)
# ---------------------------------------------------------------------------
$KeenVersions = @(
    [pscustomobject]@{
        Id    = 'KDREAMSE100'
        Name  = 'Keen Dreams EGA v1.00 (registered)'
        Audio = 'PC speaker + OPL'
        Files = @(
            @{ N = 'AUDIO.KDR';    S = 3498;   C = 0x80ac85e5L },
            @{ N = 'EGAGRAPH.KDR'; S = 213045; C = 0x2dc94687L },
            @{ N = 'GAMEMAPS.KDR'; S = 63497;  C = 0x7b517fa0L },
            @{ N = 'KDREAMS.EXE';  S = 77694;  C = 0xc73b8cb2L }
        )
    }
    [pscustomobject]@{
        Id    = 'KDREAMSC100'
        Name  = 'Keen Dreams CGA v1.00'
        Audio = 'PC speaker + OPL'
        Files = @(
            @{ N = 'AUDIO.KDR';    S = 3498;   C = 0x80ac85e5L },
            @{ N = 'CGAGRAPH.KDR'; S = 134691; C = 0x05e32626L },
            @{ N = 'GAMEMAPS.KDR'; S = 63497;  C = 0x7b517fa0L },
            @{ N = 'KDREAMS.EXE';  S = 75015;  C = 0xb6ff595aL }
        )
    }
    [pscustomobject]@{
        Id    = 'KDREAMSE113'
        Name  = 'Keen Dreams EGA v1.13 (shareware)'
        Audio = 'PC speaker + OPL'
        Files = @(
            @{ N = 'KDREAMS.AUD'; S = 3498;   C = 0x80ac85e5L },
            @{ N = 'KDREAMS.CMP'; S = 14189;  C = 0x97628ca0L },
            @{ N = 'KDREAMS.EGA'; S = 213045; C = 0x2dc94687L },
            @{ N = 'KDREAMS.EXE'; S = 81619;  C = 0x9dce0a39L },
            @{ N = 'KDREAMS.MAP'; S = 65673;  C = 0x8dce09afL },
            @{ N = 'LAST.SHL';    S = 1634;   C = 0xc0a3560fL }
        )
    }
    [pscustomobject]@{
        Id    = 'KDREAMSC105'
        Name  = 'Keen Dreams CGA v1.05'
        Audio = 'PC speaker + OPL'
        Files = @(
            @{ N = 'AUDIO.KDR';    S = 3498;   C = 0x80ac85e5L },
            @{ N = 'CGAGRAPH.KDR'; S = 134691; C = 0x05e32626L },
            @{ N = 'GAMEMAPS.KDR'; S = 65736;  C = 0x4b92df5fL },
            @{ N = 'KDREAMS.EXE';  S = 78253;  C = 0x5af7ce2bL }
        )
    }
    [pscustomobject]@{
        Id    = 'KDREAMSE193'
        Name  = 'Keen Dreams EGA v1.93 (registered)'
        Audio = 'PC speaker + OPL'
        Files = @(
            @{ N = 'KDREAMS.AUD'; S = 3498;   C = 0x80ac85e5L },
            @{ N = 'KDREAMS.CMP'; S = 14620;  C = 0x1681bc21L },
            @{ N = 'KDREAMS.EGA'; S = 213045; C = 0x2dc94687L },
            @{ N = 'KDREAMS.EXE'; S = 80715;  C = 0x3d0cd9edL },
            @{ N = 'KDREAMS.MAP'; S = 65674;  C = 0x78bd24fcL }
        )
    }
    [pscustomobject]@{
        Id    = 'KDREAMSE120'
        Name  = 'Keen Dreams EGA v1.20 (shareware)'
        Audio = 'PC speaker + OPL'
        Files = @(
            @{ N = 'KDREAMS.AUD'; S = 3498;   C = 0x80ac85e5L },
            @{ N = 'KDREAMS.CMP'; S = 14620;  C = 0x1681bc21L },
            @{ N = 'KDREAMS.EGA'; S = 213045; C = 0x2dc94687L },
            @{ N = 'KDREAMS.EXE'; S = 81519;  C = 0xf36f0edfL },
            @{ N = 'KDREAMS.MAP'; S = 65674;  C = 0x78bd24fcL }
        )
    }
    [pscustomobject]@{
        Id    = 'KDREAMS2015'
        Name  = 'Keen Dreams 2015 (all data external, no EXE)'
        Audio = 'Digital audio'
        Files = @(
            @{ N = 'CONTEXT.KDR';   S = 1331;    C = 0x70920ec3L },
            @{ N = 'EGADICT.CGA';   S = 1024;    C = 0x75a458f2L },
            @{ N = 'EGADICT.KDR';   S = 1024;    C = 0xfe21f5c4L },
            @{ N = 'EGAHEAD.CGA';   S = 12068;   C = 0x38c15a01L },
            @{ N = 'EGAHEAD.KDR';   S = 12068;   C = 0xdeb318b3L },
            @{ N = 'GAMETEXT.KDR';  S = 4314;    C = 0x0996e7f2L },
            @{ N = 'KDREAMS.CGA';   S = 179725;  C = 0x83b0c6c4L },
            @{ N = 'KDREAMS.CMP';   S = 40878;   C = 0xf8a33e2fL },
            @{ N = 'KDREAMS.EGA';   S = 213221;  C = 0xee1024b9L },
            @{ N = 'KDREAMS.MAP';   S = 65674;   C = 0x934cd898L },
            @{ N = 'KDREAMS.SND';   S = 1449826; C = 0xfb74b9a2L },
            @{ N = 'MAPDICT.KDR';   S = 1020;    C = 0x6bb0de32L },
            @{ N = 'MAPHEAD.KDR';   S = 11824;   C = 0xff335e8cL },
            @{ N = 'SOUNDDCT.KDR';  S = 1024;    C = 0x2217ceb9L },
            @{ N = 'SOUNDHHD.KDR';  S = 228;     C = 0xb8b39c5cL },
            @{ N = 'STORY.KDR';     S = 2526;    C = 0xcafc1d15L }
        )
    }
)

# ---------------------------------------------------------------------------
# CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320) -- pure PowerShell.
#
# int64 is used on purpose: it avoids the wrapped/negative arithmetic that
# [uint32] produces inside Windows PowerShell 5.1. Values always stay in
# [0, 2^32), so the right shift behaves as a logical shift.
#
# GOTCHA, verified on 5.1.26100.9444 -- the seed MUST be written 0xFFFFFFFFL.
#   [int64]0xFFFFFFFF  ->  -1          (bare literal is parsed as Int32 first)
#   0xFFFFFFFFL        ->  4294967295  (the L suffix forces Int64 parsing)
# Seeding with -1 does not merely misprint the high half: -shr 8 becomes an
# arithmetic shift that fills bit 31 with ones on every step, so the low 32
# bits -- the only ones that matter -- are corrupted too. Symptom is a
# 16-hex-digit result like 0xFFFFFFFF20D421B6 instead of 0xCBF43926.
# Hence: always seed from $script:Crc32Mask and never from a bare literal.
# ---------------------------------------------------------------------------
$script:Crc32Mask = 0xFFFFFFFFL
$script:CrcTable = $null

function Format-Crc32 {
    param([int64]$Value)
    return ('0x{0:X8}' -f ($Value -band $script:Crc32Mask))
}

function Initialize-CrcTable {
    if ($null -ne $script:CrcTable) { return }
    $t = New-Object 'int64[]' 256
    for ($i = 0; $i -lt 256; $i++) {
        $c = [int64]$i
        for ($k = 0; $k -lt 8; $k++) {
            if (($c -band 1) -ne 0) { $c = 0xEDB88320L -bxor ($c -shr 1) }
            else                    { $c = $c -shr 1 }
            $c = $c -band $script:Crc32Mask
        }
        $t[$i] = $c
    }
    $script:CrcTable = $t
}

function Get-BufferCrc32 {
    param([byte[]]$Buffer, [int]$Count)
    Initialize-CrcTable
    $crc = $script:Crc32Mask
    for ($i = 0; $i -lt $Count; $i++) {
        $crc = $script:CrcTable[($crc -bxor $Buffer[$i]) -band 0xFF] -bxor ($crc -shr 8)
    }
    return [int64]($crc -bxor $script:Crc32Mask)
}

function Get-FileCrc32 {
    param([string]$Path)
    Initialize-CrcTable
    $crc = $script:Crc32Mask
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $buffer = New-Object byte[] 65536
        while (($n = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            for ($i = 0; $i -lt $n; $i++) {
                $crc = $script:CrcTable[($crc -bxor $buffer[$i]) -band 0xFF] -bxor ($crc -shr 8)
            }
        }
    }
    finally { $stream.Dispose() }
    return [int64]($crc -bxor $script:Crc32Mask)
}

# ---------------------------------------------------------------------------
# Self test
# ---------------------------------------------------------------------------
function Invoke-SelfTest {
    $failures = 0

    $text = [System.Text.Encoding]::ASCII.GetBytes('123456789')
    $got = Get-BufferCrc32 -Buffer $text -Count $text.Length
    if ($got -ne 0xCBF43926L) {
        Emit ("FAIL  CRC32('123456789') = {0}, expected 0xCBF43926" -f (Format-Crc32 $got))
        Emit ("      raw int64 = {0} (negative means a bad seed, see the note above)" -f $got)
        $failures++
    } else {
        Emit "ok    CRC32('123456789') = 0xCBF43926"
    }

    $empty = New-Object byte[] 0
    $got = Get-BufferCrc32 -Buffer $empty -Count 0
    if ($got -ne 0x00000000L) {
        Emit ("FAIL  CRC32('') = {0}, expected 0x00000000" -f (Format-Crc32 $got))
        $failures++
    } else {
        Emit "ok    CRC32('') = 0x00000000"
    }

    $fox = [System.Text.Encoding]::ASCII.GetBytes('The quick brown fox jumps over the lazy dog')
    $got = Get-BufferCrc32 -Buffer $fox -Count $fox.Length
    if ($got -ne 0x414FA339L) {
        Emit ("FAIL  CRC32(fox) = {0}, expected 0x414FA339" -f (Format-Crc32 $got))
        $failures++
    } else {
        Emit "ok    CRC32(fox) = 0x414FA339"
    }

    # Odd-length vector: catches a seed/shift defect that happens to cancel
    # out on the 9-byte and 43-byte vectors above (both odd, but different
    # alignment). "a" is the canonical single-byte IEEE vector.
    $a = [System.Text.Encoding]::ASCII.GetBytes('a')
    $got = Get-BufferCrc32 -Buffer $a -Count 1
    if ($got -ne 0xE8B7BE43L) {
        Emit ("FAIL  CRC32('a') = {0}, expected 0xE8B7BE43" -f (Format-Crc32 $got))
        $failures++
    } else {
        Emit "ok    CRC32('a') = 0xE8B7BE43"
    }

    # Guard the seed explicitly. A negative seed corrupts the result in a way
    # that looks like a formatting problem but is not -- see the GOTCHA note
    # above the CRC section.
    if ($script:Crc32Mask -ne 4294967295L) {
        Emit ("FAIL  CRC seed mask is {0}, expected 4294967295 (0xFFFFFFFFL)" -f $script:Crc32Mask)
        $failures++
    } else {
        Emit "ok    CRC seed mask is 4294967295 (0xFFFFFFFFL)"
    }

    # Probe the lookup table directly, bypassing the main loop, so a table
    # defect cannot be masked by a compensating loop defect.
    Initialize-CrcTable
    $t1 = $script:CrcTable[1]
    $t255 = $script:CrcTable[255]
    if (($t1 -ne 0x77073096L) -or ($t255 -ne 0x2D02EF8DL)) {
        Emit ("FAIL  CRC table probe: [1]={0} [255]={1}, expected 0x77073096 / 0x2D02EF8D" -f `
              (Format-Crc32 $t1), (Format-Crc32 $t255))
        $failures++
    } else {
        Emit "ok    CRC table probe [1]/[255] match published values"
    }

    if ($KeenVersions.Count -ne 7) {
        Emit ("FAIL  version table has {0} entries, expected 7" -f $KeenVersions.Count)
        $failures++
    } else {
        Emit "ok    version table has 7 entries"
    }

    $ids = @($KeenVersions | ForEach-Object { $_.Id } | Sort-Object -Unique)
    if ($ids.Count -ne $KeenVersions.Count) {
        Emit "FAIL  duplicate version ids in table"
        $failures++
    } else {
        Emit "ok    version ids are unique"
    }

    $totalFiles = 0
    foreach ($v in $KeenVersions) {
        if ($v.Files.Count -eq 0) {
            Emit ("FAIL  {0} has no required files" -f $v.Id)
            $failures++
        }
        foreach ($f in $v.Files) {
            $totalFiles++
            if ($f.S -le 0) {
                Emit ("FAIL  {0}/{1} has non-positive size" -f $v.Id, $f.N)
                $failures++
            }
            if (($f.C -lt 0) -or ($f.C -gt 0xFFFFFFFFL)) {
                Emit ("FAIL  {0}/{1} has out-of-range CRC32" -f $v.Id, $f.N)
                $failures++
            }
        }
    }
    Emit ("ok    {0} required-file entries validated" -f $totalFiles)

    if ($failures -ne 0) {
        Emit ("SELF TEST: FAILED ({0} problem(s))" -f $failures)
        return $false
    }
    Emit "SELF TEST: PASS"
    return $true
}

# ---------------------------------------------------------------------------
# Candidate evaluation
# ---------------------------------------------------------------------------
function Test-CandidateDirectory {
    param([string]$Directory, [bool]$NoCrc)

    $rows = @()
    foreach ($v in $KeenVersions) {
        $present = 0
        $sizeOk = 0
        $crcOk = 0
        foreach ($f in $v.Files) {
            $p = Join-Path $Directory $f.N
            if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { continue }
            $present++
            $len = (Get-Item -LiteralPath $p).Length
            if ($len -ne $f.S) { continue }
            $sizeOk++
            if ($NoCrc) { continue }
            $crc = Get-FileCrc32 -Path $p
            if ($crc -eq $f.C) { $crcOk++ }
        }
        $matches = $false
        if ($NoCrc) { $matches = ($sizeOk -eq $v.Files.Count) }
        else        { $matches = ($crcOk  -eq $v.Files.Count) }
        $rows += [pscustomobject]@{
            Name    = $v.Name
            Audio   = $v.Audio
            Total   = $v.Files.Count
            Present = $present
            SizeOk  = $sizeOk
            CrcOk   = $crcOk
            Matches = $matches
        }
    }
    return $rows
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if ($SelfTest) {
    Emit "S20 KEEN DATA GATE -- SELF TEST"
    Emit ""
    $ok = Invoke-SelfTest
    Emit ""
    Flush-Report
    if ($ok) { exit 0 } else { exit 1 }
}

if ($CrcFile) {
    Emit "S20 KEEN DATA GATE -- SINGLE FILE CRC-32"
    Emit ""
    if (-not (Test-Path -LiteralPath $CrcFile -PathType Leaf)) {
        Emit ("NOT FOUND  {0}" -f $CrcFile)
        Emit ""
        Emit "CONTENT INSTALL: FAILED"
        Emit "HARDWARE READINESS: BLOCKED"
        Flush-Report
        exit 1
    }
    $len = (Get-Item -LiteralPath $CrcFile).Length
    $crc = Get-FileCrc32 -Path $CrcFile
    Emit ("file     : {0}" -f $CrcFile)
    Emit ("size     : {0} bytes" -f $len)
    Emit ("crc32    : {0}" -f (Format-Crc32 $crc))
    Emit ("decimal  : {0}" -f $crc)
    Emit ""
    Flush-Report
    exit 0
}

if (-not $DataRoot) {
    $DataRoot = Join-Path (Get-Location) '_data\refkeen'
}

Emit "S20 KEEN DATA GATE"
Emit "Upstream   : ReflectionHLE release-20260606 / 74f07efcc40142a5727caacebec848c48b141eca"
Emit ("Data root  : {0}" -f $DataRoot)
if ($SkipCrc) { Emit "Mode       : size only (CRC skipped)" }
else          { Emit "Mode       : exact byte size + CRC-32" }
Emit ""

$candidates = @()
if (Test-Path -LiteralPath $DataRoot -PathType Container) {
    $candidates += (Get-Item -LiteralPath $DataRoot)
    foreach ($d in (Get-ChildItem -LiteralPath $DataRoot -Directory)) { $candidates += $d }
}

$ready = $false
$readyDir = $null
$readyName = $null

if ($candidates.Count -eq 0) {
    Emit ("NOT FOUND  {0} does not exist or is not a directory." -f $DataRoot)
    Emit ""
} else {
    foreach ($c in $candidates) {
        $rows = Test-CandidateDirectory -Directory $c.FullName -NoCrc:$SkipCrc
        $best = $rows | Sort-Object -Property @{ Expression = 'CrcOk';   Descending = $true },
                                             @{ Expression = 'SizeOk';  Descending = $true },
                                             @{ Expression = 'Present'; Descending = $true } |
                        Select-Object -First 1
        $hits = @($rows | Where-Object { $_.Matches })

        if ($hits.Count -gt 0) {
            foreach ($h in $hits) {
                Emit ("MATCH      {0}" -f $c.FullName)
                Emit ("           {0}  [{1}]" -f $h.Name, $h.Audio)
                Emit ("           all {0} required file(s) verified" -f $h.Total)
                if ($null -eq $readyDir) {
                    $readyDir = $c.FullName
                    $readyName = $h.Name
                }
            }
            $ready = $true
        } else {
            Emit ("NO MATCH   {0}" -f $c.FullName)
            Emit ("           closest: {0} -- {1}/{2} byte sizes match" -f `
                  $best.Name, $best.SizeOk, $best.Total)
            if (-not $SkipCrc) {
                Emit ("           closest: {0}/{1} CRC-32 verified" -f $best.CrcOk, $best.Total)
            }
            $closest = $KeenVersions | Where-Object { $_.Name -eq $best.Name } | Select-Object -First 1
            foreach ($f in $closest.Files) {
                $p = Join-Path $c.FullName $f.N
                if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
                    Emit ("           missing  {0} ({1} bytes, CRC32 {2})" -f $f.N, $f.S, (Format-Crc32 $f.C))
                } else {
                    $len = (Get-Item -LiteralPath $p).Length
                    if ($len -ne $f.S) {
                        Emit ("           bad size {0}: found {1}, expected {2}" -f $f.N, $len, $f.S)
                    } elseif (-not $SkipCrc) {
                        $crc = Get-FileCrc32 -Path $p
                        if ($crc -ne $f.C) {
                            Emit ("           bad crc  {0}: {1}, expected {2}" -f `
                                  $f.N, (Format-Crc32 $crc), (Format-Crc32 $f.C))
                        }
                    }
                }
            }
        }
        Emit ""
    }
}

Emit "---------------------------------------------------------------------------"
Emit "Supported data sets (Keen Dreams):"
foreach ($v in $KeenVersions) {
    Emit ("  {0,-16} {1,2} file(s)  audio: {2}" -f $v.Id, $v.Files.Count, $v.Audio)
}
Emit ""

if ($ready) { Emit "CONTENT INSTALL: PASS" } else { Emit "CONTENT INSTALL: FAILED" }

if ($ready -and (-not $SkipCrc)) {
    Emit "HARDWARE READINESS: READY"
    Emit ""
    Emit ("Ready data set: {0}" -f $readyName)
    Emit ("Location      : {0}" -f $readyDir)
    Emit "Only now may the user be told to eject the card and boot the board."
    Emit ""
    Flush-Report
    exit 0
}

Emit "HARDWARE READINESS: BLOCKED"
Emit ""
if ($ready -and $SkipCrc) {
    Emit "Reason: -SkipCrc was used, so file contents were not verified."
    Emit "Action: re-run without -SkipCrc before declaring the data set usable."
} else {
    Emit "Reason: no scanned directory contains a complete Keen Dreams data set."
    Emit "Action: install the original Keen Dreams files listed above."
    Emit "This is an external-data problem. Do not report it as a simulator,"
    Emit "compat, SD-driver or hardware fault."
}
Emit ""
Flush-Report
exit 1
