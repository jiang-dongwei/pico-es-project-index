[CmdletBinding()]
param()

# S17-D boundary evidence for the noiz2sa port.
#
# Three claims that are otherwise only prose get decided here by the
# toolchain:
#
#   1. noiz2sa_stub_backend implements compat v1 without touching a host OS
#      API. Checked on its undefined symbols *and* its header closure, so a
#      windows.h that happens to define nothing still fails.
#
#   2. noiz2sa_target_probe links noiz2sa_game against that backend, and the
#      probe itself is host-header-free. The game archive therefore needs
#      nothing but compat v1 plus the C and C++ runtimes -- with one recorded
#      upstream exception that is not allowed to grow.
#
#   3. The backend swap changes nothing observable: the probe's framebuffer
#      digest must equal noiz2sa_host_smoke's.
#
# Run build-host.ps1 first.

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build-host"

# Keep the selected MinGW toolchain first in PATH for this process only. The
# probe links libgcc_s/libstdc++, and several other copies of those DLLs live
# on this machine, so the wrong one loads unless the toolchain wins the search.
$makeCommand = Get-Command mingw32-make.exe -ErrorAction Stop
$hostToolchainBin = Split-Path -Parent $makeCommand.Source
$env:Path = "$hostToolchainBin;$env:Path"

# Host OS API surface. These are exactly the entry points compat_host.c uses
# to be a *host* backend; a target backend reaches for FatFS/HSTX instead, so
# any of them appearing below the backend boundary is a portability defect.
$hostOsSymbols = @(
    'opendir', 'readdir', 'closedir', 'scandir',
    'mkdir', '_mkdir', 'rmdir', '_rmdir',
    'chdir', '_chdir',
    'Sleep', 'QueryPerformanceCounter', 'QueryPerformanceFrequency',
    'GetTickCount', 'GetTickCount64', 'timeGetTime',
    'clock_gettime', 'nanosleep', 'gettimeofday',
    'FindFirstFileA', 'FindFirstFileW', 'FindNextFileA', 'FindNextFileW',
    'CreateFileA', 'CreateFileW', 'ReadFile', 'WriteFile', 'CloseHandle',
    'VirtualAlloc', 'HeapAlloc', 'GetProcessHeap',
    'fopen_s', 'localtime_s', 'strerror_s'
)

# Note the deliberate omissions: malloc.h arrives with MinGW's <stdlib.h>, and
# sys/timeb.h with its <time.h>. Both are ISO C header plumbing, not OS APIs.
$hostOsHeaders = @(
    'windows.h', 'direct.h', 'dirent.h', 'unistd.h', 'io.h', 'process.h',
    'conio.h', 'fcntl.h', 'winsock2.h', 'winbase.h', 'winnt.h',
    'minwindef.h', 'sys/stat.h', 'sys/types.h', 'sys/time.h', 'sys/mman.h',
    'sys/ioctl.h'
)

# The noiz2sa engine is now clean.  Keep this list empty: an OS header in an
# upstream translation unit is a newly introduced portability leak and fails
# the boundary gate rather than becoming an undocumented exception.
$knownUpstreamGaps = @()

function Invoke-NativeText {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        # The runners take no arguments, so this must accept an empty array.
        [Parameter()][AllowEmptyCollection()][string[]]$Arguments = @()
    )

    # PS 5.1 promotes redirected native stderr to a terminating error under
    # ErrorActionPreference = "Stop"; nm and the runners both write there.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $text = @(& $Exe @Arguments 2>&1 | ForEach-Object { [string]$_ })
        $code = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
    return [pscustomobject]@{ Lines = $text; ExitCode = $code }
}

function Get-UndefinedSymbols {
    param([Parameter(Mandatory = $true)][string]$Archive)

    $result = Invoke-NativeText -Exe $nmExe -Arguments @(
        '--undefined-only', $Archive)
    if ($result.ExitCode -ne 0) {
        throw "nm failed on $Archive (exit $($result.ExitCode))"
    }
    return @($result.Lines | ForEach-Object {
            $parts = $_.Trim() -split '\s+'
            # Two-field lines are "U symbol"; archive/object headers have one.
            if ($parts.Count -ge 2) { $parts[$parts.Count - 1] }
        } | Where-Object { $_ } | Sort-Object -Unique)
}

# Returns one entry per (translation unit, host header) pair. The layer is
# decided by the *unit's* mirrored path, not by the header: dirent.h lives in
# the MinGW sysroot, so classifying by header would file it under the port.
function Get-HostHeaderHits {
    param([Parameter(Mandatory = $true)][string]$TargetDirectory)

    $entries = @()
    $seen = @{}
    $files = @(Get-ChildItem -Path $TargetDirectory -Recurse -Filter '*.obj.d' -ErrorAction SilentlyContinue)
    foreach ($file in $files) {
        $unit = $file.Name -replace '\.obj\.d$', ''
        $layer = "port"
        if ($file.FullName -match '[\\/]_deps[\\/]') { $layer = "upstream" }

        # GCC emits one logical line with backslash continuations.
        $flat = (Get-Content -Raw -LiteralPath $file.FullName) -replace "\\\r?\n", ' '
        foreach ($token in ($flat -split '\s+')) {
            if ($token -notmatch '\.h$') { continue }
            $normalized = ($token -replace '\\', '/').ToLowerInvariant()
            foreach ($key in $hostOsHeaders) {
                if (-not $normalized.EndsWith('/' + $key)) { continue }
                $dedup = "$unit|$key"
                if ($seen.ContainsKey($dedup)) { break }
                $seen[$dedup] = $true
                $entries += [pscustomobject]@{
                    Unit   = $unit
                    Header = $key
                    Layer  = $layer
                }
                break
            }
        }
    }
    return @($entries | Sort-Object Layer, Unit, Header)
}

