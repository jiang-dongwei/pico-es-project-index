[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$testRoot=Join-Path $projectRoot ('build-install-tests\'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force $testRoot | Out-Null
$romRoot=Join-Path $testRoot 'roms\TI83'
$saveRoot=Join-Path $testRoot 'SAVES\TI83'
New-Item -ItemType Directory -Force $romRoot,$saveRoot | Out-Null
$rom=Join-Path $romRoot 'owned-test.83rom';$save=Join-Path $saveRoot 'user.sav'
[IO.File]::WriteAllBytes($rom,[byte[]](0x29,8,0))
[IO.File]::WriteAllBytes($save,[byte[]](1,2,3,4))
$romHash=(Get-FileHash $rom).Hash;$saveHash=(Get-FileHash $save).Hash
$install=Join-Path $PSScriptRoot 'install-ti83-bootloader-sd.ps1'
$uf2=Join-Path $projectRoot 'build-pico-retropicoes-v21-bootloader-make\ti83.uf2'
for($i=0;$i -lt 2;++$i){
    $output=@(& $install -SdRoot $testRoot -AllowDirectoryRootForTest -IncludeSampleGames)
    if($output -notcontains 'CONTENT INSTALL: PASS' -or $output -notcontains 'HARDWARE READINESS: BLOCKED'){throw 'Installer failed content/readiness distinction'}
}
if((Get-FileHash $rom).Hash -ne $romHash -or (Get-FileHash $save).Hash -ne $saveHash){throw 'Installer changed user data'}
foreach($relative in @('snake\snake.8xp','2048\2048.8xp')) {
    $source=Join-Path $projectRoot ('assets\games\scott-ti-games\'+$relative)
    $target=Join-Path $romRoot (Split-Path -Leaf $relative)
    if((Get-FileHash $source).Hash -ne (Get-FileHash $target).Hash){throw 'Game install mismatch'}
}
$editedGame=Join-Path $romRoot 'snake.8xp'
[IO.File]::WriteAllBytes($editedGame,[byte[]](7,8,9))
$editedHash=(Get-FileHash $editedGame).Hash
& $install -SdRoot $testRoot -AllowDirectoryRootForTest -IncludeSampleGames | Out-Null
if((Get-FileHash $editedGame).Hash -ne $editedHash){throw 'Existing edited game was overwritten'}
$index=Join-Path $testRoot 'emu\emulators.txt'
$rows=@(Get-Content $index | Where-Object {$_ -like 'ti83;*'})
if($rows.Count -ne 1 -or $rows[0] -ne 'ti83;ti83;TI-83/84'){throw 'Index is not idempotent'}
if(@(Get-ChildItem $testRoot -Recurse -Filter '*.ti83.tmp').Count){throw 'Temporary install files remained'}
$wrong=Join-Path $projectRoot 'build-pico-retropicoes-v21-make\ti83.uf2'
$indexHash=(Get-FileHash $index).Hash
$rejected=$false
try{& $install -SdRoot $testRoot -AllowDirectoryRootForTest -ApplicationUf2 $wrong | Out-Null}catch{$rejected=$true}
if(-not $rejected -or (Get-FileHash $index).Hash -ne $indexHash){throw 'Standalone image was accepted or index mutated'}
$full=Join-Path $testRoot 'full';New-Item -ItemType Directory -Force (Join-Path $full 'emu') | Out-Null
$fullIndex=Join-Path $full 'emu\emulators.txt'
[IO.File]::WriteAllLines($fullIndex,[string[]](1..32 | ForEach-Object {"app$_;app$_;App $_"}))
$fullHash=(Get-FileHash $fullIndex).Hash;$rejected=$false
try{& $install -SdRoot $full -AllowDirectoryRootForTest | Out-Null}catch{$rejected=$true}
if(-not $rejected -or (Get-FileHash $fullIndex).Hash -ne $fullHash){throw 'Full index was changed without explicit replacement'}
Write-Output 'TI83 INSTALLER TEST: PASS repeated install, data preservation, missing ROM, standalone rejection, full index rejection'
