# TI83 parallel task handoff

Date: 2026-10-07. Branch: `codex/ti83-ti84`; baseline: `7330ed6`.
Worktree: `E:/ComporyProject/pico_ES_Public_Projects/Pico_ES_TI83_TI84`.
Handoff unit: this branch's committed TI83 implementation (retrieve with
`git log -1 --oneline`). No merge into main has been performed.
Only `ti83/**` and TI83-specific discussion/checkpoint files are changed.
The main workspace's task registration was appended before worktree creation;
its pre-existing modifications are preserved.

## Delivered implementation

Unmodified, pinned TilEm 2.0 CPU/calculator hardware core, private allocator
(large memory to PSRAM), exact-sized full ROM loading and read-failure cleanup.
TI-83 / TI-83 Plus / TI-84 Plus, 96x64 LCD at 3x or 2x, all 49 keys, physical
A/B normalization and 2nd/Alpha locks. UI uses the shared ROM browser,
Settings host and frontend lifecycle with real settings persistence, simulated
empty-state demo, reset confirmation, safe pause and Loader return callbacks.
No shared code or board initialization was modified.

Original cover, idempotent SD installer, runtime-data check, source/hash
contract check, host tests and both HW16 build scripts are included.

## Automatic verification

* Core host tests: model/size rejection, exact Z80 program execution on all
  three models, keyboard matrix and edges, held ON without repeated IRQ,
  virtual keyboard wrapping, guarded LCD edges/scaling/scanlines, allocation.
* Production frontend host tests: missing/short/truncated ROM cleanup, three
  models and simulated demo, duplicate start rejection without destroying the
  session, Menu at frame boundary, pause/resume and idempotent stop. CTest 2/2.
  This test caught and fixed the initial demo's incorrect hardcoded LCD stride.
* Shared frontend: ROM browser, lifecycle and Settings host tests passed;
  existing adoption gate passed. External PCE/O2 checks were skipped because
  those upstream trees are not bootstrapped in this task worktree.
* Local full TI ROM smoke: TI-83, TI-83 Plus and TI-84 Plus each displayed
  `1+2 = 3`. KnightOS TI84p reached `/bin/`. Images and downloads are ignored
  build artifacts. Source and license limitations: `RUNTIME_SOURCES.md`.
* SD installer: two installs preserve fabricated user ROM/save bytes and one
  index row; standalone UF2 and a full index are rejected before mutation.
  Testing used isolated directories, not a real SD card.
* Contract check: source hashes, public modules, frontend v2, dual USB macro,
  old direct-Loader gesture disabled, unsupported Audio capability absent.
* Both builds passed; UF2 address/family/block verification passed. Own adapter
  compilation treats warnings as errors. Existing PIO USB attribute warnings
  remain in third-party code. Build-cmake artifact scan located the final ELF.

## Firmware

| Image | Start | Bytes | SHA-256 |
|---|---|---:|---|
| `build-pico-retropicoes-v21-bootloader-make/ti83.uf2` | `0x10080000` | 835072 | `768ddeb4b2526ed037412e702500484b932884669f10c34e70a3dacb509c5188` |
| `build-pico-retropicoes-v21-make/ti83.uf2` | `0x10000000` | 835072 | `68617884afe75357988c798ece4cf1068ab4c8a1dd55a06af7550ebb3260e7de` |

Flash 417060 bytes; static main RAM 373804/524288 bytes (71.30%); linker
runtime headroom 152532 bytes. Core memory including ROM/RAM/LCD is allocated
at runtime in PSRAM (approximately 295 KiB, 545 KiB or 1153 KiB by model).
Core control structures and timers stay in internal SRAM. HW16 platform uses
the existing 252 MHz configuration, unchanged.

## Remaining hardware work and limits

No COM11 access, SD write, BOOTSEL or hardware test was performed. The task
is a software candidate, not a hardware PASS and not a finished compatibility
claim for all calculator applications. Main-window hardware owner must confirm
the SD identity, install this exact Loader candidate and chosen runtime image,
then perform one concentrated test: real and empty ROM browser, both entries
to the same Settings, Xbox A/B, calculator arrows and virtual keyboard/ON,
2nd/Alpha, SAVE/CANCEL/DEFAULT, pause/resume, confirmed cold reset, Quit and
Return to Loader. Observe real serial PERF lines and memory/startup behavior.

State save/load, standalone .8xp import/link transfers, CE/CSE/SE models and
link audio are not implemented. TI-83 RAM programs cannot yet be imported;
Plus models can only access applications already present in Flash dumps.
RAM/Flash changes are discarded at Quit/Reset. Next development steps should
add program transfer and versioned calculator state persistence before claiming
general game/content support. No task points or global PASS record was added.

## SD deployment — 2026-10-07 16:09 CST

At the user's request, deployed to H: (exFAT, volume serial 7AD1442C).
Installed the Loader candidate at `H:\emu\16\ti83.uf2`, the cover at
`H:\emu\assets\themes\0\ti83.png`, and all three verified TI ROMs under
`H:\roms\TI83`. Created `H:\SAVES\TI83` and added exactly one
`ti83;ti83;TI-83/84` Loader row. The index now has 32 active entries;
all other index lines were preserved. SHA-256 readback of the UF2, cover,
and three ROMs matched their source files. UF2 validation and installer passed.

Pre-install backup and readback manifest:
`.emv2/backups/sd-before-ti83-20261007-160907/`.
This supersedes the earlier statement that no SD write was performed.
No serial, BOOTSEL or board test was performed; hardware acceptance remains pending.

## Game-first revision — ti83-a2, 2026-10-07

Supersedes a1's .8xp limitation. Public browser now lists .8xp games and hides
system dumps. Default backend system remains `/roms/TI83/ti84plus.84prom`.
Cold boot auto-powers on the calculator (a1 required manual ON), verifies the
single-program file, transfers to RAM via emulated GrayLink and executes the
program with remote OS keys. BASIC and standalone classic Z80 assembly are
supported up to 24000 data bytes. Recognized CE/CSE and Ion/Mirage headers,
malformed checksums/lengths/names and grouped variables are rejected.

Game controls: directions -> calculator arrows, A -> 2nd, B -> Alpha, Start
-> virtual keypad; Menu/Home -> common Settings. Keypad default OFF and saved
preference is respected. Reset cold-boots and reimports the selected game.
Source/license-complete GPL Snake and 2048 subset is bundled. Installer option
`-IncludeSampleGames` seeds games and copies corresponding source/license to
`/licenses/TI83`, preserving different existing game data.

Validation: CTest 3/3; source/frontend contract; repeated installer preserving
ROM/save/edited-game bytes; real TI84 production frontend Snake and 2048
import, remote auto-start, directions, pause/resume/Menu/Quit; missing system
and cancellation cleanup; original BASICOK fixture auto-start. LCD images were
visually inspected (Snake, 2048 before/after moves, BASICOK). Commands:
`test-host.ps1`, `test-runtime.ps1 -SystemRom <full-ti84-dump>`,
`Test-Contracts.ps1`, `test-installer.ps1`, both HW16 build variants.

| Image | Start | Bytes | SHA-256 |
|---|---|---:|---|
| a2 Loader | 0x10080000 | 842240 | df7e9bb540e5657cac67d5f338d91af5dc4757eb2ca0d559db7997f876d5cb45 |
| a2 standalone | 0x10000000 | 842240 | a6fab1bb9e4745a074b4ca06c8045aa5793780f1cf968a827266c0a41a204953 |

Flash 420732 bytes; static RAM 373812/524288 bytes (71.30%); linker headroom
152524 bytes. Program file buffer temporarily uses up to 24074 heap bytes.
Calculator backing memory remains in PSRAM. Existing SDK/PIO USB warnings are
unchanged; own adapter/program sources compile with warnings as errors.

a2 has not been written to SD: the previously authorized H: card is currently
not mounted. No COM11/BOOTSEL access or new board evidence. No global PASS.
Prepared local `build-delivery/ti83-a2-sd-update.zip` contains Loader UF2,
cover, two games and source/licenses; it retains the existing system filename
and does not replace the SD-wide emulators.txt. TI system ROMs are excluded.

Hardware acceptance: reconnect the known H: SD, verify volume serial 7AD1442C,
back up old TI83 UF2/index and deploy a2 with sample games. In Loader select
TI-83/84: three old system dumps should no longer appear; select Snake or 2048
and verify automatic game start, all directions, 2nd/Alpha, Start keypad,
Menu pause/resume, confirmed Reset, Quit and Return to Loader. Capture PERF TI83
rather than BRICKGAME. Host success is not board performance acceptance.

Limits: Shell/library/dependency import, CE/CSE, snapshots, game/high-score
persistence and audio are absent. Only default TI84 system is exposed in this
flow; advanced model choice remains future work. Unknown dependencies may
produce an OS error. General game compatibility and long-play stability are
not claimed from these smoke tests.

2026-10-07 17:05 CST: At the user's request, deployed ti83-a2 to H:,
volume serial 7AD1442C. Loader UF2 (SHA256
DF7E9BB540E5657CAC67D5F338D91AF5DC4757EB2CA0D559DB7997F876D5CB45),
cover, Snake and 2048 passed SHA-256 readback. Bundled game sources/licenses
also matched. Existing three system ROMs, save files and emulators.txt are
unchanged. Backup/manifest: .emv2/backups/sd-before-ti83-a2-20261007-170535/.
Supersedes the prior H:-absent/no-a2-deployment statement. Deployment PASS;
board game startup, controls and performance remain awaiting user acceptance.