function New-DataFixture {
    param(
        [Parameter(Mandatory = $true)][string]$Parent,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $directory = Join-Path $Parent $Name
    New-Item -ItemType Directory -Path $directory | Out-Null
    foreach ($sub in @('zako', 'middle', 'boss', 'images')) {
        $source = Join-Path $script:dataRoot $sub
        $destination = Join-Path $directory $sub
        Copy-Item -Recurse -Path $source -Destination $directory

        # compat_stub.c deliberately owns no host directory API.  Its probe
        # implementation consumes this UTF-8 filename manifest instead,
        # while the regular host backend continues to enumerate the same
        # copied files directly.  Only BulletML directories need one.
        if ($sub -ne 'images') {
            $names = @(Get-ChildItem -LiteralPath $destination -File -Filter '*.xml' |
                       ForEach-Object { $_.Name })
            [IO.File]::WriteAllLines((Join-Path $destination '.compat-dir'),
                                      [string[]]$names,
                                      (New-Object Text.UTF8Encoding($false)))
        }
    }
    return $directory
}

function Invoke-Runner {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        [Parameter(Mandatory = $true)][string]$Directory
    )

    Push-Location $Directory
    try {
        $result = Invoke-NativeText -Exe $Exe -Arguments @()
    }
    finally {
        Pop-Location
    }
    # Upstream quitLast() ends in exit(1) -- its normal exit path -- so success
    # is judged by the PASS line, never by the process exit code.
    return $result
}

# ---------------------------------------------------------------------------

$nmExe = Join-Path $hostToolchainBin "nm.exe"
if (-not (Test-Path $nmExe)) {
    throw "nm.exe not found next to mingw32-make.exe ($nmExe)"
}

$gameArchive = Join-Path $buildDir "libnoiz2sa_game.a"
$stubArchive = Join-Path $buildDir "libnoiz2sa_stub_backend.a"
$probeExe = Join-Path $buildDir "noiz2sa_target_probe.exe"
$smokeExe = Join-Path $buildDir "noiz2sa_host_smoke.exe"

foreach ($path in @($gameArchive, $stubArchive, $probeExe, $smokeExe)) {
    if (-not (Test-Path $path)) {
        throw "$path is missing; run build-host.ps1 first"
    }
}

$script:dataRoot = Join-Path (Split-Path -Parent $projectRoot) "_deps\noiz2sa"
$problems = @()

# --- A: the stub backend is host-API-free ------------------------------------
$stubSymbols = Get-UndefinedSymbols -Archive $stubArchive
$stubSymbolHits = @($stubSymbols | Where-Object { $hostOsSymbols -contains $_ })
$stubHeaderHits = @(Get-HostHeaderHits -TargetDirectory (Join-Path $buildDir "CMakeFiles\noiz2sa_stub_backend.dir"))

# --- B: the probe is host-header-free ---------------------------------------
$probeHeaderHits = @(Get-HostHeaderHits -TargetDirectory (Join-Path $buildDir "CMakeFiles\noiz2sa_target_probe.dir"))

# --- C: the game archive, split into port layer and upstream -----------------
$gameHits = @(Get-HostHeaderHits -TargetDirectory (Join-Path $buildDir "CMakeFiles\noiz2sa_game.dir"))
$portHits = @($gameHits | Where-Object { $_.Layer -eq 'port' })
$upstreamHits = @($gameHits | Where-Object { $_.Layer -eq 'upstream' })

$upstreamHeaderNames = @($upstreamHits | ForEach-Object { $_.Header } | Sort-Object -Unique)
$upstreamUnits = @($upstreamHits | ForEach-Object { $_.Unit } | Sort-Object -Unique)
$newUpstreamGaps = @($upstreamHeaderNames | Where-Object { $knownUpstreamGaps -notcontains $_ })

foreach ($hit in @($stubHeaderHits + $probeHeaderHits)) {
    $problems += "$($hit.Unit) includes $($hit.Header)"
}
foreach ($hit in $portHits) {
    $problems += "port layer: $($hit.Unit) includes $($hit.Header)"
}
if ($stubSymbolHits.Count -gt 0) {
    $problems += "stub backend references host OS symbols: $($stubSymbolHits -join ', ')"
}
if ($newUpstreamGaps.Count -gt 0) {
    $problems += "upstream closure gained host headers: $($newUpstreamGaps -join ', ') (units: $($upstreamUnits -join ', '))"
}

