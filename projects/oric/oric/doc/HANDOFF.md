# Oric-1 / Atmos candidate handoff — 2026-09-22

**Status: candidate ready; user hardware acceptance pending. No task/bug PASS.**

Worktree: `E:/ComporyProject/Pico_ES-oric`, branch `codex/oric1-atmos`.
Base: `7330ed6`; initial audit: `4bade86`. Main/shared/other emulator files were
not edited. Ownership is limited to `oric/**` and the Oric discussion/checkpoint.

## Implemented

- Independent 48K Oric-1/Atmos machine around pinned zlib chips; Oricutron
  reference only. CPU decimal mode, RAM/ROM map, VIA keyboard/cassette,
  AY-3-8912 and raster-line ULA. Standard TAP pulse playback, no BIOS traps.
- Public v2 ROM browser + Settings host + frontend_shell linked and invoked.
  Empty simulated test, directory/page navigation, physical A/B/Menu,
  pause/resume/reset/quit/Loader and input release gates.
- Actual mute, scanlines, FPS, machine, direction layout and virtual key/send;
  independent versioned settings file with SAVE/CANCEL/DEFAULT.
- Two original BIOS-free test TAPs and original Loader cover. User BIOS path
  and missing/invalid ROM errors are explicit. No proprietary data in delivery.

## Verification commands and results

Run from `E:/ComporyProject/Pico_ES-oric`, with selected MinGW bin prepended to
PATH as in `tools/build_pico.ps1`.

| Command | Result |
| --- | --- |
| `cmake -S oric -B oric/_build/host -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release` | configured |
| `cmake --build oric/_build/host --parallel 4` | built, own host code strict warnings/errors |
| `ctest --test-dir oric/_build/host --output-on-failure` | 3/3 tests succeeded |
| `oric/_build/host/oric_rom_probe.exe <basic10/basic11b.rom> <Controls/Hires.tap> <output.ppm>` | all 4 combinations exited 0; zero loaded-byte mismatches; original guest entered |
| `./oric/tools/build_pico.ps1 -Image loader` | HW16 pico2b ARM Secure Release built |
| `./oric/tools/build_pico.ps1 -Image standalone` | HW16 pico2b ARM Secure Release built |
| `arm-none-eabi-nm -S oric/_build/pico-loader/oric.elf` | browser/Settings host/shell symbols present; CPU/VIA/AY tick symbols in SRAM |
| `picotool info -a oric/_build/pico-standalone/oric.uf2` | RP2350 ARM Secure, pico2b, SDK 2.3.0, origin 0x10000000 |
| `python oric/tools/package_candidate.py` | both UF2 block/family/range checks and local packaging succeeded |
| regenerate test header and compare to `core/demo_tape.h` | byte-identical |
| compare CPU/AY/LICENSE to pinned chips clone | byte-identical; VIA local patch documented |

Tests cover CPU/reset/ROM protection/decimal, ULA bounds and attributes,
truncated tapes, two guest programs, VIA/AY keyboard/audio, one-shot timer,
long audio rate, Menu-vs-Start/Back, public empty demo/lifecycle/home gate,
and the real target adapter under mocked I/O. Target tests verify that failed
Settings writes leave B usable, settings round-trip/checksum fallback,
audio stop-before-reset/free and relaunch. These are **host results**.

BIOS test logs: `oric/_build/probe-basic10-Controls.log`,
`probe-basic10-Hires.log`, `probe-basic11b-Controls.log`,
`probe-basic11b-Hires.log`. Build logs: `oric/_build/host-build.log`,
`pico-loader-build.log`, `pico-standalone-build.log`.

## Firmware and resources

Package: `E:/ComporyProject/Pico_ES-oric/oric/_build/delivery/oric-candidate1/`.