## ti83-perf1 — performance candidate

User 2048 a2 board logs: 9.6–12.8 FPS, step 75.1–100.8 ms, draw ~1.95 ms,
present 4–5 us, every frame over budget, no video drops/resync. This identifies
the core execution stage as the bottleneck; graphics/HDMI is not the dominant
cost. PSRAM/cache effects and dispatch/code layout are optimization hypotheses,
not independently measured causes yet.

TI84 physical RAM banks 0x40/0x41 (32 KiB) now have an authoritative internal
SRAM buffer. CPU callbacks preserve bank maps, forced-page windows, read/write/
execute delays, protection state and no-exec exceptions; other RAM/Flash goes
through unchanged upstream callbacks. Flush/detach before raw memory inspection,
cloning, state save or freeing. Production reset frees/recreates the calculator;
attach happens after ROM load/stateloaded and before OS boot. Single owner only.

Upstream source remains unchanged/hash locked. A private forced header places
CPU loop, memory/I/O helpers and timer/breakpoint checks in SRAM. `-Os
-fno-inline` reduces ARM dispatch size from 154768 to 104748 bytes. The dispatcher
itself remains in Flash: moving it to SRAM failed the >=64 KiB runtime headroom
check, and that experimental build was not delivered. Final hot-symbol address
guard verifies SRAM placement. Platform and calculator clocks/time slices are
unchanged; no frames or emulated instructions are intentionally skipped.

Tests: 50000 randomized memory/protection/delay operations equivalent; Snake
and 2048 each 300 scripted-input frames match reference registers/clock/IRQ,
full ROM/RAM/LCD memory and hardware registers; CTest 4/4; production frontend
real-ROM import and controls remain PASS. Source contract and installer PASS.
These checks prove exercised semantics, not board speed or general compatibility.

Final builds: both HW16 variants PASS; FLASH 367068 bytes, static SRAM 408236
bytes, runtime heap headroom 118100 bytes (>65536); hot-symbol SRAM guard PASS.
Loader UF2: 734720 bytes, address 0x10080000, SHA256
D4051C665129047B680A94057F133A5A2AF66115EDBA05D1B7DBB7A8529F7589.
Standalone UF2: 734720 bytes, address 0x10000000, SHA256
2ECFC33F65B7242F41124B0DD1B3267E718E7D2A02030C8835B8C488E5836BEC.
H: absent at final packaging; perf1 has not been deployed or tested on board.

2026-10-07 17:32 CST: User reconnected H:, confirmed volume serial 7AD1442C
and prior a2 UF2 hash. Deployed perf1 loader only to H:/emu/16/ti83.uf2;
734720 bytes, SHA256 D4051C665129047B680A94057F133A5A2AF66115EDBA05D1B7DBB7A8529F7589.
UF2 address/block validation, staged hash and final readback PASS. Index and
all TI83 ROM/game/save files (6 preserved files total) hash unchanged.
Backup/manifest: .emv2/backups/sd-before-ti83-perf1-20261007-173256/.
Supersedes H:-absent deployment status above. Board performance remains pending;
no hardware speedup or global task acceptance claimed.

## ti83-perf2 — ROM hot pages, split CPU and responsive startup

User perf1 board retest: 2048 15.4–20.2 FPS, step 46.9–62.5 ms,
draw ~1.9 ms, no video/audio/UART drops. This confirms improvement from a2,
but every frame still exceeds the 16.667 ms target. Task remains unaccepted.

Host bus trace for 300 idle/waiting-input frames after 2048 launch found
7,901,013 opcode fetches and 3,277,554 operand reads from physical ROM bank 00;
RAM 41 opcode fetches were 345,522. This identifies bank 00 as the dominant
memory traffic in that scenario, not an independently measured wall-time share.
perf2 keeps bank 00 (16 KiB) and a tagged 16 KiB/128-byte-line ROM cache in SRAM.
All Flash writes invalidate both caches; command/busy reads bypass caching.
Read protection, unlock sequence, forced pages, no-exec and delays are preserved.
Flush/detach before raw RAM access; detach before raw ROM mutation/stateloaded.

scripts/split-cpu.py generates a private translation unit from pinned upstream:
main opcode switch is separated from CB/ED/DD/FD paths, with iterative prefix
handoff (no recursive prefix chains). Unmodified upstream stays hash-locked.
Main dispatch (~12610 bytes), its wrapper, CPU loop/helpers and memory callbacks
reside in SRAM. Prefix dispatch remains Flash. No calculator/platform clock,
time-slice, frame skipping or undocumented-instruction semantics changes.

Starting game previously simulated a fixed 4250 ms cold boot/key sequence and
another 500 ms after Enter ACK. Boot now advances in cancellable 10 ms chunks,
waits for cold power-off HALT before ON and active power-on HALT after ON (bounded
by previous limits), then Enter/Clear. Default TI84 host boot takes 1320 ms
simulated time. This is not a measured board startup duration. The post-ACK
500 ms wait is removed; game execution continues in normal frames. Loading
shows BOOTING SYSTEM / IMPORTING GAME, polls cancellation during boot, and
prints TI83 LOAD boot_ms/import_ms using actual platform ticks on the board.

Verification: 8192 opcode/prefix/flag cases versus original TilEm CPU; 50000
randomized bus operations plus actual Flash program/sector erase invalidation;
5/5 CTest; Snake/2048 300 scripted frames each compare split CPU + cached bus
against original CPU + bus for registers/clock/IRQ/full memory/LCD/hw registers;
production frontend missing-system/cancel/import/movement/pause/resume/quit;
BASIC launch, upstream lock/contract and isolated installer checks PASS.
Both HW16 images PASS SRAM symbol/address and >=65536-byte headroom guards.
Static SRAM 454768 bytes; runtime linker headroom 71568 bytes. FLASH 367596 bytes.
Loader: 735744 bytes, 0x10080000, SHA256
4C4C74810F99F6CEDAC1A6AC5486176C45E52F9DAB602B65FC069CD1AF8A44DF.
Standalone: 735744 bytes, 0x10000000, SHA256
974A25C4F3FE70165F294A939FC45D955891ACFFC2C053302AB857AD142E88A0.
H: absent at packaging; perf2 has not been deployed. Required next evidence:
same 2048 scene PERF logs, TI83 LOAD phase timing, boot/control/reset/quit checks.
No perf2 board speedup, normal startup duration or 60 FPS claimed yet.

2026-10-07 17:51 CST: User reconnected H:, volume serial 7AD1442C confirmed,
previous perf1 loader hash verified and backed up. Deployed perf2 loader only
to H:/emu/16/ti83.uf2, 735744 bytes, SHA256
4C4C74810F99F6CEDAC1A6AC5486176C45E52F9DAB602B65FC069CD1AF8A44DF.
UF2 address/block check, staging hash and final readback PASS. Six preserved
index/ROM/game/save files hash unchanged. Backup/manifest:
.emv2/backups/sd-before-ti83-perf2-20261007-175146/.
Supersedes perf2 H:-absent status. Hardware FPS and startup duration pending;
request TI83 LOAD and same-scene PERF TI83 logs. No task acceptance claimed.

## ti83-perf3 — complete redraw of loading buffers

User video 40d2184c5ea9494e1b6ef58481ccc28b.mp4 (38 frames at 29 FPS,
1.31 seconds) shows IMPORTING GAME while alternating between the old generic
TI-83/84 LOADING page/footer and STARTING GAME. The defect is in the loading
frontend: each progress update acquired the alternating inactive display buffer
but only cleared rows 108–139, leaving that buffer's prior title/background/
footer intact. Steady game frames already clear the whole surface.

A shared draw_loading function now clears the full acquired surface and draws
title + current stage on every initial/progress presentation. No shared video
backend or core CPU/cache/timing behavior changed. This fixes the reproduced
loading-page source defect; hardware verification remains pending.

Production frontend smoke now starts two alternating buffers with different
poison patterns and advances a fake clock to force loading progress updates.
Every submitted pixel must be background/text, the heading must match across
buffers and the old footer region must be blank. Old perf2 fails this assertion;
perf3 passes Snake 77/76 and 2048 81/81 presentations per buffer. Host fake LOAD
timestamps are deliberately synthetic and are not startup-speed evidence.
5/5 CTest, both real-ROM game input/lifecycle smoke, 600 game-reference frames
and original BASIC launch PASS. Board loading stability, actual gameplay,
startup phase timings and perf2/perf3 FPS still require the user's feedback.

perf3 both HW16 builds and hot-symbol/UF2/headroom guards PASS. Static SRAM
454768 bytes, heap headroom 71568 bytes; FLASH loader 367396 bytes.
Loader UF2: 735744 bytes, 0x10080000, SHA256
B9CFBC68FE1998297B8B2D049CB8F9F2F8E56258F5BF731FFBDE6A5E3C76227C.
Standalone UF2: 735744 bytes, 0x10000000, SHA256
41796821EBDA3D92AA6E219535419042FE75BA5F67BD6830CE82656905C46540.
SD update ZIP verified against loader hash. H: absent at final check; perf3
has not been deployed. User should reconnect card for the authorized update.

