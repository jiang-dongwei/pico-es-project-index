[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$DataRoot)
$ErrorActionPreference='Stop'
$romRoot=Join-Path ([IO.Path]::GetFullPath($DataRoot)) 'roms\TI83'
$valid=@()
if(Test-Path -LiteralPath $romRoot){
    foreach($file in Get-ChildItem -LiteralPath $romRoot -File -Recurse){
        $expected=switch($file.Extension.ToLowerInvariant()){
            '.83rom' {262144} '.83prom' {524288} '.84prom' {1048576}
            '.rom' {if($file.Length -in @(262144,524288,1048576)){$file.Length}else{0}}
            default {0}
        }
        if($expected -eq 0 -or $file.Length -ne $expected){continue}
        $stream=[IO.File]::OpenRead($file.FullName)
        try{
            $boot=if($file.Length -eq 262144){0}else{$file.Length-16384}
            $null=$stream.Seek($boot,[IO.SeekOrigin]::Begin)
            $first=$stream.ReadByte()
            if($first -ge 0 -and $first -ne 255){$valid+=$file.FullName}
        }finally{$stream.Dispose()}
    }
}
Write-Output "TI83 ROM candidates: $($valid.Count)"
$defaultSystem=Join-Path $romRoot 'ti84plus.84prom'
$games=@(if(Test-Path -LiteralPath $romRoot){Get-ChildItem -LiteralPath $romRoot -File -Recurse -Filter '*.8xp'})
Write-Output "TI83 game files: $($games.Count)"
if($valid -contains $defaultSystem -and $games.Count){
    Write-Output 'HARDWARE READINESS: CANDIDATE (full OS boot not verified)'
    $valid | ForEach-Object {Write-Output "ROM: $_"}
}else{
    Write-Output 'HARDWARE READINESS: BLOCKED'
    Write-Output 'Need /roms/TI83/ti84plus.84prom (full 1M system) and classic .8xp games; empty browser still offers simulated demo.'
}