| Image | ARM payload range | Bytes | SHA-256 |
| --- | --- | --- | --- |
| `oric-loader.uf2` | `0x10080000..0x100B7500` exclusive | 453632 | `1508067219b93c7ce4acf1d0bef0c27e5b88c88b589160769d2505a988423323` |
| `oric-standalone.uf2` | `0x10000000..0x10037500` exclusive | 453632 | `a7c4364aa2d8b806284d2480d961c15a87446e70f76ed41adda487434f8172cc` |

Each also contains the SDK's separate absolute-family compatibility block at
`0x10FFFF00`; it is not ARM executable payload. `picotool info` cannot identify
this relocated Loader executable, matching its base-address limitation; this
is recorded rather than treated as a successful inspection. ELF segment and
UF2 payload validation establish the relocation, pending actual Loader boot.

Final Loader RAM regions: vector 272 B, `.data` 53208 B (includes hot code),
`.bss` 411652 B, minimum heap reservation 2048 B = 467180/524288 B. Remaining
above that reservation: 57108 B. Both scratch banks reserve 4096-byte stacks.
Machine state is 68960 B; double scanout is 307200 B. Runtime PSRAM uses
53760 B indexed video, 16384 B BIOS staging, 12416 B directory entries and
up to 1 MiB tape, plus shared runtime allocations. Link fit does not prove
stack margin or real-time performance.

## Remaining risks / main-window coordination

1. **No hardware test or SD deployment occurred.** Use the single checklist
   `HVR-ORIC-001.md`; leave acceptance pending until the user confirms.
2. ULA is line-granular; IRQ/tape details and the local one-shot VIA variant
   need compatibility validation. Four original-tape/BASIC cases do not prove
   commercial game coverage. No disks, expansions, WAV/raw tapes, state files
   or VIA shift register support. NTSC and larger multi-block programs untested.
3. Real throughput, HSTX, subjective AY sound, controller hotplug/held Home,
   SRAM/stack margin, and Settings SD power-loss behavior remain unverified.
   A torn config file falls back to defaults; it is not an atomic journal.
4. Main integrates only the proposed `oric;oric;Oric-1 / Atmos` index entry,
   application/cover and original tapes after choosing the actual SD target.
   BIOS must come from the user. Do not overwrite the Loader itself.
5. **No public API extension is needed.** Existing main dependency patches
   are required: PIO-USB PIO2 selection and XInput handler, pinned by file hash
   in `board-dependencies.json`. The wrapper checks them read-only. A clean
   upstream-only rebuild was not claimed; main owns distribution of these
   shared patches. Shared return logging still says “Start hold”; this Oric
   target disables that shortcut and invokes return only from Settings.

## Changed files

All files are new except `oric/doc/PORT_AUDIT.md`, which appends the audit's
implementation decision. No accepted historical conclusions were changed.

- `oric/.gitignore`, `oric/CMakeLists.txt`, `oric/core-manifest.json`, `oric/README.md`
- `oric/cmake/frontend_contract.cmake`, `oric/pico/CMakeLists.txt`
- `oric/core/oric.h`, `oric/core/oric.c`, `oric/core/demo_tape.h`
- `oric/platform/retropicoes/adapter.h`, `adapter.c`, `main.cpp`
- `oric/third_party/chips/m6502.h`, `m6522.h`, `ay38910.h`, `LICENSE`
- `oric/tools/make_test_tapes.py`, `rom_probe.c`, `build_pico.ps1`, `package_candidate.py`
- `oric/tests/test_oric.c`, `test_frontend.c`, `test_target.cpp`
- `oric/tests/stubs/pico/stdlib.h`, `oric/tests/stubs/hardware/uart.h`
- `oric/assets/loader/oric.png`
- `oric/doc/PORT_AUDIT.md`, `SOURCES.md`, `board-dependencies.json`, `HANDOFF.md`
- `.emv2/discussion/2026-09-22-oric-atmos-port/requirements.md`
- `.emv2/checkpoints/HVR-ORIC-001.md`
