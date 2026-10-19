[CmdletBinding()]
param([string]$Upstream = "")

$ErrorActionPreference = "Stop"
$expectedCommit = "519a857001220a54ed0c9224be1d741a84b362a1"

if ([string]::IsNullOrWhiteSpace($Upstream)) {
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")).Path
    $Upstream = Join-Path (Split-Path -Parent $repoRoot) "pico-286-upstream"
}
$Upstream = (Resolve-Path -LiteralPath $Upstream).Path

$actual = (& git -C $Upstream rev-parse HEAD).Trim()
if ($actual -ne $expectedCommit) {
    throw "pico-286 upstream mismatch: expected $expectedCommit, got $actual"
}

$checks = @(
    @{ Path = "LICENSE"; Pattern = "MIT License" },
    @{ Path = "src\emulator\audio\sound_blaster.c.inl"; Pattern = "version 2" },
    @{ Path = "src\emulator\fpu.c"; Pattern = "Copyright 2022 Justine" },
    @{ Path = "src\emu8950\emu8950.c"; Pattern = "SPDX-License-Identifier: MIT" },
    @{ Path = "drivers\fatfs\ff.c"; Pattern = "FatFs - Generic FAT Filesystem" },
    @{ Path = "drivers\fatfs\f_util.c"; Pattern = "Apache License, Version 2.0" },
    @{ Path = "drivers\sdcard\LICENSE"; Pattern = "Redistribution and use in source and binary forms" }
)

foreach ($check in $checks) {
    $path = Join-Path $Upstream $check.Path
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing license evidence: $($check.Path)" }
    if (-not (Select-String -LiteralPath $path -SimpleMatch $check.Pattern -Quiet)) {
        throw "License marker changed: $($check.Path) [$($check.Pattern)]"
    }
}

$forbidden = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot "..") -Recurse -File |
    Where-Object { $_.Extension -in ".img", ".rom", ".com", ".exe", ".uf2", ".sav" }
if ($forbidden) {
    throw "Forbidden runtime/generated files found under pico286: $($forbidden.FullName -join ', ')"
}

Write-Host "PICO286 LICENSE GATE: PASS commit=$actual components=$($checks.Count)"
