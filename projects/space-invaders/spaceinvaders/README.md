# RetroPicoES Space Invaders

This directory contains the S12 single-board Space Invaders port and its host
tests. It does not contain game ROMs, diagnostic ROMs, save files, or external
sound samples.

The first implementation stage provides:

- an unmodified, pinned Intel 8080 CPU core;
- a project-owned bus wrapper with no SDL or Pico SDK dependency;
- a deliberately small arcade scheduling contract;
- self-generated host tests for arithmetic, flags, cycles, memory, I/O,
  `EI` delay, `HLT`, interrupts, and fractional cycle scheduling.

The RP2350B/HW_CONFIG=16 firmware integration is available in `pico/`.
Required ROM files are read from `/roms/SPACEINVADERS/`.

The board layer now validates four 2 KiB chips in `h/g/f/e` order, maps ROM,
RAM and its read mirror, models the MB14241-style shifter and sound-port edges,
and schedules alternating `RST 1`/`RST 2` requests at 120 Hz from the exact
1.9968 MHz CPU clock. The ROM gate also produces deterministic absolute paths
and display-ready missing/size error text. File I/O is intentionally deferred
to the platform layer.

The host-tested frontend additionally provides:

- 224x256 1bpp rotation and exact 15:16 scaling to a centered 210x240 image;
- selectable monochrome or classic red/green overlay output in RGB565;
- normalized two-player controls: Xbox Back inserts a coin, Start starts on a
  fresh press, A/B fire, and the D-pad moves; holding Start has no hidden menu
  or Loader action;
- synthesized UFO, shot, explosion, hit and four fleet-step sounds at 22050 Hz;
- a portable, versioned, ROM-bound and CRC-protected state payload at
  `/SAVES/spaceinvaders/state.bin`;
- one-second CRLF-terminated performance records and a two-SRAM-frame `READY`
  ownership handoff for Core 0/Core 1 integration.

## Host test

```powershell
cmake -S spaceinvaders -B spaceinvaders/build-host
cmake --build spaceinvaders/build-host
ctest --test-dir spaceinvaders/build-host --output-on-failure
```

See `doc/RETROPICOES_V21_LICENSES.md` and `doc/UPSTREAM.md` before changing
third-party code.

The repeatable S12-A gate is:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  spaceinvaders/scripts/Test-S12ASourceBaseline.ps1
```

The HW16 firmware can be built at 252 MHz in both address layouts without
adding ROM data:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  spaceinvaders/scripts/build-retropicoes-v21.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File `
  spaceinvaders/scripts/build-retropicoes-v21.ps1 -Bootloader
```

The standalone image starts at `0x10000000`; the Loader application starts at
`0x10080000`. The firmware shows and logs each ROM/core/audio/first-frame load
stage, uses USB1/NES1 for P1 and USB2/NES2 for P2, streams synthesized audio on
Core 1, saves a ROM-bound state before Loader return, and emits one CRLF UART
performance report per second without blocking the emulation loop.

The shared ROM browser appears before launch and lists the four-chip Space
Invaders ROM set. In play, Xbox Back inserts a coin, Start begins play, and
A/B fire. Xbox Guide/Home opens the common Settings page and pauses the core.
The page offers Resume, Save state, Load state, Reset game, Quit game (save
and return to the ROM browser), and Return to Loader (save first). A confirms
and B returns inside menus. Long-press Start no longer exits. Selecting the
game starts cold by default, avoiding an old Game Over state; Load state is
an explicit Settings action. Hardware acceptance is pending.

## Loader SD install

The project-owned Loader image, unique menu row and original 320x240 cover can
be installed without touching ROMs, saves or unrelated applications:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  spaceinvaders/scripts/install-spaceinvaders-bootloader-sd.ps1 `
  -SdRoot H:\
```

If `emulators.txt` already contains 16 applications, pass
`-ReplaceProgramName name` explicitly; the installer never chooses one to
delete. `S12 CONTENT INSTALL: PASS` validates project files, while
`S12 HARDWARE READINESS` is `READY` only when `invaders.h`, `invaders.g`,
`invaders.f`, and `invaders.e` are all exactly 2048 bytes. The isolated,
repeatable installer test is `scripts/Test-S12ESdInstaller.ps1`.

## Hardware follow-up (2026-09-21)

User-provided UART logs from the optimized video build show 60.0 and 59.9 FPS
against the 60 FPS target. Both samples report zero audio underflow, audio
drop, video drop, resync, and UART drop. The user also confirmed that the score
text displays completely. Project status: **menu pending improvement**;
remaining controls, sound, save/load, and Loader-return checks need separate
hardware confirmation.
