# Third-party license inventory

This inventory applies to the pinned upstream commit
`519a857001220a54ed0c9224be1d741a84b362a1`. It intentionally does not treat
the repository-level MIT file as a replacement for component notices.

| Component | Upstream path | License | Notes |
|---|---|---|---|
| pico-286 original code | repository except components below | MIT | Copyright (c) 2025, xrip |
| XTulator Sound Blaster 2.0 code | `src/emulator/audio/sound_blaster.c.inl` | GPL-2.0-or-later | Copyright (C) 2020 Mike Chambers; makes the combined firmware GPL-compatible rather than MIT-only |
| Blink-derived FPU | `src/emulator/fpu.c` | ISC-style permissive license | Copyright 2022 Justine Alexandra Roberts Tunney |
| emu8950 | `src/emu8950/` | MIT | Copyright 2001-2020 Mitsutaka Okazaki and 2021-2022 Graham Sanderson |
| FatFs R0.14b | `drivers/fatfs/ff.*`, related FatFs files | FatFs permissive license | Copyright (C) 2021, ChaN |
| FatFs utility glue | `drivers/fatfs/f_util.*` | Apache-2.0 | Copyright 2021 Carl John Kugler III |
| pico SD bindings | `drivers/sdcard/` | BSD-2-Clause | Copyright 2020-2021, Elehobica; individual Raspberry Pi PIO files retain BSD-3-Clause headers |
| audio driver | `drivers/audio/` | MIT | Copyright (c) 2022 Vincent Mistler |
| generic SPI PSRAM driver | `drivers/psram/` | MIT | Copyright 2023 Ian Scott; not used for the HW_CONFIG=16 QMI path |
| printf implementation | `src/printf/` | MIT | Copyright Marco Paland |

## Distribution requirements

- Preserve every upstream copyright and license header in patched or copied
  source.
- Ship the Pico_ES GPL-3.0 notice together with the GPL-2.0-or-later Sound
  Blaster notice and all permissive component notices.
- Do not distribute DOS, commercial game data, disk images, user saves, or
  unverified third-party BIOS files with the source or firmware.
- The embedded `src/emulator/includes/bios.h` first appears in pico-286 history
  and is treated as part of the pinned upstream snapshot; provenance must be
  rechecked before redistributing it separately from the GPL firmware.