2026-10-08 11:02 CST: User explicitly requested perf3 installation on reconnected
H:. Volume serial 7AD1442C confirmed; previous firmware hash identifies perf2.
Backed up previous UF2 and index under
.emv2/backups/sd-before-ti83-perf3-20261008-110227/ (deployment.json included).
Wrote only H:/emu/16/ti83.uf2, perf3 loader, 735744 bytes, SHA256
B9CFBC68FE1998297B8B2D049CB8F9F2F8E56258F5BF731FFBDE6A5E3C76227C.
UF2 address/block, backup, staging and final readback checks PASS. Six preserved
index/ROM/game/save files hash unchanged. Supersedes earlier not-deployed status.
Loading-page stability and performance remain awaiting hardware retest.

2026-10-08 user acceptance: previous Starting game flicker disappeared after
perf3 installation. Loading-page alternating-buffer defect is accepted as fixed
in the user's tested scene. This is limited video acceptance, not whole TI83
compatibility/performance acceptance. User stopwatch measured approximately
13 seconds from selection to 2048 game entry; Snake also takes a comparable
wait. TI83 LOAD stage timings were not available to the user. Common cold-boot /
GrayLink import is a candidate bottleneck; the largest stage is not measured.
No fixed 13-second sleep exists in the loader; default boot readiness and link
protocol work are executed on every selection. Startup latency remains pending.

## ti83-perf4 — session-local pristine boot cache and link hot functions

User perf3 loading-page flicker acceptance remains valid. Remaining symptom:
2048 approximately 13 seconds by stopwatch; Snake comparable. No per-stage
hardware logs were available, so no individual stage has a measured board share.

After the first successful game-mode OS boot, detach/flush fast RAM, deep-copy
the clean OS-ready calculator, and reattach. Each subsequent game/reset still
opens and fully reads the current system image, applies stateloaded certificate
fixup, and compares every ROM byte before copying the pristine boot state.
Mismatch/copy allocation failure discards the cache and falls back to cold boot.
The game is always freshly imported; game RAM/scores are not reused. Cache has
one entry, lives only during this application run and is not written to SD.
Large copied memory is PSRAM; control blocks stay in SRAM. Exiting to Loader /
power cycling starts cold again. First cold startup and link transfer still run.

Private compile headers move x4_z80_in/out and dbus_update / byte-assist hot
functions into SRAM, preserving pinned upstream source and protocol timing.
To leave room for cached calculator control blocks, the nonzero-bank ROM line
cache shrinks from 16 KiB to 8 KiB; the dominant ROM bank 00 remains 16 KiB,
and authoritative RAM remains 32 KiB. Mandatory ELF guard now checks I/O/link
symbols as well as CPU/main-dispatch/RAM callbacks. This is a board performance
candidate; moving functions is not claimed as a measured latency improvement.

Loading page keeps complete redraw and now shows elapsed whole seconds. UART
TI83 LOAD includes warm, system_ms (ROM read/fixup/validation/restore), boot_ms
(cold boot + creation of pristine copy), import_ms and total_ms (whole start).
Host fake clocks force all loading redraws; printed host milliseconds must not
be used to predict board seconds. Subsequent warm launches omit 132 ten-ms
boot progress callbacks in the default ROM test. First launch still boots.

Validation: 5/5 CTest; 8192 upstream CPU cases / 50000 cached bus operations;
600 total scripted game/reference frames; real-ROM cold then same-game warm
launch/movement and Snake->2048 / 2048->Snake warm switching PASS. A missing
system file is rejected even with a cached boot state. Dual-buffer poison/title/
footer checks remain PASS. 2048 initial tile positions/values can differ between
fresh sessions because skipping boot changes RNG consumption; both starts show
a valid fresh board and respond to controls. No pixel-identity claim across
randomized independent sessions. BASIC auto-start and upstream lock/contract PASS.

Both HW16 images/UF2/SRAM-symbol/headroom guards PASS. Static SRAM 450984 bytes;
linker runtime headroom 75352 bytes (>65536). Loader 737280 bytes, 0x10080000:
B53F3030E696A67C51FFA1BEAE85CE00CF145F799FC87C7BE8A7BEE3CB3A66B4.
Standalone 737280 bytes, 0x10000000:
9409B30FB8AD61AB5E8F3F03E3D8B35BD19D19EA9F2D9F9C8DB4D89AE98063CA.
H: absent at final development check; perf4 not deployed. Required board retest:
first 2048 cold start, return to browser and load Snake, load 2048 again without
leaving TI83; record elapsed seconds / TI83 LOAD and steady PERF, controls/reset.
No startup-duration or 60 FPS acceptance claimed.

## perf4 SD deployment — 2026-10-08 13:07 +08:00

User reconnected H:. Verified exFAT volume serial 7AD1442C and size
61857595392 bytes. Deployed only H:/emu/16/ti83.uf2 (737280 bytes), Loader
start 0x10080000; temporary copy and final readback SHA256 matched
B53F3030E696A67C51FFA1BEAE85CE00CF145F799FC87C7BE8A7BEE3CB3A66B4.
All 6 existing index/ROM/game/save files retained identical hashes.
Backup and manifest: .emv2/backups/sd-before-ti83-perf4-20261008-130744/.
Deployment PASS; board startup duration, warm game switching, controls and
steady performance remain pending user test. Test 2048 first, then Snake and
2048 again through the TI83 browser without leaving the emulator.

## perf4 board result and perf5 CPU candidate — 2026-10-08

User board log: warm=0, system_ms=584, boot_ms=2921, import_ms=11011,
total_ms=14553. Steady 2048 fps/display 17.9, step=53480–53487 us,
draw=1801–1804 us, total=55531–55536 us, over=18/18; all IO counters zero.
Cold import accounts for about 76% of loading and CPU execution accounts for
about 96% of the steady frame. perf4 has not resolved startup or performance.
No warm-launch board result was supplied.

perf5 removes the global CPU no-inline setting, explicitly inlines byte memory
helpers and the memory breakpoint wrapper, and avoids calling an empty logical
memory/opcode breakpoint list. Original checks still run for nonempty lists.
The upstream files remain unmodified. Host CPU uses the same hot declarations
as the Pico CPU; comparison still uses the original upstream CPU separately.
Tests add active opcode/read/write/execute breakpoint checks. No simulated
clock, frame budget, link protocol or game logic has been reduced or skipped.

TI83 IMPORT now reports bytes, transfer_ms, launch_ms, transfer_calls,
launch_calls and ok, to distinguish serial byte transfer from remote startup.
The existing TI83 LOAD report and full loading redraw remain intact.
Host 5/5 tests and real-ROM Snake/2048 600 scripted reference frames,
cold/warm switching/controls and BASIC launch PASS. Hardware gains pending.

perf5 final build: both HW16 addresses/UF2/SRAM symbol/headroom guards PASS.
Static SRAM 453896 bytes; runtime linker headroom 72440 bytes (>65536).
Loader 847872 bytes, 0x10080000, SHA256:
D03BD3FC05186876EA0F81FC9AA9C6BAC383B6BDA1ABEB132C01179B4E2E1A21.
Standalone 847872 bytes, 0x10000000, SHA256:
97A8F506D541E56BAF2131563326C142CCE1AAE289DE5A9BDCBD4CD9EDF36CA3.
Upstream hash/contract checks PASS; H runtime resources contain 3 full-size
system candidates and 2 classic games. First-launch latency/FPS remain pending.

perf5 deployed to H:/emu/16/ti83.uf2 on 2026-10-08 13:51 +08:00.
Verified volume 7AD1442C, Loader UF2 layout, temporary-copy and final-readback
hash. All 6 existing index/ROM/game/save files unchanged. Local backup and
manifest: .emv2/backups/sd-before-ti83-perf5-20261008-135152/.
Deployment PASS; real-board CPU/latency improvement pending. Request TI83
IMPORT, TI83 LOAD and steady PERF from 2048, then a warm browser game switch.

## perf5 board result and perf6 hot-prefix candidate — 2026-10-08

User confirmed boot banner ti83-perf5. 2048 cold: transfer 5769 ms,
launch 3053 ms, import 8831 ms, system 584 ms, boot 2365 ms, total 11818 ms.
Steady 22.8 fps/display; step 41691–41694 us; draw 1752–1757 us;
total 43711–43714 us; all frames over budget; IO counters zero.
Compared with perf4's steady 17.9 fps / 53480 us and 14553 ms cold load,
measured FPS rose about 27%, step fell about 22%, cold load fell about 19%.
This is an incremental improvement, not startup/60 FPS acceptance. Warm boot
has still not been measured on the board. No global resolved-task entry added.

Host-only optional TI83_CPU_PROFILE counts instruction categories and ROM cache
misses; it is OFF by default and not enabled in Pico. Original reference CPU
remains unprofiled. 2048 scripted 300-frame workload: 7426976 main dispatches,
176266 CB, 108096 ED, 62225 DD and 58806 FD. High-frequency ED instructions
include LDIR/OUTI/LDDR/SBC/LD DE; DD uses INC/loads/OR/stack operations.
Existing ROM cache served 9033318 bank-zero reads; other ROM banks had only
hundreds of line misses across millions of reads, so cache enlargement lacks
support in this workload. Counts are host workload evidence, not board timing.

perf6 extracts 20 standalone CB/ED/DD cases directly from locked upstream
headers into a small SRAM dispatcher. Other instructions retain the complete
original prefix dispatcher. Case extraction checks that each selected block
ends independently and has no goto. Prefix fetch, PC/register/flag/memory/port
side effects, cycle delays and interrupt boundaries remain original. The 256-byte
parity lookup table also moves to SRAM; DAA remains in Flash. Build guards now
require the hot-prefix function and parity table inside SRAM. No emulated clock
or frame budget reduction. Full-frame loading redraw and perf5 import phase
logging are retained.

