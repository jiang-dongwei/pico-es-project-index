[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Path,[switch]$Bootloader)
$ErrorActionPreference='Stop'
$bytes=[IO.File]::ReadAllBytes([IO.Path]::GetFullPath($Path))
if($bytes.Length -eq 0 -or $bytes.Length % 512 -ne 0){throw 'Invalid UF2 block length'}
$start=if($Bootloader){[uint32]0x10080000}else{[uint32]0x10000000}
$minimum=[uint32]::MaxValue;$blocks=$bytes.Length/512
$secureIndex=0;$secureCount=0
for($i=0;$i -lt $blocks;++$i){
    $offset=$i*512
    if([BitConverter]::ToUInt32($bytes,$offset) -ne 0x0a324655 -or
       [BitConverter]::ToUInt32($bytes,$offset+4) -ne ([Convert]::ToUInt32('9e5d5157',16)) -or
       [BitConverter]::ToUInt32($bytes,$offset+508) -ne 0x0ab16f30){throw 'Invalid UF2 magic'}
    $flags=[BitConverter]::ToUInt32($bytes,$offset+8)
    $address=[BitConverter]::ToUInt32($bytes,$offset+12)
    $size=[BitConverter]::ToUInt32($bytes,$offset+16)
    if($size -gt 476 -or $size -eq 0){throw 'Invalid UF2 payload'}
    $family=[BitConverter]::ToUInt32($bytes,$offset+28)
    if($family -eq [Convert]::ToUInt32('e48bff57',16)){
        if(($flags -band 0x8000) -eq 0 -or $address -ne 0x10ffff00 -or $size -ne 256){throw 'Unexpected absolute metadata block'}
        continue
    }
    if(($flags -band 0x2000) -eq 0 -or [BitConverter]::ToUInt32($bytes,$offset+28) -ne ([Convert]::ToUInt32('e48bff59',16))){throw 'UF2 is not RP2350 ARM Secure'}
    $count=[BitConverter]::ToUInt32($bytes,$offset+24)
    if($secureIndex -eq 0){$secureCount=$count}
    if([BitConverter]::ToUInt32($bytes,$offset+20) -ne $secureIndex -or $count -ne $secureCount){throw 'Invalid UF2 block order/count'}
    ++$secureIndex
    if($address -lt $start -or [uint64]$address+$size -gt 0x11000000){throw 'UF2 crosses application flash partition'}
    if($address -lt $minimum){$minimum=$address}
}
if($secureIndex -ne $secureCount -or $secureCount -eq 0){throw 'Incomplete secure UF2 stream'}
if($minimum -ne $start){throw 'Wrong UF2 start address'}
Write-Output ('UF2 VERIFY: PASS start=0x{0:X8} blocks={1}' -f $minimum,$blocks)