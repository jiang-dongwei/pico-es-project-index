# Oric-1 / Atmos port audit (2026-09-22)

Initial status at audit commit `4bade86`: source audit only.
Continuation: independent core and frontend implemented; candidate verification
is recorded in `HANDOFF.md`. Hardware acceptance is still pending.
This work is isolated on `codex/oric1-atmos`.

## Upstream and license

- Task authority: `tasks-kian.md` assigns Oric-1/Atmos and identifies Oricutron pieces.
- Audited upstream: https://github.com/pete-gordon/oricutron at commit
  `002279fce9fa756d1d63cdc40ae97939eb7de7ed` (local audit clone at
  `_deps/oricutron-audit`, excluded from delivery).
- `LICENSE.txt` and the examined core headers identify GPL version 2. The
  project is GPL-3.0, so copied files need a file-level compatibility review
  before import; do not silently relicense them. Preserve copyright notices
  and publish source for any distributed binary.
- Oricutron's included demos have separate author permissions stated in its
  ReadMe. No demos, commercial games, tapes or disk images are selected for
  this port. The included `roms/` has patches, symbols and Orix firmware, but
  no `basic10.rom` or `basic11b.rom`. Do not infer rights to redistribute an
  Oric BASIC ROM from the emulator source license.

## Resource and runtime boundary

Target: RP2350B, HW_CONFIG=16, 520 KiB SRAM, 8 MiB PSRAM, 320x240 RGB565
HSTX. Oric RAM is 48 KiB and BASIC ROM is 16 KiB. Budget additional state for
6502, VIA, AY-3-8912, ULA, keyboard, one decoded tape, framebuffer and the
shared HSTX/audio backend; measure actual linker SRAM usage before claiming fit.

The smallest useful scope is Oric-1 and Atmos, 48 KiB, PAL video, `.tap`
selection, keyboard/joystick mapping, AY audio, and a user supplied 16 KiB
system ROM per model. Disk `.dsk`, Microdisc/Jasmin, Telestrat, Pravetz,
snapshots, `.wav` tapes, debugger and serial peripherals are later work.
Whether `.tap` auto-loading can be made reliable without a keyboard text
entry facility remains unverified. A BIOS-only boot screen does not satisfy
game playback.

Oricutron's `machine.c` directly calls SDL audio and input functions, while
`machine.h` embeds SDL types and disk/serial/expansion state. `main.c` owns the
desktop loop. The port must isolate 6502, VIA, AY, ULA and tape logic behind
a deterministic frame API, then bind board video/audio/input/files in a
private adapter. No shared frontend or compat ABI change is authorized.

## Mandatory frontend design

Register `/roms/ORIC/` `.tap` entries with `common/frontend/rom_browser`.
Its empty state provides the labelled `DEMO / TEST GAME (SIMULATED)` entry.
Use `settings_host` for both browser and running Settings and `frontend_shell`
for lifecycle. Physical A opens, B returns, Menu/Home enters Settings. Keep
Oric keyboard/game mappings and machine choice in the adapter. No private
browser/settings loop, Select+Start, or long Start menu path.

Capability matrix for initial scope:

| Capability | Initial scope |
| --- | --- |
| ROM browser | Shared browser, `.tap`, directories and simulated empty item |
| Running Menu | Physical Menu/Home through shared shell |
| Quit game | Stop core and return to shared browser |
| Reset | Cold reset with the same user supplied system ROM |
| Save/load state | Hidden until implemented and tested |
| Video setting | Hidden until an actual adapter option exists |
| Audio setting | AY mute may be shown after implementation |
| Core options | Oric-1/Atmos machine choice and keyboard map, if functional |
| Persistence | No save claim until SD write/read is tested |

## Gates and coordination

1. Decide and document a distributable system-ROM basis, or require users to
   provide `basic10.rom`/`basic11b.rom` on SD. Verify exact expected sizes and
   signatures in host tests. No ROM is bundled by this audit.
2. Extract the minimal core with deterministic host tests for reset, memory
   map, VIA/ULA/AY timing, TAP parsing and malformed input.
3. Integrate the existing frontend modules with a private Oric data source,
   run common frontend tests, then build standalone and Loader HW_CONFIG=16.
4. Produce a candidate UF2 and a single concentrated user hardware check:
   populated and empty browser, both Settings entries, picture, controller,
   audio, return and UART timing. Only the user can accept it.

No specific shared-frontend API gap is proven yet. If integration reveals one,
send the exact API need to the main window before any shared-layer change.

## Continuation decision

The implemented route uses zlib `floooh/chips` for the CPU/VIA/AY, with an
independently written Oric machine adapter. Oricutron remains documentation
only; its GPL-2-only code was not imported. Full pins, modifications and data
rights are in `SOURCES.md`. BIOS is user supplied; two original executable
TAPs provide a distributable baseline. The private capability matrix in the
discussion record now has working implementations and host coverage.

Hardware mapping is read from tracked HW_CONFIG=16 BoardConfigs: RP2350B,
252MHz default, HSTX126MHz, SD SPI1 (CS43/SCK30/MOSI31/MISO40), USB PIO2
(DP28/26), NES PIO1 (6/7/8 and 9/10/11), PSRAM CS47 and UART0 GPIO2/3.
Hot chip cycles and Oric RAM are in SRAM; indexed video, BIOS staging, directory
entries and TAP storage use PSRAM. Core1 stays owned by the existing audio/HSTX
backend. No board pin or shared dependency source was edited.