Host 5/5 PASS, including 8192 opcode/prefix/flag and four enabled-breakpoint
paths; Snake/2048 600 scripted reference frames PASS full registers/clock/IRQ/
ROM/RAM/LCD/hardware state. Production cold/warm launches, controls, lifecycle,
loading-buffer regression and BASIC launch PASS. Upstream hash/contracts PASS.
Board performance/latency remains pending; H is absent at latest check.

perf6 final firmware guards PASS (both Loader and standalone addresses).
Static SRAM 455952 bytes, runtime linker headroom 70384 bytes (>65536).
Loader 851456 bytes, start 0x10080000, SHA256:
22D333D6FEDDD796ED7EB9B80A3A2734DA9BBCC1856809F09CE960D9230F90F4.
Standalone 851456 bytes, start 0x10000000, SHA256:
AC5A59E36C547B385748882AB3DCCB6CC66A94487561C4F5A83996EE8EC69C2F.
CPU profile is OFF in normal host tests and Pico. Parity table SRAM guard uses
symbol size 256 to distinguish the SDK function with the same local name.
Firmware-only SD update ZIP: ti83/build-delivery/ti83-perf6-sd-update.zip.
H remains absent; perf6 has NOT been deployed or accepted on hardware.

perf6 SD deployment — 2026-10-08 14:22 +08:00: user reconnected H:.
Verified volume serial 7AD1442C / 61857595392 bytes and Loader UF2 layout.
Updated only H:/emu/16/ti83.uf2 (851456 bytes); temporary copy and final
readback SHA256 matched 22D333D6FEDDD796ED7EB9B80A3A2734DA9BBCC1856809F09CE960D9230F90F4.
All 6 existing index/ROM/game/save files retained identical hashes.
Previous image and deployment manifest:
.emv2/backups/sd-before-ti83-perf6-20261008-142250/.
Deployment PASS; board performance/latency/controls acceptance remains pending.
Compare cold 2048 TI83 IMPORT/TI83 LOAD/steady PERF, then warm game switching.

## perf6 no measurable board gain; perf7 diagnostic — 2026-10-08

User confirmed ti83-perf6. 2048 transfer=5755 ms, launch=3051 ms,
boot=2373 ms, import=8816 ms, total=11810 ms; steady fps/display=22.8,
step=41656–41669 us, total=43697–43708 us, every frame over budget and
IO counters zero. Compared with perf5 total=11818 ms, step=41691–41694 us,
this is within ordinary variation. Hot-prefix/parity SRAM placement has no
meaningful board improvement in this tested scene. Startup/FPS remain unresolved.

perf7-profile is diagnostic, not another claimed speed fix. The generated CPU
samples one instruction per 1024 using the Pico hardware microsecond timer.
It records fetch, main/CB/ED/DD/FD dispatch, end-of-instruction timer checks,
and remaining interrupt/HALT/exception work. Sums are sampled microseconds,
not full stage durations; resolution and instrumentation overhead prevent
interpreting them as precise instruction timing. Timer checks inside I/O helpers
remain in dispatch time. Counters classify the initial prefix, including any
later ignored/repeated prefixes. Original CPU state and emulated cycles remain
unchanged. Original upstream comparison CPU has no profiling hooks.

Boot, transfer and launch each report TI83 CPU and TI83 CPU COUNT. First 120
in-game frames form another window, then sampling disables automatically;
exit/failure also disables sampling. A cheap disabled-hook branch remains in
this diagnostic build, so do not use it as a strict performance A/B candidate.
TI83 MEM prints calc/hardware-register/timer/LCD/ROM addresses and current
emulated CPU frequency to check hot state placement. No new emulator setting
or loading screen text. H is absent at the latest check; no deployment yet.

Host 5/5 PASS, including enabled sampling plus 8192 instruction/prefix/flag
reference cases and four breakpoint paths. Snake/2048 600-frame full-state
reference comparison, production cold/warm controls/lifecycle/loading-buffer
regression and BASIC launch PASS. Host clock resolution is coarse and may
produce zero timing sums; these host times are not board latency evidence.
Upstream hash/contracts PASS. Required next board test: launch 2048, leave it
running for at least 120 frames (about 6 seconds at current speed), capture
TI83 CPU / CPU COUNT / MEM plus TI83 IMPORT/LOAD and steady PERF.

perf7-profile final build/UF2/hot SRAM/parity/headroom guards PASS for both
HW16 addresses. Static SRAM 456304 bytes; runtime linker headroom 70032 bytes.
Loader 852992 bytes, start 0x10080000, SHA256:
A3BA2E4ADF8A141080E9764F7191BBB8248D8AB11EC12F97CDFA477F705BF219.
Standalone 852992 bytes, start 0x10000000, SHA256:
8E1A3C1930FF9A48168ABEC54ADD425A4A33E1553BDE599510AE156D1162DA48.
Firmware-only ZIP: ti83/build-delivery/ti83-perf7-profile-sd-update.zip.
Final H check absent; NOT deployed. Board profile and further optimization pending.

perf7-profile SD deployment — 2026-10-08 15:10 +08:00. Prior attempt found
H absent and made no changes. This retry verified H volume serial 7AD1442C
and size 61857595392 bytes, then updated only H:/emu/16/ti83.uf2.
Loader UF2 layout PASS; temporary copy and final readback SHA256 matched
A3BA2E4ADF8A141080E9764F7191BBB8248D8AB11EC12F97CDFA477F705BF219.
All 6 existing index/ROM/game/save files retained identical hashes.
Previous firmware and manifest:
.emv2/backups/sd-before-ti83-perf7-profile-20261008-151010/.
Deployment PASS; hardware CPU profile and speed/latency fixes still pending.
Launch 2048 for at least 10 seconds; capture TI83 CPU/CPU COUNT/MEM,
TI83 IMPORT/LOAD and PERF. Sampling stops after 120 gameplay frames.

## perf7 board profile and perf8 loading feedback — 2026-10-08

Board profile received for 2048: warm=0, system=584 ms, boot=2552 ms,
transfer=6235 ms, launch=3319 ms, import=9579 ms, total=12753 ms.
First sampled steady windows ~21 FPS / step=45472 us, versus perf6 ~22.8 FPS /
41660 us; sampling adds observable overhead. The 120-frame sample sums:
fetch=1077, main=2523, CB=77, ED=151, DD=49, FD=48, timers=954,
IRQ/HALT/exception tail=360 us from 2832 sampled instructions. Dispatch is the
largest sampled category; main dispatch outweighs all prefixes combined.
Hot calc (20079090), regs (20079288), timers (20077ED0) reside in SRAM;
LCD (11120018) and ROM (11000018) reside in PSRAM. Emulated CPU is 15000 kHz.
These samples are approximate scoped measurements with overhead, not full-frame
absolute costs. Performance/startup remain unresolved; no resolved-task entry.

User requested a progress bar if loading remains slow. perf8-progress adds a
three-stage bar with Preparing / Loading game / Starting labels and the game
filename. Preparing animates inside its first segment because duration is not
known. Completing preparation fills the first third. The second segment follows
actual transferred payload bytes (conservatively excludes the last queued byte
until transfer completion); the final segment follows successfully acknowledged
remote launch commands. Three equal visual segments represent completed phases,
not an elapsed-time prediction. No invented remaining time or overall percentage.
The bar never moves backwards; full fill is drawn only after successful launch.
Elapsed-seconds text is replaced by the game name. Serial LOAD/IMPORT timings
remain. Loading still clears the full framebuffer before each present, preventing
old title/footer/game content from alternating between display buffers.

Timing profiling is now a CMake option TI83_TIMING_PROFILE, default OFF for
host and Pico. Normal CPU hooks compile away; no live timing state or hook
symbols remain in the Loader ELF. The diagnostic can be re-enabled explicitly.
This UI change is not a claim that startup or 60 FPS has been fixed.

Validation: 5/5 host tests; new progress checks verify all three stages, monotonic
confirmed work, final completion, expected bar pixels and both poisoned buffers.
Snake/2048 600 scripted full-state reference frames, cold/warm controls/lifecycle,
BASIC launch and upstream hash/contracts PASS. Loading preview visually checked:
ti83/build-delivery/perf8-loading.png. Both HW16 builds/UF2/hot SRAM/parity and
headroom guards PASS; static SRAM 456008 bytes, runtime headroom 70328 bytes.
Loader 852992 bytes, 0x10080000, SHA256:
F09FB1588227875C5F60A22FC407335EDC54A045089091A671B91EA8116CF6CE.
Standalone 852992 bytes, 0x10000000, SHA256:
2AB983230361C7D72223AF3F891792C4DCEE48BBCB3563757C1E1E4A9848395B.
H absent at latest check; not deployed. Board bar visibility, forward movement,
no flicker, cancellation, warm switching and steady timing remain pending.

## perf9-main — selective main-dispatch optimization, 2026-10-08

User asked to keep working after the progress-bar candidate. Focus remains
perf7's largest sampled category: main instruction execution. A full main
-O2 experiment required 470112 bytes static SRAM / 56224 bytes runtime margin,
below the 65536-byte guard. That experiment was rejected and never deployed.

