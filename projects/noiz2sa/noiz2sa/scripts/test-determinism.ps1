[CmdletBinding()]
param(
    [string]$DataRoot = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$workspaceRoot = Split-Path -Parent $projectRoot
if ([string]::IsNullOrWhiteSpace($DataRoot)) {
    $DataRoot = Join-Path $workspaceRoot "_deps\noiz2sa"
}

$smoke = Join-Path $projectRoot "build-host\noiz2sa_host_smoke.exe"
if (-not (Test-Path $smoke)) {
    throw "build-host\noiz2sa_host_smoke.exe is missing; run build-host.ps1 first"
}

# The smoke executable imports libgcc_s/libstdc++ from the selected MinGW
# toolchain.  Prefer that directory for this process, just like build-host.ps1
# and test-target-boundary.ps1: without it Windows can load an incompatible
# copy from another toolchain and terminate before it emits a digest.
$makeCommand = Get-Command mingw32-make.exe -ErrorAction Stop
$hostToolchainBin = Split-Path -Parent $makeCommand.Source
$env:Path = "$hostToolchainBin;$env:Path"

function Invoke-NativeText {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        [Parameter()][AllowEmptyCollection()][string[]]$Arguments = @()
    )

    # quitLast() deliberately returns exit(1) after its atexit diagnostics.
    # With the caller's ErrorActionPreference set to Stop, Windows PowerShell
    # otherwise turns the redirected native stderr into an error record and
    # drops the stdout lines that contain the digest.  Capture both streams
    # while native-command errors are non-terminating; success is determined
    # solely by the digest below.
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

# Each replay run needs its own clean working directory: the engine reads
# the barrage directories and writes the preference file relative to cwd,
# and a pre-existing noiz2sa.prf would change the loaded state.
$tempBase = Join-Path ([IO.Path]::GetTempPath()) ("noiz2sa-determinism-" + [IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $tempBase | Out-Null

try {
    $digests = @()
    for ($run = 1; $run -le 2; $run++) {
        $runDir = Join-Path $tempBase "run$run"
        New-Item -ItemType Directory -Path $runDir | Out-Null
        # images/ is part of the minimal data set: the S17-C screen adapter
        # loads the title sprites during initSDL() and exits(1) without them.
        foreach ($dir in @("zako", "middle", "boss", "images")) {
            Copy-Item -Recurse -Path (Join-Path $DataRoot $dir) -Destination $runDir
        }
        $result = Invoke-NativeText -Exe $smoke -Arguments @($runDir)
        $output = $result.Lines
        $digest = $output | Select-String -Pattern "NOIZ2SA STATE DIGEST ([0-9a-f]+)"
        if (-not $digest) {
            Write-Output ($output | Select-String -Pattern "SMOKE")
            throw "run $run produced no state digest"
        }
        $digests += $digest.Matches[0].Groups[1].Value
        Write-Output "run $run : $($digest.Line)"
    }

    if ($digests[0] -ne $digests[1]) {
        throw "determinism FAILED: digests differ ($($digests[0]) vs $($digests[1]))"
    }
    Write-Output "NOIZ2SA DETERMINISM: PASS digest=$($digests[0])"
}
finally {
    Remove-Item -Recurse -Force $tempBase -ErrorAction SilentlyContinue
}
