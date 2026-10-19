# Included classic calculator games

Unmodified Snake and 2048 by Scott Mangiapane, from
https://github.com/scottmangiapane/ti-games at commit
`6c48442b2bf973734927166187287cac26571055`.
The upstream LICENSE is GPL-3.0. Original compiled programs, corresponding
Axe source programs/text and upstream README/LICENSE are included together.
These are the monochrome TI-83 Plus / TI-84 Plus versions, not CE games.

| Game | Program name | SHA-256 |
|---|---|---|
| Snake | SNAKE | e02c5721d4a09e6e7afbb2fbebe346dd73d8cb13008b016ac928232e62fbb55a |
| 2048 | PLAY2048 | 4ebf3c9c81e2895bf875d4b0791984debf1f67cd130e2e5b59bc165573b31d6e |

Use `install-ti83-bootloader-sd.ps1 -IncludeSampleGames` to install only the
compiled games into `/roms/TI83`, and the complete source/license subset into
`/licenses/TI83/scott-ti-games`. Existing different game bytes are preserved.
No TI system ROM is included in this bundle. The upstream author recommends
Axe Parser for the sources and `Asm(` to run the compiled programs; the port
automates that launch command via the emulated link protocol.