Final candidate extracts 14 standalone main-opcode cases directly from the
locked upstream header: 10,19,20,25,28,2B,3E,C1,C9,CD,DB,E6,F1,F5.
These include frequent loop/branch/load/stack/call/return/I/O/logic operations
seen in the host workload. A small SRAM z80_hot_main handler is compiled -O2;
remaining main/prefix code retains the compact settings. The selected cases
are removed from the compact dispatcher; unsupported cases route there without
changing emulated state. Prefix fallback can also enter the hot main handler.
Extraction asserts standalone break/no goto, and build guards require hot main
inside SRAM. There is an extra classification call for unsupported opcodes;
actual net benefit must be tested on board. No clock reduction or frame skip.

Progress bar/game-name/full loading redraw from perf8 remain; timing profiling
OFF. 5/5 host tests, 8192 opcode/prefix/flag plus four breakpoint paths,
Snake/2048 600 scripted full-state reference frames, cold/warm loading/controls/
lifecycle, bar monotonicity/both-buffer regression and BASIC launch PASS.
Upstream hash/contracts PASS. Both HW16 addresses, hot SRAM/parity/headroom
and UF2 guards PASS. Static SRAM 457904 bytes; runtime headroom 68432 bytes.
Loader 857088 bytes, start 0x10080000, SHA256:
C17B194DC6DCDDE7EC5320512B3B51C5E5E783EF7FAD53EE1546D008F4DD79AE.
Standalone 857088 bytes, start 0x10000000, SHA256:
A0E3F6DB17AFBD6C695045A9B6C95067EC0E4E7C45A5A4B0D9A19B26A95C97F9.
H appeared at initial check but was absent at final checks. Neither perf8 nor
perf9 has been deployed. Board startup/FPS improvement and progress UI acceptance
remain pending. Next board comparison uses perf6's unprofiled ~22.8 FPS /
41660 us step / 11810 ms cold load, not slower perf7 diagnostic timings.

perf9-main SD deployment — 2026-10-08 16:09 +08:00. User reconnected H:.
Verified volume serial 7AD1442C and size 61857595392 bytes. Updated only
H:/emu/16/ti83.uf2 (857088 bytes); Loader layout, temporary-copy and final
readback SHA256 matched C17B194DC6DCDDE7EC5320512B3B51C5E5E783EF7FAD53EE1546D008F4DD79AE.
All 6 existing index/ROM/game/save files unchanged. Previous firmware and
manifest: .emv2/backups/sd-before-ti83-perf9-main-20261008-160944/.
Deployment PASS; board speed/latency and three-stage progress UI acceptance
remain pending. Capture first 2048 TI83 IMPORT/LOAD and steady PERF, check
bar/flicker/cancellation, then warm-switch games without leaving TI83.

## perf10-timers — remove dispatch regression, shorten common CPU paths, 2026-10-08

Board perf9 result: steady 21.5 FPS, step 44342–44350 us, cold total 12326 ms
(system 584 / boot 2495 / import 9211). This regressed from perf6's ~22.8 FPS
and 41660 us step; no improvement accepted. Removed the separate main opcode
classification/handler and restored the compact main dispatcher.

Timer checks now inline the exact upstream unsigned clock-minus-head-deadline
predicate for both CPU and realtime queues. If either head is due, the complete
unchanged upstream timer processing body runs from SRAM. No skipped due callbacks,
changed simulated clock, reduced CPU frequency or frame skipping. ROM bank-zero
instruction fetch bypasses general address/cache lookup only with a valid cache,
page-zero mapping and idle flash; execution delay, protection and exception
semantics remain. Existing loading feedback/full-frame clearing are retained.

Validation: 5/5 host tests; 8192 instruction cases, 4 breakpoint paths, 8 timer
wrap/deadline callback traces; 50000 randomized bus operations plus protected
bank-zero fetches. Snake/2048 600 total full-state reference frames, production
frontend cold/warm/game switching/controls/cancel, loading dual-buffer checks,
BASIC launch and pinned-upstream/contracts PASS. Profiling OFF. Both HW16 builds,
UF2 address, hot SRAM/parity and >=65536 runtime margin guards PASS.
Static SRAM 456464 bytes, runtime headroom 69872 bytes.
Loader 854016 bytes, start 0x10080000, SHA256:
741B0529E265E0FF695C1C49E4038BC2A80E814143E88723CBE412A73899DB38.
Standalone 854016 bytes, start 0x10000000, SHA256:
03830B04B6718A070F2C71D8E4F495F5998E2043B904D19288482A8608570523.

H: absent at build completion; candidate not deployed. Board FPS/startup gains
remain unverified. Compare steady 2048 logs with both perf6 and perf9, retaining
15 MHz emulation. Loading time and 60 FPS are unresolved; no global resolved
performance/startup case recorded.

perf10-timers SD deployment — 2026-10-08 16:33:44 +08:00. Verified H: exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (854016 bytes), temporary and final readback SHA256 matched 741B0529E265E0FF695C1C49E4038BC2A80E814143E88723CBE412A73899DB38. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf10-timers-20261008-163344. Deployment PASS; hardware FPS/startup improvement remains pending user logs.

## perf11-keyloop — idle/input workload audit and CPU hot paths, 2026-10-08

User perf10 board logs: idle tail 26.5 FPS, step 35628–35634 us, draw
1809–1815 us, input 234–236 us; input/activity range 27.7–34.3 FPS.
Steady improvement over perf9 21.5 FPS / 44345 us step is real, but 60 FPS
and startup acceptance remain unresolved. CPU step still accounts for ~94–95%
of frame work. IO drop/resync counters are zero; present is 0–1 us.

Audited production frontend: startup alone reads game and full system, clones
pristine boot state and imports game. Active frame performs one input update,
one 16666/16667-us emulated step, one LCD blit and present. No per-frame SD
read, boot-cache restore, program import or RAM flush/copy. Warm switching still
reads/compares system once per launch for cache validity; not the steady FPS cause.

Host-only reproducible profile: scripts/profile-workload.ps1 -SystemRom <dump>.
It counts instruction categories, PC hits and ROM cache traffic for 60-frame
idle-before, right-key pulses, idle-after phases. Sequential phases do not have
identical board states; counts are workload evidence, not host/board timing.
2048 source KEY routine uses Repeat getKey / End. Idle-after has 1601508 main
instructions vs 1431926 during key pulses (~10.6% fewer), RET/POP BC >103k each,
CALL ~69.5k, IN ~58.1k; common PC 183F–1842 ~46k hits each. ROM bank 0 has
2235730 cached reads (zero misses); page 3B 34551 reads (zero misses). Busy
keyboard polling and instruction mix explain why pressed-key frames can cost
less, despite identical simulated time. Repeated cache hits are memory bus
accesses, not repeated SD loading. Exact wall-time subcategory rank requires
board profiling; instruction frequency alone is not that measurement.

Candidate changes: retain perf10 timer/fetch guard; add ROM0 data-read shortcut
only when mapping/cache/flash permit, preserving read delay/protection state.
Compile existing 16-bit bus helpers -O2 in SRAM for CALL/RET/stack operations.
Skip empty input/output breakpoint checks, preserving enabled port breakpoint
behavior. Move unchanged upstream keyboard group/read routines into SRAM to
avoid Flash transitions during polling. No keypad result caching, idle-loop
skipping, CPU-clock reduction or frame skipping. Strongly inlining 16-bit
helpers was rejected (62280 runtime SRAM margin <65536), never deployed.

5/5 host tests PASS: 8192 opcode cases, 6 enabled breakpoint paths including
port read/write, 8 timer wrap/deadline traces, 50000 randomized bus operations,
ROM0 read/fetch protection and flash invalidation. Snake/2048 600 scripted
reference frames, cold/warm/session/controls/loading double-buffer/BASIC PASS.
Production workload instruction/PC/ROM counts identical to perf10. Profiling
counts exist only in separate host build, timing profiling remains OFF in UF2.
Board speed and unchanged input/display behavior still require user verification.
Both HW16 builds PASS. Static SRAM 457320 bytes, runtime margin 69016 bytes. Loader 855552 bytes / 0x10080000 / SHA256 2A15E992FF743278A36A0FE732160C0C2BB36770B27F049C8285D9C40D02B943. Standalone 855552 bytes / 0x10000000 / SHA256 F641DB446F6A1E456D5AFF757C1F5573394779AA47E96D757130F1239E7E500B. Production ELFs contain no instruction/PC/timing counter arrays; timing profile OFF.

Deployment pending: H: was present during investigation but absent at final identity/path checks after both builds. perf11 was not copied to SD; perf10 remains the last verified deployed version. Reconnect expected H: volume before firmware-only update.


perf11-keyloop SD deployment — 2026-10-08 17:02:34 +08:00. User reconnected H:. Verified exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (855552 bytes), temporary and final readback SHA256 matched 2A15E992FF743278A36A0FE732160C0C2BB36770B27F049C8285D9C40D02B943. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf11-keyloop-20261008-170234. Deployment PASS; hardware FPS/startup/input acceptance pending user logs.

## perf12-dispatch — remove one per-instruction dispatch layer, 2026-10-08

User board perf11: steady idle 26.9 FPS, step 35073–35080 us, draw 1786–1792 us,
input 224–227 us, total ~37090 us; action windows 28.2–30.6 FPS. Compared with
perf10 idle 26.5 FPS / ~35632 us step, gain is only ~1.5–1.6%; no large gain
claimed. Cold load 10923 ms = system 584 / boot 2068 / import 8234; transfer
5541 + launch 2682 ms, unchanged 1962 / 1024 advance calls. Cold load improved
from perf9's 12326 ms; perf10 cold-load log unavailable. Input-dependent FPS
persists as expected from measured keyboard polling workloads. 60 FPS and
short loading duration remain unresolved; user requested further work.

