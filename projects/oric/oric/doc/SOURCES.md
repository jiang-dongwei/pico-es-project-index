# Source and license inventory

## Linked source

`third_party/chips/{m6502.h,m6522.h,ay38910.h,LICENSE}` comes from
[floooh/chips](https://github.com/floooh/chips/tree/ca7d7ddd3ba77b48685d24120cf413ea53786767),
commit `ca7d7ddd3ba77b48685d24120cf413ea53786767`. The headers retain Andre
Weissflog's zlib/libpng notice. `m6502.h` and `ay38910.h` are unchanged.
`m6522.h` is a **locally modified version**: T1 reloads its latch only in
continuous mode; one-shot mode wraps through FFFF. This follows the one-shot
timing diagram in the [MOS 6522 data sheet](https://datasheet4u.com/pdf/1115291/6522.pdf).
It differs from the upstream header's stated timer model. It is required by
the observed BASIC cassette cleanup sequence, which otherwise enters a
short-period interrupt storm before restoring T1. Host regression and both
BASIC versions exercise the change; physical Oric timing has not been measured.
No upstream-wide correctness claim is made. The patch is confined to this port.

`core/oric.*`, adapter, tests and generator are newly written GPL-3.0-or-later
project code. Pico CMake is adapted from this repository's GPL-3 CHIP-8 build.
Shared compat/frontend/board files are linked read-only from the repository;
none are copied back or edited. The hardware build uses the tracked Digger
platform and pico_shared baseline at this worktree's base `7330ed6`.

## Reference only

[Oricutron](https://github.com/pete-gordon/oricutron/tree/002279fce9fa756d1d63cdc40ae97939eb7de7ed),
commit `002279fce9fa756d1d63cdc40ae97939eb7de7ed`, supplies hardware and TAP
format reference. Its examined CPU/machine source notices specify GPL-2.0
only. No Oricutron source, ROM patch, font, demo or binary is linked or shipped.
Using it as documentation avoids combining those source files with GPL-3 code.
VIA/AY wiring was checked against
[Defence Force hardware documentation](https://wiki.defence-force.org/doku.php?id=oric:hardware:the_via_and_ay-3-8912).

## Data rights

`tools/make_test_tapes.py` produces three **original**, self-contained 6502 test
programs and original 6x8 glyph patterns. Generated `core/demo_tape.h`, TAPs
and procedural cover are GPL-3.0-or-later project assets. They contain no
Oric BASIC, commercial game, Oricutron demo or user save.

System ROMs remain user-supplied, exactly 16384 bytes. Local host-only tests
used these identities, checked against
[MAME's Oric ROM declarations](https://github.com/mamedev/mame/blob/master/src/mame/tangerine/oric.cpp):

| File | SHA-1 |
| --- | --- |
| basic10.rom | `333116e6884d85aaa4dfc7578a91cceeea66d016` |
| basic11b.rom | `9451a1a09d8f75944dbd6f91193fc360f1de80ac` |

Local test acquisition: `https://raw.githubusercontent.com/Abdess/retrobios/main/bios/Oric/Oric/`
plus the filenames above. Location: ignored `_build/local-bios/`. Availability
is **not** a redistribution license. Neither BIOS is staged or bundled.
The adapter validates length; alternative 16K ROMs are allowed but untested.

SDK 2.3.0 and board-side dependencies are existing local dependencies, used
read-only. Candidate delivery includes only the application and original data;
the existing Loader is not modified or redistributed by this task.

`board-dependencies.json` records exact SDK/PIO-USB/XInput commits and hashes
of two **pre-existing local board patches**: PIO-USB's PIO2 selection and the
XInput packet handler. The wrapper verifies these before building. Those
working trees were already modified by the main project; this task did not
change them. A clean machine needs the main project's corresponding fixed
patches, not just the upstream commits. This limitation is explicit rather
than claiming an untested clean upstream build.
