# pico-286 for Retro_PicoES V2.1

S15 integrates the `xrip/pico-286` DOS/PC emulator with the RP2350B
Retro_PicoES V2.1 board (`HW_CONFIG=16`). The upstream emulator is kept as a
pinned external dependency. Board-specific code and patches live under this
directory so the other Pico_ES ports remain untouched.

## Current stage

- S15-A is complete: upstream, licensing, host build and RP2350 ELF link
  baselines are recorded.
- S15-B is active: private platform boundary, CPU/memory and timing contracts.
  The HW16 board profile is pinned and host-validated before peripheral wiring.
- HW16 PIT ticks now cross a bounded atomic mailbox: Core 1 publishes timing
  events and only the emulator loop on Core 0 mutates PIC/CPU interrupt state.
- The upstream emulator's five PSRAM-resident regions total 7,192,560 bytes,
  leaving 1,196,048 bytes of the 8 MiB device for bounded platform buffers.
- S15-D has started in parallel with a host-validated RGB565 scanline
  converter for indexed, packed CGA-style and 80x25 text sources. The upstream
  Core 1 renderer is now split into one-time initialization and a bounded
  realtime step; hardware HSTX ownership is not yet connected.
- Pinned upstream: `519a857001220a54ed0c9224be1d741a84b362a1`.
- Hardware builds, firmware images, DOS media, BIOS images, ROMs, and saves are
  not committed.

## Data policy

Runtime data will use `/data/pico286/`. Do not commit DOS distributions,
commercial games, disk images, firmware binaries, or user-generated saves.

## Host baseline

Run `scripts/Test-Pico286Host.ps1`. The script checks the pinned checkout and
directly builds/runs the upstream i82 controller test plus the private timing
and HW16 board-profile tests without starting the graphical emulator. Direct
compilation avoids the known Windows CMake ABI probe stall in this workspace.

Run `scripts/Bootstrap-Pico286.ps1` to obtain the pinned upstream and apply the
ordered patches idempotently. Re-running it reports each patch as `PRESENT`.
The patch stack may intentionally overlap; the bootstrap script peels it in
reverse order before restoring it in forward order on repeated runs.

For the current RP2350 candidate, configure with `RETROPICOES_HW16=ON` and set
`RETROPICOES_PORT_DIR` to this checkout's `pico286/platform/retropicoes`
directory. This
locks the system clock to 252 MHz, selects QMI PSRAM CS 47, UART0 GPIO2/3,
SPI1 SD GPIO30/31/40/43 and NES port 1 GPIO6/8/7. The private audio selection
keeps PC audio emulation active without driving the upstream PWM/I2S pins;
HSTX video and HDMI audio consumption are the next platform-adapter step.

After configuring, run `scripts/Test-Pico286Hw16Config.ps1` to verify the
generated definitions and the four board-sensitive RP2350 objects.
If Ninja stalls while spawning a long Windows compiler command, run
`scripts/Invoke-Pico286ObjectCompile.ps1 -BuildDirectory <build-dir>` to compile
`pico-main.c` through a GCC response file.