Main instruction handler is always inlined into existing compact dispatch
function, eliminating one function call and allowing compiler to remove the
main-path next-mode pointer traffic. CPU loop and prefix handlers retain their
existing boundaries. Opcode implementations, callbacks, breakpoints, timer,
interrupt/HALT handling, 15-MHz emulated clock and frame time are unchanged.
This targets >1.4–1.6 million main instructions per simulated second from the
previous host workload, rather than the ~1.8 ms display blit. SRAM size gain/loss
and actual board timing are separate measurements.

Full dispatch-inlining into CPU loop was rejected: static SRAM 474088 bytes /
52248 bytes runtime margin <65536 guard. Never deployed. Selected main-only
inlining passes guards (runtime margin 68616), versus perf11 margin 69016.

5/5 host tests, 8192 opcode/prefix/flag cases, 6 breakpoint paths, 8 timer traces,
50000 randomized bus operations and ROM0/Flash checks PASS. Snake and 2048
600 total full-state reference frames, production cold/warm/session/input/
loading double-buffer regressions and BASIC launch PASS. Locked upstream and
contracts PASS. Board FPS/startup/control acceptance pending. No global resolved
performance case recorded.
Both HW16 final builds/UF2/hot-SRAM/parity/headroom guards PASS. Static SRAM 457720 bytes, runtime margin 68616 bytes. Loader 856064 bytes / 0x10080000 / SHA256 9699D44957427C591B46C4CEC330E2781BF3146A57ECDBE4AE593CE89DA450C3. Standalone 856064 bytes / 0x10000000 / SHA256 48067B1FF76357398A1BACA3FF374982C50165B7D6B05558813B4860C65864FA. ELF main handler inlined; no profile counter arrays; timing profiling OFF.
H: absent at final check; perf12 not deployed. Last verified deployment perf11. Board speed/latency acceptance pending.


perf12-dispatch SD deployment — 2026-10-08 17:18:17 +08:00. User reconnected H:. Verified exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (856064 bytes), temporary and final readback SHA256 matched 9699D44957427C591B46C4CEC330E2781BF3146A57ECDBE4AE593CE89DA450C3. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf12-dispatch-20261008-171817. Deployment PASS; hardware FPS/startup/input acceptance pending user logs.

## perf13-loop — CPU outer-loop compilation, 2026-10-08

User perf12 steady idle result: 28.2 FPS/display, step 33366–33370 us,
draw 1789–1796 us, input 223–226 us, total 35380–35390 us. Compared with
perf11 26.9 FPS / ~35079 us step, CPU time fell ~4.9%; user sees improvement
and requests continuation, not full performance acceptance. IO counters zero.
Loading result not provided for perf12; no new latency improvement claimed.

Reviewed skill FPS cases: VIA/CPU stable-boundary paths support shortening
repeated CPU housekeeping, while preserving timer/IRQ boundaries. Existing
perf10 timer-head guard already follows that pattern. Coleco hot SRAM guidance
already applies to main/fetch/keyboard functions and cached ROM0/RAM. HSTX
backpressure and audio batching cases do not match present ~0 us / video drops
zero / audio absent. Renderer-only caching targets ~1.8 ms vs CPU ~33.4 ms,
so remains lower priority. No wholesale unrelated emulator fix applied.

Candidate compiles only SRAM z80_execute outer loop with -O2, allowing faster
register allocation and branch code for fetch/dispatch/IRQ/timer/HALT handling.
The large main dispatch retains -Os plus perf12 main-only inlining; no repeated
full-inlining experiment, changed opcodes, batched instruction execution,
skipped polls, reduced simulated clock, frame skip or shared platform changes.
First guard build passed with runtime SRAM margin 68136 (perf12 68616), +480
static bytes. Exact improvement requires board timing; not inferred from code.

5/5 host tests PASS; 8192 opcode cases, 6 breakpoint paths, 8 timer deadline/
wrap traces and randomized cached bus/Flash tests. Snake/2048 600 total scripted
full-state reference frames, production loading/session/input/cancellation/
pristine warm boot/double-buffer regression and BASIC launch PASS. Contracts
and pinned upstream PASS. No global resolved FPS/startup entry recorded.
Both HW16 final builds/UF2/hot-SRAM/parity/headroom guards PASS. Static SRAM 458200 bytes, runtime margin 68136 bytes. Loader 857088 bytes / 0x10080000 / SHA256 22F9FD643FCE87D9DD95BFEB80C84B436A75A1DB51FAF5C0616B9DD9E8A49ED9. Standalone 857088 bytes / 0x10000000 / SHA256 7D1855DAC0F7AA64A2B7FEC628BF351D33BDD60F01184E9F985326DD6F1F8DC9. No profile counter arrays in production ELF; timing profiling OFF.
H: absent at completion; perf13 not deployed. Last verified deployed firmware perf12. Board performance/input/loading verification remains pending.


perf13-loop SD deployment — 2026-10-08 17:38:54 +08:00. User reconnected H:. Verified exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (857088 bytes), temporary and final readback SHA256 matched 22F9FD643FCE87D9DD95BFEB80C84B436A75A1DB51FAF5C0616B9DD9E8A49ED9. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf13-loop-20261008-173854. Deployment PASS; hardware FPS/startup/input acceptance pending user logs.

## perf14-wordbus — paired memory access for stack/calls, 2026-10-08

User perf13 steady idle: 28.2 FPS, step 33413–33419 us vs perf12 33366–33370 us,
no improvement (~+0.14% CPU time). Outer-loop -O2 reverted. No claim that the
compile-only experiment helped; no new loading logs or full acceptance.

New SRAM paired-word helpers reduce two byte callbacks and duplicated debug
checks for frequent CALL/RET/PUSH/POP/16-bit loads. Read shortcut accepts only
active TI84 fast-bus owner with no logical/physical read breakpoints and either
valid idle-flash ROM0 wholly inside the fixed mapping or two proven contiguous
physical SRAM addresses. Write shortcut accepts only contiguous SRAM addresses
with no logical/physical write breakpoint. Independently translates the second
wrapped logical address to preserve bank crossings and port27/28 overlay edges.
Fallbacks have no side effects and retain original byte operations/debug hooks.

Word reads preserve two read delays and PROTECTSTATE reset; writes preserve two
write delays, byte order and lastwrite. No event sync occurs between original
byte accesses except breakpoint callbacks, so all enabled memory-breakpoint
paths are explicitly excluded. Flash writes/busy/state/read protection, other
models and ownership mismatch retain byte callbacks. No clock/frame/idle-skip
changes. New helpers join required SRAM symbol guard.

5/5 host tests PASS: extended 50000 randomized byte/word bank/protection/delay
operations and Flash invalidation; 8192 CPU cases, 6 breakpoint paths, 8 timer
traces plus 4 TI84 logical/physical read/write breakpoints exercised inside word
instructions. Snake/2048 600 reference frames compare registers/clock/IRQ/full
memory/LCD/hardware state. Production cold/warm/control/session/cancel/double-
buffer regressions and BASIC launch PASS. Locked upstream/contracts PASS.
Actual board gains and startup/control compatibility remain pending user logs.
Both HW16 final builds/UF2/hot-SRAM/parity/headroom guards PASS. Static SRAM 458376 bytes, runtime margin 67960 bytes. Loader 857600 bytes / 0x10080000 / SHA256 632168ABC1DF081CAE38D55D58A0B722160E9FFA0497A060133F83E6399983D6. Standalone 857600 bytes / 0x10000000 / SHA256 E22C4E2C6E09B333B720ADAC1852EF4F25100920A34E29F96CD3558D3AF7EFB3. Both new word helpers manually verified SRAM in both ELF files, with production profile arrays absent / timing profiling OFF.
H: absent at completion; perf14 not deployed. Last verified deployment perf13. Hardware speed/loading/control checks pending.


perf14-wordbus SD deployment — 2026-10-08 17:58:06 +08:00. User reconnected H:. Verified exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (857600 bytes), temporary and final readback SHA256 matched 632168ABC1DF081CAE38D55D58A0B722160E9FFA0497A060133F83E6399983D6. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf14-wordbus-20261008-175805. Deployment PASS; hardware FPS/startup/input acceptance pending user logs.

## perf15-ram — shorten SRAM reads and paired translation, 2026-10-09

User perf14 steady idle: fps/display 30.1, step 31076–31081 us, draw
1787–1792 us, input 224–226 us, total 33092–33097 us. Compared with perf13
~33416 us step / 28.2 FPS, CPU time decreased ~7%, FPS increased ~6.7%.
IO/drop/resync counters remain zero; user confirms improvement and requests
continuation. No startup/long-session/full 60-FPS acceptance claimed.

Candidate keeps word helpers. Normal SRAM byte reads return immediately after
translation, applying RAM_READ_DELAY and PROTECTSTATE reset without subsequent
Flash/protection dispatch. Paired RAM reads/writes reuse first-byte translation
inside a 64-byte logical block; second translation is retained at block end.
Mapper pages are 16 KiB aligned; port27 upper threshold ends at byte 63 and
port28 lower threshold begins at byte 0 of a 64-byte block. Thus both bytes
have identical mapping decisions within a block. Bank/overlay boundaries and
16-bit address wrapping retain original two-translation proof/fallback.
No mapped-pointer cache, stale bank state, timer/callback/input/clock changes.

