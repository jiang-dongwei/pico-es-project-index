# Oric-1 / Atmos — HW16 candidate 2

Status: **candidate, awaiting user hardware acceptance**. New 48K Oric machine
adapter with 6502, VIA, AY-3-8912, text/HIRES and standard TAP playback. Source
and data provenance: [doc/SOURCES.md](doc/SOURCES.md).

## SD layout (main window integrates)

| Candidate file | SD destination |
| --- | --- |
| `oric-loader.uf2` | `/emu/16/oric.uf2` |
| `oric.png` | `/emu/assets/themes/0/oric.png` |
| original `roms/ORIC/*.tap` | `/roms/ORIC/` |

Proposed `/emu/emulators.txt` entry: `oric;oric;Oric-1 / Atmos`.
The package does not overwrite the shared index or write a physical SD card.
`oric-standalone.uf2` starts at 0x10000000 and is for direct BOOTSEL testing;
the Loader application starts at 0x10080000. Do not interchange them.

For other tapes supply the selected model's 16384-byte system ROM:
`/roms/ORIC/basic11b.rom` (Atmos default) or `/roms/ORIC/basic10.rom` (Oric-1).
No BASIC ROM is distributed. The two exact original test TAPs also run without
BASIC. An empty directory offers the clearly labelled **SIMULATED TEST** item.
`PicoES_Oric_Audio.tap` is a separate original sound diagnostic. It plays
C–D–E–F–G–A–B–C tones for half a second each, then half a second of silence,
and repeats. The screen shows the current note. It uses the selected model's
BASIC ROM and the single-block autorun fastload path.
Single-block autorun machine-code TAPs load directly after BASIC initializes.
Other standard tapes type `CLOAD""` through the keyboard matrix and take
cassette time, typically tens of seconds. Non-autorun BASIC tapes
can be started by sending R, U, N and RETURN through Settings → Virtual key.

## Controls and Settings

- Browser: Up/Down select, Left/Right page, physical A opens, B goes up.
  B at the root opens Settings. Physical Menu/Home opens Settings everywhere.
- Game: arrows (or QAOP), A = SPACE, B = CTRL, Start = RETURN, Back = ESC.
  No Select+Start combination and no long-Start menu shortcut.
- Settings: working copy with SAVE/CANCEL/DEFAULT; Audio, Scanlines, FPS,
  model, direction keys, virtual key/send. Model applies on launch/reset.
  Quit, Reset and Return Loader require a second A on the same item.
  Send key resumes with a five-frame keypress; ordinary option edits still
  require SAVE. B or Home cancels edits and resumes the previous screen.
- Configuration: `/SAVES/ORIC/settings.dat`, version 1, bounded values/checksum.
  An interrupted write falls back to defaults next boot. No save states.

## Build and test

From the worktree root, with CMake, Python, ARM GCC and MinGW on PATH:

```powershell
cmake -S oric -B oric/_build/host -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release
cmake --build oric/_build/host --parallel 4
ctest --test-dir oric/_build/host --output-on-failure
./oric/tools/build_pico.ps1 -Image loader
./oric/tools/build_pico.ps1 -Image standalone
python oric/tools/package_candidate.py
```

The build wrapper uses `build-cmake` skill and defaults to existing dependencies
under `E:\ComporyProject\Pico_ES\_deps`; override `-DependenciesRoot` and
`-MinGWBin` as needed. Build output stays under `oric/_build/`.
Optional local ROM test (BIOS excluded from repository and package):

```powershell
oric/_build/host/oric_rom_probe.exe basic11b.rom oric/_build/host/test-tapes/PicoES_Oric_Controls.tap output.ppm
```

## Candidate 2 performance change

PAL target is 1,000,000 / 19,968 = 50.08 FPS. Candidate 1 HIRES hardware
logs showed 16 FPS (about 49 ms step + 12.6 ms draw) and increasing audio
underflow. Candidate 2 eliminates indexed PSRAM video staging: the ULA sends
each 240-byte SRAM line directly to the claimed inactive RGB565 SRAM frame.
Host tests compare buffered/streaming pixels, guest state and PCM exactly.
`step` now includes RGB565 line conversion; `draw` covers surface acquisition,
borders and overlay. Hardware speedup is pending measurement. User reported
Settings interactions normal on candidate 1; no new UI behavior is intended.

## Limits and required hardware check

- No disk/expansion, WAV/raw pulse tape, snapshot, printer, serial or VIA
  shift-register implementation. TAP limits: 1 MiB, standard BASIC/machine-code
  blocks, names up to 16 bytes. Browser: 128 entries/directory, path 255 bytes,
  16 levels. Larger collections should use subdirectories.
- ULA is raster-line timed, not individual-character cycle exact. PAL and
  NTSC raster rates are implemented; only the original PAL programs were tested.
- AY uses 25 kHz internal samples rationally resampled to 22050 Hz. Subjective
  sound, timing margin, physical controllers and HSTX stability await hardware.
- UART emits nonblocking PERF about every 2 seconds and heartbeat every 10
  seconds. A shared backend return message still says “Start hold” although
  this port invokes it only through confirmed Settings; no shared file changed.
- `picotool info` recognizes the standalone image; this tool version does not
  recognize relocated Loader images. Package checks validate ARM-family UF2
  addresses/blocks; ELF symbols and linker map validate the relocated code.

One concentrated acceptance: empty and populated browser; both original TAPs;
real user tape with each BIOS; physical A/B/Menu and held-key release; Settings
SAVE/reboot/CANCEL/DEFAULT; mute/scanlines/FPS; repeated pause/reset/quit/relaunch;
Loader return; picture/audio and UART performance. Build success is not acceptance.