$gameSymbols = Get-UndefinedSymbols -Archive $gameArchive
$gameCompatNeeds = @($gameSymbols | Where-Object { $_ -like 'compat_*' } | Sort-Object -Unique)

# ---------------------------------------------------------------------------

Write-Output "=== A: stub backend is host-API-free ==="
Write-Output ("  undefined symbols : {0} (all ISO C: {1})" -f $stubSymbols.Count, ($stubSymbols -join ' '))
Write-Output ("  host OS symbols   : {0}" -f $(if ($stubSymbolHits.Count) { $stubSymbolHits -join ', ' } else { 'none' }))
Write-Output ("  host OS headers   : {0}" -f $(if ($stubHeaderHits.Count) { ($stubHeaderHits | ForEach-Object { $_.Header }) -join ', ' } else { 'none' }))

Write-Output ""
Write-Output "=== B: probe is host-header-free ==="
Write-Output ("  host OS headers   : {0}" -f $(if ($probeHeaderHits.Count) { ($probeHeaderHits | ForEach-Object { $_.Header }) -join ', ' } else { 'none' }))

Write-Output ""
Write-Output "=== C: game archive, port layer clean + upstream gap ratcheted ==="
Write-Output ("  port layer leaks  : {0}" -f $(if ($portHits.Count) { ($portHits | ForEach-Object { "$($_.Unit):$($_.Header)" }) -join ', ' } else { 'none' }))
Write-Output ("  upstream gap      : {0}" -f $(if ($upstreamHeaderNames.Count) { $upstreamHeaderNames -join ', ' } else { 'none' }))
Write-Output ("  upstream gap limit: {0}" -f ($knownUpstreamGaps -join ', '))
if ($upstreamUnits.Count -gt 0) {
    Write-Output ("  upstream gap from : {0}" -f ($upstreamUnits -join ', '))
}
$stillPresent = @($knownUpstreamGaps | Where-Object { $upstreamHeaderNames -contains $_ })
if ($stillPresent.Count -lt $knownUpstreamGaps.Count) {
    $resolved = @($knownUpstreamGaps | Where-Object { $upstreamHeaderNames -notcontains $_ })
    Write-Output ("  NOTE gap closed for: {0} -- tighten `$knownUpstreamGaps so the fix cannot regress." -f ($resolved -join ', '))
}
Write-Output ("  compat_* needed   : {0}" -f $gameCompatNeeds.Count)
Write-Output ("    {0}" -f ($gameCompatNeeds -join ' '))

$tempBase = Join-Path ([IO.Path]::GetTempPath()) ("noiz2sa-boundary-" + [IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $tempBase | Out-Null

$probeHash = $null
$probeNonZero = -1
try {
    $probeRun = Invoke-Runner -Exe $probeExe -Directory (New-DataFixture -Parent $tempBase -Name "probe")
    $smokeRun = Invoke-Runner -Exe $smokeExe -Directory (New-DataFixture -Parent $tempBase -Name "smoke")
    $probeText = $probeRun.Lines -join "`n"
    $smokeText = $smokeRun.Lines -join "`n"

    $match = [regex]::Match($probeText, 'TARGET PROBE FRAME HASH ([0-9a-f]{16}) nonzero=(\d+)')
    if ($match.Success) {
        $probeHash = $match.Groups[1].Value
        $probeNonZero = [int]$match.Groups[2].Value
    }

    $smokeHash = $null
    $match = [regex]::Match($smokeText, 'FRAME HASH ([0-9a-f]{16}) nonzero=(\d+)')
    if ($match.Success) { $smokeHash = $match.Groups[1].Value }

    Write-Output ""
    Write-Output "=== D: cross-backend render equality ==="
    Write-Output ("  probe (stub backend) : {0}" -f $(if ($probeHash) { $probeHash } else { 'NO HASH' }))
    Write-Output ("  smoke (host backend) : {0}" -f $(if ($smokeHash) { $smokeHash } else { 'NO HASH' }))

    if ($probeText -notmatch 'TARGET PROBE: PASS') { $problems += "probe did not report PASS" }
    if (-not $probeHash) { $problems += "probe produced no framebuffer digest" }
    if (-not $smokeHash) { $problems += "smoke produced no framebuffer digest" }
    if ($probeHash -and $smokeHash -and ($probeHash -ne $smokeHash)) {
        $problems += "backend swap changed rendering: probe=$probeHash smoke=$smokeHash"
    }
    $expected = 320 * 240
    if ($probeNonZero -ne $expected) {
        $problems += "probe framebuffer is not fully painted (nonzero=$probeNonZero, expected $expected)"
    }
}
finally {
    Remove-Item -Recurse -Force $tempBase -ErrorAction SilentlyContinue
}

Write-Output ""
if ($problems.Count -gt 0) {
    foreach ($problem in $problems) { Write-Output "FAIL $problem" }
    throw "NOIZ2SA TARGET BOUNDARY: FAIL ($($problems.Count) problem(s))"
}

Write-Output ("NOIZ2SA TARGET BOUNDARY: PASS framebuffer=$probeHash nonzero=$probeNonZero")
