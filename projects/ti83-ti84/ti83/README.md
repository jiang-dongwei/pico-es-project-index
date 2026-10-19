# TI-83/84 for RetroPicoES

Performance candidate `ti83-perf21-hotmain`, RP2350B / HW_CONFIG=16. TI-83, TI-83 Plus and TI-84 Plus ROMs booted on the host and displayed
`1+2 = 3`. KnightOS TI-84 Plus also reached its file browser. Snake and 2048 auto-launch/input are host-verified; hardware remains pending. Core and production frontend host tests,
standalone and Loader firmware builds have passed.

Uses the unmodified LGPL-2.1-or-later TilEm 2.0 calculator core. Source archive
and per-file SHA-256 hashes are recorded in `sources.lock.json`; the Pico
allocator and board adapter remain outside upstream. Application code is
GPL-3.0-or-later. ROMs are not bundled.

## Game-first entry (ti83-a2)

Enter TI-83/84 from Loader and select a classic `.8xp` game. System dumps are
hidden from the game browser. The emulator automatically cold-boots TI-84 Plus,
turns it on, imports the program over an emulated GrayLink and runs it.
Place a full 1 MiB TI-84 Plus system dump at `/roms/TI83/ti84plus.84prom`;
place games in the same directory or game subdirectories. The existing v1
system filename is retained, so the deployed system does not need renaming.
Only one default system is used in the game flow; model selection is not yet
exposed in Settings. No commercial system ROM is bundled.

Supports a single RAM-resident BASIC or standalone Z80 assembly program per
file (up to 24000 program-data bytes). Checksum, repeated sizes, program name,
and payload length are checked before boot. CE/CSE programs and recognized
Ion/Mirage shell headers are rejected. Multi-file dependencies, Flash apps,
Shell installation, snapshots and audio remain unsupported. Other shell or
library dependencies cannot always be detected from the header. Games run
inside the original OS, so game compatibility still needs individual testing.
Calculator RAM, game progress and high scores are discarded on Quit/Reset.

Bundled GPL games: Snake and 2048, with original source and license under
`assets/games`. Both have passed real-ROM host auto-launch and directional
input smoke tests through the production frontend. Board gameplay and timing
remain awaiting hardware acceptance.

## Controls and frontend

Cold boot opens `common/frontend/rom_browser`; A launches a game, B goes back,
Left/Right changes pages and Menu/Home enters Settings. An empty directory
offers a clearly marked simulated LCD/keypad demo, not a real calculator OS.
Both browser and in-game Settings use the same `settings_host` and
`frontend_shell`. Reset and Return to Loader require confirmation.

Games start with the keypad hidden by default: D-pad sends arrows, A sends 2nd and B sends Alpha. In legacy calculator mode A sends Enter and B sends Clear.
Start toggles the complete 49-key virtual keypad. In keypad mode D-pad moves
the cursor, A holds the selected key and B holds 2nd. Selecting 2nd or Alpha
toggles its latch; select again to release it. ON is available in the keypad.
Menu/Home pauses the calculator and releases all emulated keys. A/B gameplay
bindings use the common controller profile, with Xbox/XInput as the current
hardware acceptance target.

The real 96x64 LCD displays at 3x, or 2x with the keypad. Scanlines, FPS,
menu colors and keypad preference are real settings. SAVE commits the working
copy, CANCEL discards it and DEFAULT changes the working copy only.

## Build and install

Run `scripts/test-host.ps1` and `scripts/Test-Contracts.ps1` from PowerShell. Use `scripts/test-runtime.ps1 -SystemRom <full-ti84-dump>` for real-ROM game import and production frontend smoke tests.
`scripts/build-retropicoes-v21.ps1` builds standalone; add `-Bootloader` for
the `0x10080000` Loader application. `-DependencyRoot` defaults to
`E:\ComporyProject\Pico_ES` for the existing SDK/PIO USB/XInput/picotool.
Shared application sources come from this worktree; no shared source is edited.

The original 320x240 cover is `assets/loader/ti83.png`, reproducible with
`scripts/create-cover.py` (Pillow plus Windows Consolas font).
`scripts/install-ti83-bootloader-sd.ps1 -SdRoot H:\ -IncludeSampleGames` installs the Loader image,
cover and one `ti83;ti83;TI-83/84` index row. It checks image addresses and
hashes, preserves ROMs/saves and rejects a full index without explicit
replacement. Do not run against a real card until its identity and hardware
ownership are confirmed. The a2 installer was checked in an isolated directory; a1 was previously deployed to the user-authorized H: card.