5/5 host tests PASS; 50000 randomized byte/word mapping/delay/protection cases,
plus 12288 directed pairs at tails 61/62/63 of all 1024 blocks under four
port27/28 overlay combinations (0,1,7,255), alternating RAM banks. Covers every
16KiB page edge, overlay threshold and FFFF wrap; reads/delays/protection and
eligible writes match upstream, final full memory matches. Existing Flash
invalidation, 8192 CPU cases, breakpoint and timer regressions PASS.
Snake/2048 600 full-state reference frames, production cold/warm/session/input/
cancel/double-buffer loading and BASIC launch PASS. Pinned upstream/contracts
PASS. Initial HW16 guard build runtime margin 67936 bytes, versus perf14 67960.
Board FPS/control/loading verification remains pending; no global resolved case.
Both HW16 final builds/UF2/hot-SRAM/parity/headroom guards PASS. Static SRAM 458400 bytes, runtime margin 67936 bytes. Loader 857600 bytes / 0x10080000 / SHA256 3BABF54ED02EF768DE8754F39EDE72D939872C6F6261430FEC4BD8035CF2A7C7. Standalone 857600 bytes / 0x10000000 / SHA256 A7C419C83D487EEBEC29ECBFA7295DAB074810D6AF859E3238D86E093D62832B. Production timing profile OFF; diagnostic arrays absent.
H: absent at completion; perf15 not deployed. Last verified deployed firmware perf14. Hardware FPS/control/loading validation pending.


perf15-ram SD deployment — 2026-10-09 11:51:09 +08:00. User reconnected H:. Verified exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (857600 bytes), temporary and final readback SHA256 matched 3BABF54ED02EF768DE8754F39EDE72D939872C6F6261430FEC4BD8035CF2A7C7. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf15-ram-20261009-115108. Deployment PASS; hardware FPS/startup/input acceptance pending user logs.

## perf16-fetch — inline guarded ROM0 opcode access, 2026-10-09

User perf15 steady idle: 31.2 FPS/display, step 29980–29986 us, draw
1774–1794 us, total ~32000 us. Startup-containing first window 29.8 FPS is
excluded from steady comparison. Compared with perf14 ~31078 us, CPU time
fell ~3.5%; user requests optimizing largest remaining cost. Current cold load
10278 ms = system584 / boot1934 / import7723, transfer5237 / launch2475 ms,
unchanged1962/1024 advance calls. No perf14 cold-load baseline was supplied.

CPU main still has ~30 ms vs ~1.8 ms draw. Previously counted idle workload has
~1.6 million main instructions / simulated second and mostly ROM0 traffic.
Candidate removes the outer-loop callback for active fast-bus TI84, fixed ROM0
mapping, valid cache, idle Flash. Shared cache owner/valid/ROM state replaces
separate fields without duplicate cache. Inline fetch applies original execute
delay, protected-page exception, PROTECTSTATE reset and FF-at-0038 exception.
Cache invalidation/attach/detach/ownership, other pages/models and Flash state
retain the previous callback. Prefix M1 accesses keep the existing callback;
no new instruction batching, idle skip, clock/frame or keypad behavior changes.

Validation: 5/5 host tests; inline path included in randomized bank/protection/
delay fetch comparisons and protected-ROM0 sweep, Flash program/erase fetch
checks after cache invalidation. Prior word/boundary/breakpoint/timer cases
remain PASS. Snake/2048 600 full-state reference frames, production cold/warm/
controls/cancel/loading double-buffer and BASIC PASS. Pinned upstream/contracts
PASS. Initial SRAM guard margin67816 bytes, +120 static bytes over perf15.

Warm frontend smoke initially failed its one-right-key movement assertion.
Screenshot showed two 2 tiles already at far right in separate rows; right is
a legal no-op. Original callback A/B run passed with a different initial board;
both original and inline CPU matched full-state reference frames. Test now tries
up to four directions, still requires a changed game area; it does not weaken
input validation. Subsequent runtime smoke PASS. Initial false failure retained
here; not reported as an emulator input fix or global accepted case.
Board CPU/startup/control results remain pending.
Both HW16 final builds/UF2/hot-SRAM/parity/headroom guards PASS. Static SRAM 458520 bytes, runtime margin 67816 bytes. Loader 857600 bytes / 0x10080000 / SHA256 4CC1B1ED5E7FFE01EC12823D05A9DBAEAF8F5DD16A8A55764B3BB17A0C020C37. Standalone 857600 bytes / 0x10000000 / SHA256 8DF92FA36CF288088F02C0F91F49D08771960CA2A95A7125E0A2124689EDFCF7. Shared fetch cache explicitly verified SRAM in both ELF files; production profile arrays absent / timing profile OFF. Hardware gains pending.


perf16-fetch SD deployment — 2026-10-09 12:09:22 +08:00. H: appeared before completion. Verified exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (857600 bytes), temporary and final readback SHA256 matched 4CC1B1ED5E7FFE01EC12823D05A9DBAEAF8F5DD16A8A55764B3BB17A0C020C37. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf16-fetch-20261009-120922. Deployment PASS; hardware FPS/startup/input acceptance pending user logs.

## perf17-operands — guarded inline immediate operand reads, 2026-10-09

User perf16 steady idle: 34.5 FPS/display, step26881–26886 us,
draw1787–1791 us, total28892–28898 us. CPU time fell about10.3% from
perf15 ~29983 us. CPU still accounts for ~93% of total frame cost.

Candidate removes the byte-read callback for 24 main-instruction immediate
operand sites when the current fast-bus owner has valid cached ROM0, fixed
page0 mapping and idle Flash. It reuses the existing 16 KiB ROM cache; no
duplicate cache, per-frame load or import is added. Original masking, PC
increments, logical/physical read breakpoints, read delay and protection state
are retained. Other pages/models, Flash operations and prefix instructions
keep the existing callback. Guest clock, instruction count and frame timing
are unchanged. This is separate from perf16 opcode fetch optimization.

Validation: 5/5 host tests, including randomized bus/delay/protection/Flash
comparisons and logical/physical immediate-operand read breakpoints. Snake
and2048 each300 full-state reference frames PASS; production cold/warm restart,
controls/pause/menu/cancel, coherent double-buffer loading and BASIC PASS.
Pinned upstream/contracts PASS. Hardware FPS/startup gains remain pending;
host wall-clock measurements are not board timing.
Both HW16 final builds/UF2/hot-SRAM/parity/headroom guards PASS. Static SRAM
460288 bytes, runtime margin66048 bytes (minimum65536). Shared cache SRAM and
production diagnostics absent verified. Loader861184 bytes /0x10080000 /SHA256
6E9605BAE7C07D2143D8EAD58F176ACE6292C270C8D2F62669F5E546ADD26AC1.
Standalone861184 bytes /0x10000000 /SHA256
6A45D1277837C4C4E39F17AFAAEA1B7310D905D6A98B3D6986BE074B4C5F8D1B.

H: absent at completion; perf17 not deployed. Firmware-only delivery ZIP ready. Last verified deployed firmware is perf16-fetch. Hardware FPS/startup/control validation pending.

perf17-operands SD deployment — 2026-10-09 14:03:26 +08:00. Verified H: exFAT serial7AD1442C /61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (861184 bytes). Temporary/final readback SHA256 matched 6E9605BAE7C07D2143D8EAD58F176ACE6292C270C8D2F62669F5E546ADD26AC1. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf17-operands-20261009-140325. Deployment PASS; hardware FPS/startup/control gains pending user logs.

## perf18-dispatch — ordinary instruction dispatch without prefix-loop entry, 2026-10-09

User perf17 steady idle:34.9 FPS/display, step26539–26544 us,
draw1787–1792 us, total28552–28557 us. CPU gain ~1.3% fromperf16
~26883 us; CPU still ~93% of frame cost. IO counters remain zero.

Candidate starts with the ordinary main instruction handler and enters the
prefix loop only when a prefix is returned. Ignored/repeated prefixes return
to the single shared main handler; no duplicated inline main block or per-op
classification function is added. Main/prefix instruction counters remain
consistent. Instruction semantics, breakpoints, timers, guest clock and frame
budget are unchanged. No ROM/game/cache allocation or per-frame loading added.

Validation:5/5 host tests, including8192 opcode/prefix/flag cases, breakpoint
and clock-wrap/timer traces. Snake and2048 each300 full-state reference frames
PASS; production cold/warm restart, controls/pause/menu/cancel, coherent
loading double buffers and BASIC PASS. Pinned upstream/contracts PASS.
Initial HW16 Loader experiment static SRAM460280 bytes, runtime margin66056
bytes (minimum65536). Hardware FPS/startup/control gains remain unverified.
Both final HW16 builds/UF2/hot-SRAM/parity guards PASS. Static SRAM460280 bytes,
runtime margin66056 bytes (minimum65536). Timing profile OFF; diagnostic arrays
absent and shared cache SRAM verified in both ELF files.
Loader861184 bytes /0x10080000 /SHA256
7EBBAB261120BF38F487F7878BD6E8D025DA817576B5C0F0865EA9AA38718605.
Standalone861184 bytes /0x10000000 /SHA256
18E48A95D04737FE749FF7AD2CE20852B0F124DE6E9C2F135EC52716EE51D265.
H: absent at completion; perf18 not deployed. Firmware-only ZIP ready.
Last verified SD deployment is perf17-operands. Hardware gains pending.

