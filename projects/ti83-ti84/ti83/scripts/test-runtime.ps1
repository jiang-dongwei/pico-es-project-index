[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SystemRom)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$system=[IO.Path]::GetFullPath($SystemRom)
if(-not (Test-Path -LiteralPath $system) -or (Get-Item -LiteralPath $system).Length -ne 1048576){throw 'Need full 1 MiB TI84 Plus system'}
$output=Join-Path $root 'build-runtime\game-smoke'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$frontend=Join-Path $root 'build-host\frontend_smoke.exe'
foreach($relative in @('snake\snake.8xp','2048\2048.8xp')) {
    $program=Join-Path $root ('assets\games\scott-ti-games\'+$relative)
    $image=Join-Path $output ((Split-Path -Leaf $program)+'.ppm')
    $other=if($relative.StartsWith('snake')){'2048\2048.8xp'}else{'snake\snake.8xp'}
    $otherProgram=Join-Path $root ('assets\games\scott-ti-games\'+$other)
    & $frontend $system $program $image $otherProgram
    if($LASTEXITCODE){throw "Frontend runtime test failed: $program"}
    & (Join-Path $root 'build-host\game_smoke.exe') $system $program ($image+'.equivalent.ppm') --compare
    if($LASTEXITCODE){throw "Reference equivalence failed: $program"}
}
$basic=Join-Path $output 'basic-test.8xp'
& python (Join-Path $root 'tests\make_basic_fixture.py') $basic
if($LASTEXITCODE){throw 'BASIC fixture generation failed'}
& (Join-Path $root 'build-host\game_smoke.exe') $system $basic (Join-Path $output 'basic-test.ppm')
if($LASTEXITCODE){throw 'BASIC auto-launch failed'}
Write-Output "TI83 RUNTIME HOST: PASS; inspect PPM images in $output"