See `doc/HANDOFF.md` for actual verification and remaining acceptance work.

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

## Loading feedback (ti83-perf21-hotmain)

The loading screen shows three stages and a progress bar. Preparation uses an
animated indicator; game transfer and startup advance from completed bytes and
acknowledged commands. The three visual segments represent phases, not predicted
remaining seconds. The selected game name replaces the elapsed-seconds display.
Menu still cancels loading. Full-frame clearing preserves the flicker fix.

CPU timing profiling defaults OFF (TI83_TIMING_PROFILE CMake option). Existing
LOAD/IMPORT logs remain available. Actual cold startup and 60 FPS are still
unresolved; this candidate improves feedback during the wait.

## CPU workload optimization (ti83-perf21-hotmain)

Active frames do not reload ROMs or games. 2048 polls keys while idle; host
counts show more instructions and ROM0 traffic while waiting than during key
processing. This candidate shortens cached ROM0 data reads, optimizes 16-bit
bus helpers, skips empty port breakpoint checks and puts upstream keyboard
scanning in SRAM. Emulated time and keyboard behavior remain unchanged.
Run scripts/profile-workload.ps1 -SystemRom <full-ti84-dump> for reproducible
host-only idle/key instruction/PC/cache counts. Counts are not board timings.
Actual FPS improvement and loading duration require board testing.

The perf12 candidate additionally inlines the main instruction handler into the
dispatch function to remove one per-instruction call; prefix/CPU-loop boundaries
are retained. Board gains remain unverified.


The perf13 candidate compiles the CPU outer loop -O2 while retaining compact
main dispatch. Instruction/event behavior and 15 MHz emulation are unchanged;
actual gain requires board timing.


Perf14 reverts perf13 outer-loop -O2 after no hardware improvement. Paired-word
SRAM/cache helpers shorten stack/call memory access, retaining original byte
paths for breakpoints, Flash state and noncontiguous mappings. Hardware gains
remain pending; emulated clock/time and event handling are unchanged.


Perf15 adds direct SRAM byte-read completion and avoids a second word-address
translation inside a 64-byte block. Every mapping/overlay/wrap boundary keeps
full translation. Board gains remain unverified.


Perf16 reads verified cached ROM0 opcodes directly inside the CPU outer loop.
Ownership, mapping, cache-valid and Flash guards preserve callback fallbacks;
execution delays/protection/exceptions are unchanged. Prefix access retains
the callback. Board gains remain unverified.

Perf16 hardware logs show steady34.5 FPS and ~26883 us CPU time, about10.3%
less than perf15. Perf17 directly reads cached ROM0 immediate operands at24
main-instruction sites while retaining mapping/Flash/owner guards, memory
breakpoints and read delays. Prefix and fallback paths are unchanged. It reuses
the existing cache, preserves guest time, and passes reference/game tests;
its hardware FPS/startup gains remain unverified.
Perf17 board logs show steady34.9 FPS, ~26542 us CPU time (~1.3% less than
perf16). Perf18 routes ordinary instructions directly through the main handler,
entering the prefix loop only when needed. Repeated/ignored prefixes retain a
single shared main path. Instruction/timer/frame semantics are unchanged;
host reference/game checks PASS, hardware gains remain unverified.
Perf18 board logs show steady35.2 FPS and ~26296 us CPU time. Active frames
advance the CPU once and do not reread/import games or copy whole ROMs.
Perf19 skips keyboard matrix scans when all keys are up and skips ghost-key
closure when selected rows are empty. Nonempty matrix behavior and guest timing
retain the original implementation. All group masks and randomized multi-key
states compare against upstream; game/reference tests PASS. Hardware gains
remain unverified.
Perf20 inlines paired memory-read/write completion into the SRAM word helpers
to remove high-frequency function boundaries for stack/word operations. The existing
32 KiB RAM buffer is reused; mapping, Flash, breakpoint, boundary and cycle
semantics are preserved. Unused production wrapper copies are excluded to retain the unchanged SRAM
headroom guard; host reference tests keep their wrappers.
Host/reference/game tests PASS; actual board gains remain unverified.
Perf20 board logs show steady35.9 FPS and ~25744 us CPU time (~2.3% less
than the latest baseline). Perf21 executes53 common ordinary opcodes directly
inside the SRAM CPU loop atO2; complete main fallback resides inFlash. Host
2048 counts show75% steady-idle and69% right-key main-path coverage. All opcode,
state and game tests PASS. Guest timing and prefix handling are unchanged.
Hardware gains and effects on other games remain unverified.