perf18-dispatch SD deployment — 2026-10-09 14:33:29 +08:00. Verified H: exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (861184 bytes). Temporary/final readback SHA256 matched 7EBBAB261120BF38F487F7878BD6E8D025DA817576B5C0F0865EA9AA38718605. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf18-dispatch-20261009-143329. Deployment PASS; hardware FPS/startup/control gains pending user logs.

## perf19-keyscan — eliminate empty keypad scans, 2026-10-09

User perf18 steady idle:35.2 FPS/display, step26293–26298 us,
draw1787–1793 us, total28306–28314 us. CPU gain ~0.9% fromperf17;
CPU remains about93% of frame cost. All IO counters zero.

Repeated-work audit: frontend_run_frame performs one simulated16.666/16.667 ms
CPU advance, input update and LCD draw. System/game reads, boot clone, imports
and fast-RAM flush/attach are outside active frame execution. ROM0/cache lines
copy only on invalid cache/miss, not each frame. No whole-ROM reload/copy found.
Framebuffer clear/redraw is ~1.8 ms and preserves coherent double buffers;
per-instruction timer/IRQ boundaries are required for guest timing. Existing
empty breakpoint lists are already guarded.

One redundant hot operation exists in keyboard port reads: with all keys up,
the original function scans8 selected rows and performs a ghost-key closure
pass even though the output must be0xff. Earlier host instruction counts had
~58k IN instructions per simulated second; they are workload counts, not board
cost measurements. Candidate checks the8-byte keypad matrix via two memcpy
word snapshots and returns0xff if empty. If selected rows yield no bits, closure
is also skipped. Nonempty connected-key closure retains original code; no cache
or delayed keyboard refresh is introduced. ON-key IRQ, groups and guest cycles
remain unchanged. Hash-locked upstream is untouched; generated keypad source
preserves upstream license. ARM disassembly confirms two loads/OR/branch for
the empty path, without memcpy calls or scan loops.

Validation:6/6 host tests. New keypad comparison uses unmodified upstream for
all256 group masks, empty/ON/single press/release/reset, chained ghost-key
connectivity and1024 random matrices. Snake and2048 each300 full-state frames,
production cold/warm controls/pause/menu/cancel/loading and BASIC PASS.
Pinned upstream/contracts PASS. Hardware idle/pressed FPS and startup pending.
Both final HW16 builds/UF2/hot-SRAM/parity guards PASS. Static SRAM460312 bytes,
runtime margin66024 bytes (minimum65536). Production timing profile OFF,
profiling arrays and reference keypad absent; shared cache SRAM verified.
Loader861184 bytes /0x10080000 /SHA256
F7E2711CF4CDF38365F650A1B565FEE04CF2D79A86B3EA197213C615E2AD6821.
Standalone861184 bytes /0x10000000 /SHA256
E6228C91F7CD2928DDD42117500B894E9B7B9C848FF6730BE41B5599C210D599.
H: absent at completion; perf19 not deployed. Firmware-only ZIP ready.
Last verified SD deployment is perf18-dispatch. Hardware gains pending.

## perf20-wordread — inline paired read inside the SRAM word helper, 2026-10-09

User latest steady logs:35.1 FPS/display, step26336–26342 us,
draw1847–1851 us, total28409–28418 us. No measurable gain overperf18.
Build banner was not supplied; local last verified SD deployment isperf18,
andperf19 had not been deployed by this agent. Do not attribute these logs
conclusively toperf19. CPU remains the dominant ~93% frame component.

Updated host workload confirms idle-after1.601M main instructions/simulated
second:RET103762, POP BC103653, CALL69535, PUSH AF69319. ROM0 reads2.236M,
zero misses. These are frequency counts, not board category timings. Earlier
sampled board timings identified main execution as largest CPU category.

Candidate inlines paired ROM0/SRAM read and SRAM write validation/completion
into the existing out-of-line SRAM z80_readw/z80_writew helpers, removing their
extra cross-translation unit calls and read output-pointer handoff. Caller instruction functions retain the
same word-helper call; no full-handler or whole-CPU inlining. Existing32 KiB
fast RAM is exposed to the helper under a new symbol, without allocating a
second buffer. Address masks, ownership, logical/physical breakpoint fallback,
Flash/cache/page guards,64-byte overlay boundaries and per-byte delays remain.
Both paired caller paths are inlined; byte-access fallbacks remain intact.
No idle skip/batching/time changes.

Combined read/write inlining experiment rejected:runtime SRAM65480 bytes below
65536 minimum. Intermediate read-only candidate passed65728-byte runtime margin. Final
combined candidate removes unused production wrapper copies, then passes66064
bytes with the same65536 minimum. Host wrappers remain for direct tests;
production CPU uses the same inline bodies. Initial build macro collided with c->ram field; corrected
before testing. Neither failed experiment was delivered.

Validation:6/6 host tests, randomized word/bank/overlay/wrap/Flash comparisons,
logical/physical word breakpoints and timer traces PASS. Snake and2048 each300
full-state reference frames, production cold/warm controls/loading/BASIC PASS.
Pinned upstream/contracts PASS. Hardware FPS/startup/control gains pending.
Both final HW16 builds/UF2/hot-SRAM/parity guards PASS. Static SRAM460272 bytes,
runtime margin66064 bytes (minimum65536). Timing profile OFF; diagnostic arrays
and unused external word wrappers absent in production. Shared32 KiB fast RAM
and16 KiB ROM0 cache remain single SRAM objects, verified in both ELF files.
Loader861184 bytes /0x10080000 /SHA256
91FA51EF81F38BBA106D550C992AD97A470ECF663C8E2812639522F32D5C086A.
Standalone861184 bytes /0x10000000 /SHA256
82A5F8CF582DD300CBDEADB36CC9A6497D3727643467640841DBF4FCCB19578F.
H: absent at completion; perf20 not deployed. Firmware-only ZIP ready.
Hardware FPS/startup/control gains pending.

perf20-wordread SD deployment — 2026-10-09 16:03:07 +08:00. Verified H: exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (861184 bytes). Temporary/final readback SHA256 matched 91FA51EF81F38BBA106D550C992AD97A470ECF663C8E2812639522F32D5C086A. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf20-wordread-20261009-160306. Deployment PASS; hardware FPS/startup/control gains pending user logs.

## perf21-hotmain — inline common main instructions in SRAM CPU loop, 2026-10-09

User perf20 steady idle:35.9 FPS/display, step25742–25746 us,
draw1788–1791 us, total27756–27762 us. CPU improvement~2.3% from
latest26339 us baseline; IO counters remain zero. CPU still dominates.

Candidate generates53 selected ordinary opcode cases directly from pinned
upstream into an always-inline hot executor inside the SRAM CPU loop. The loop
usesO2; common instructions avoid the general dispatch function call. Unlike
perf9, no per-instruction classifier call is introduced:the hot switch executes
the instruction, default calls the complete fallback. Complete main fallback
is noinline Flash; prefix dispatch/helpers retain existing SRAM placement.
Selected opcodes exclude prefixes; ignored/repeated prefixes and IM0 interrupts
still use the full dispatcher. Host workload counters show main hot coverage
76% boot/import,74% initial idle,69% right key,75% steady idle. Frequency is not
board timing; Flash fallback can reduce gains for different instruction mixes.

Guest instructions, cycle delays, timer/IRQ boundaries, debug semantics,
memory/cache ownership and frame budgets remain unchanged. No batching, idle
skip or new emulated-state caches. Generator asserts all256 main cases and
validates selected block structure; hash-locked upstream remains untouched.
Diagnostics counters exist only in host profiling builds, absent in production.

Validation:6/6 host tests including8192 opcode/prefix/flag cases, logical/
physical breakpoints and timer traces. Snake and2048 each300 full-state
reference frames, production cold/warm controls/pause/menu/cancel/loading and
BASIC PASS. Pinned upstream/contracts PASS. Initial HW16 SRAM449000 bytes,
runtime margin77336 bytes (minimum65536). Board FPS/startup pending; compare
idle and held-key behavior in2048 andSnake, not idleFPS alone.
Both final HW16 builds/UF2/hot-SRAM/parity/cold-Flash placement guards PASS.
Static SRAM449000 bytes, runtime margin77336 bytes (minimum65536).
Hot executor has no out-of-line symbol; production timing profile OFF and all
instruction/path/profile arrays absent. Single shared RAM/cache SRAM verified.
Loader873472 bytes /0x10080000 /SHA256
862EED1D2E1EE5B4BB1BC9232135EF6892BA0B044D8FAEC7D74ED2A25B7C1832.
Standalone873472 bytes /0x10000000 /SHA256
C0B47ECBFE46BAD6ADD48052D116C0A8613B6B333C46E1489BFCF960FA59164D.
H: absent at completion; perf21 not deployed. Firmware-only ZIP ready.
Hardware idle/key FPS, startup and Snake behavior remain pending.

perf21-hotmain SD deployment — 2026-10-09 16:54:06 +08:00. Verified H: exFAT serial 7AD1442C / 61857595392 bytes. Replaced only H:/emu/16/ti83.uf2 (873472 bytes). Temporary/final readback SHA256 matched 862EED1D2E1EE5B4BB1BC9232135EF6892BA0B044D8FAEC7D74ED2A25B7C1832. All 6 index/ROM/game/save files unchanged. Backup: E:\ComporyProject\pico_ES_Public_Projects\Pico_ES_TI83_TI84\.emv2\backups\sd-before-ti83-perf21-hotmain-20261009-165406. Deployment PASS; hardware idle/key FPS, startup and Snake behavior pending user logs.
