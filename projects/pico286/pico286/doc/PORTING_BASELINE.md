# S15-A porting baseline

## Pinned source

- Repository: `https://github.com/xrip/pico-286.git`
- Commit: `519a857001220a54ed0c9224be1d741a84b362a1`
- Commit date: 2026-08-21
- Submodules: none
- Root license: MIT, with additional component licenses listed in
  `THIRD_PARTY_LICENSES.md`.

## Reusable emulator modules

- `src/emulator/cpu.c`, `memory.c`, `ports.c`, `i8253.c`, `i8259.c` and
  `i8237.h`: x86 CPU and PC controller model.
- `src/emulator/video/`: CGA/TGA/VGA register and memory model.
- `src/emulator/audio/` and `src/emu8950/`: PC audio devices and OPL synthesis.
- `src/emulator/disks-rp2350.c.inl`: disk semantics are reusable after FatFS
  calls and paths move behind the RetroPicoES storage adapter.

## Drivers that must not be used directly

- `src/pico-main.c`: owns MURMULATOR clocks, QMI setup, pins and the upstream
  Core 1 loop.
- `drivers/hdmi`, `drivers/vga-nextgen`, `drivers/graphics`: replaced by the
  HW_CONFIG=16 HSTX scanout adapter.
- `drivers/audio`: physical I2S/PWM output replaced by HSTX HDMI audio packets.
- `drivers/sdcard` and bundled FatFs: replaced by the already validated
  pico_shared HW16 SD/FatFS stack.
- `drivers/ps2`, `drivers/nespad`: replaced by dual PIO-USB keyboard/gamepad
  input; existing NES ports may be added after the USB baseline.
- `drivers/psram`: generic SPI PSRAM replaced by RP2350B QMI PSRAM on GPIO47.

## Required seams

The upstream core currently calls `graphics_set_palette`, `read*psram`,
`write*psram`, FatFS functions and platform timing directly from emulator
translation units. S15-B must introduce private compile-time hooks for these
services. It must not change Pico_ES `compat/` or another port's `pico_shared`.

## Verified baseline

- Upstream Windows Release host build: PASS; executable
  `bin/host/Release/286.exe`, 7,462,512 bytes.
- Strict direct build of `tests/i82_controllers_test.c`: PASS with GCC 14.2,
  `-Wall -Wextra -Werror -Wconversion -Wshadow`.
- RP2350 ARM Secure configuration: PASS with Pico SDK 2.3.0, the shared
  `pico2b` board header and `PICO_NO_PICOTOOL=1` for the ELF-only baseline.
- All RP2350 objects compiled and the CMake-generated final ELF link passed
  after preserving nine `pico_double` wrapper symbols from GCC LTO. Reported
  usage was FLASH 166,344/2,097,152 bytes, RAM 515,444/524,288 bytes, scratch X
  2,048/4,096 bytes, scratch Y 3,904/4,096 bytes and PSRAM
  7,192,560/8,388,608 bytes.
- `0001-respect-explicit-pico-sdk.patch` stops the VS Code SDK file from
  overriding an explicit SDK and permits `-DPICO_SDK_PATH=...` without an
  environment variable. `0002-preserve-rp2350-double-wrappers-with-lto.patch`
  fixes the RP2350 LTO link failure.
- On this Windows runner, Pico SDK's always-built nested `pioasm` install target
  stalls after producing a working `pioasm.exe`. The compile completed and the
  exact generated link was executed directly for the result above; a clean
  end-to-end CMake/UF2 build remains a later delivery check.
- The graphical host executable was not used as an acceptance test because it
  requires runtime disk/UI interaction; starting it without data was stopped.
- The workspace's known CMake ABI-probe stall reproduced for the small wrapper,
  so the committed Windows test script intentionally uses direct GCC.
