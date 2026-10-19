$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path $root ('build-sd-fixture-' + $PID)
$rom = Join-Path $fixture 'roms\STUDIO2'
New-Item -ItemType Directory -Force -Path $rom | Out-Null
& python (Join-Path $root 'testrom/generate.py') --output $rom
if ($LASTEXITCODE -ne 0) { throw 'Diagnostic generation failed' }
$hashes = @(Get-ChildItem $rom -File | Get-FileHash)
$save = Join-Path $fixture 'SAVES\STUDIO2'
New-Item -ItemType Directory -Force -Path $save | Out-Null
[IO.File]::WriteAllText((Join-Path $save 'user.keep'),'preserve-user-data')
$saveHash = (Get-FileHash (Join-Path $save 'user.keep')).Hash
for ($i = 0; $i -lt 2; $i++) {
    $result = @(& (Join-Path $PSScriptRoot 'install-studio2-bootloader-sd.ps1') -SdRoot $fixture -AllowDirectoryRootForTest)
    if ($result -notcontains 'HARDWARE READINESS: READY') { throw 'Diagnostic readiness failed' }
}
$index = Join-Path $fixture 'emu\emulators.txt'
if (@(Get-Content $index | Where-Object { $_ -eq 'studio2;studio2;RCA Studio II' }).Count -ne 1) { throw 'Duplicate/missing index row' }
foreach ($h in $hashes) { if ((Get-FileHash $h.Path).Hash -ne $h.Hash) { throw 'ROM changed' } }
if ((Get-FileHash (Join-Path $save 'user.keep')).Hash -ne $saveHash) { throw 'Save changed' }
$before = (Get-FileHash $index).Hash
$rejected = $false
try {
    & (Join-Path $PSScriptRoot 'install-studio2-bootloader-sd.ps1') -SdRoot $fixture -AllowDirectoryRootForTest -ApplicationUf2 (Join-Path $root 'build-pico-retropicoes-v21-make\studio2.uf2') | Out-Null
} catch { $rejected = $true }
if (-not $rejected -or (Get-FileHash $index).Hash -ne $before) { throw 'Standalone UF2 rejection failed' }
$full = Join-Path $fixture 'full'
New-Item -ItemType Directory -Force -Path (Join-Path $full 'emu') | Out-Null
$fullIndex = Join-Path $full 'emu\emulators.txt'
[IO.File]::WriteAllLines($fullIndex, @(0..31 | ForEach-Object { "existing$_;existing$_;Existing $_" }))
$before = (Get-FileHash $fullIndex).Hash
$rejected = $false
try { & (Join-Path $PSScriptRoot 'install-studio2-bootloader-sd.ps1') -SdRoot $full -AllowDirectoryRootForTest | Out-Null } catch { $rejected = $true }
if (-not $rejected -or (Get-FileHash $fullIndex).Hash -ne $before) { throw '32-row capacity rejection failed' }
$malformed = Join-Path $fixture 'malformed'
$badRoms = Join-Path $malformed 'roms\STUDIO2'
$badBios = Join-Path $malformed 'bios'
New-Item -ItemType Directory -Force -Path $badRoms,$badBios | Out-Null
[IO.File]::WriteAllBytes((Join-Path $badRoms 'bad.st2'), [byte[]]::new(512))
[IO.File]::WriteAllBytes((Join-Path $badBios 'studio2.rom'), [byte[]]::new(2048))
$result = @(& (Join-Path $PSScriptRoot 'install-studio2-bootloader-sd.ps1') -SdRoot $malformed -AllowDirectoryRootForTest)
if ($result -notcontains 'HARDWARE READINESS: BLOCKED') { throw 'Malformed content was marked READY' }
[IO.File]::WriteAllBytes((Join-Path $badRoms 'bounded.bin'), [byte[]]::new(256))
$result = @(& (Join-Path $PSScriptRoot 'install-studio2-bootloader-sd.ps1') -SdRoot $malformed -AllowDirectoryRootForTest)
if ($result -notcontains 'HARDWARE READINESS: READY') { throw 'Bounded raw candidate gate failed' }
Write-Output 'STUDIO2 INSTALLER: idempotence, diagnostic readiness, ROM/save preservation, UF2 address and capacity rejection PASS'
