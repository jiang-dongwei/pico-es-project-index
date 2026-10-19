# RetroPicoES compat v1

This directory is the canonical local copy of the platform-neutral interface
first developed in the SDLPoP port and then reused unchanged by xrick and
Digger Remastered.

## Scope

- `compat.h` defines the 320x240 RGB565 framebuffer, normalized input, audio
  callback, timer, memory and filesystem boundaries.
- `audio.c/.h` provide a lock-free single-producer/single-consumer PCM ring.
- `video.c/.h` provide platform-neutral surfaces and RGB565 software drawing.
- Each game or emulator owns its private adapter and the HW_CONFIG=16 backend.
- This module does not include or depend on `pico_shared`, FatFS, TinyUSB or a
  game/emulator core.

The historical `SDLPoP_` include guards and `SDLPoP_PLATFORM_PICO` build macro
are intentionally frozen in v1. Renaming them would make the fourth adopter
different from the three validated copies.

## Version and adoption

Version `1.0.0` is represented by the exact hashes in `manifest.sha256`.
Run `scripts/Test-CompatV1.ps1` from this directory to check the canonical copy
and the SDLPoP, xrick and Digger adopters. New ports should consume this folder
directly through `RetroPicoES::compat_v1` and implement the platform functions
declared in `compat.h` in their own private adapter.

This is an independent local module. Creating or publishing a remote repository
requires separate authorization.

## License

The compat v1 sources are distributed under GNU GPL version 3 or later, matching
the SDLPoP port in which they were developed. See `COPYING`. Emulator cores and
board dependencies retain their own licenses and are not relicensed by this
module.
