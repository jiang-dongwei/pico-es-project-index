# Local runtime data, 2026-10-07

The user explicitly requested online ROM downloads. All files are local under
`build-runtime/`, ignored by Git and excluded from code/firmware delivery.
Texas Instruments ROM redistribution permission has not been established;
the BIOS collection's repository license does not license TI OS bytes.
No unrelated personal data or calculator saves are present.

| File | Size | SHA-256 | Source |
|---|---:|---|---|
| ti83.rom | 262144 | `f8f511f9fecff51d7561ad2cc4195fc89f2829db94616cf08c302ea1ba64d8be` | [pinned BIOS file](https://raw.githubusercontent.com/archtaurus/RetroPieBIOS/c4ba6e653a7e431b31c5a3ec4a6577081102cf20/BIOS/ti83.rom) |
| ti83plus.rom | 524288 | `85363d8e699fe0ef3d73fe633a9f73078f8292a33b6150aa7abb1304af7485cc` | [pinned BIOS file](https://raw.githubusercontent.com/archtaurus/RetroPieBIOS/c4ba6e653a7e431b31c5a3ec4a6577081102cf20/BIOS/ti83plus.rom) |
| ti84plus.rom | 1048576 | `dbb47afae091ab36f9abe74e32083013fbeff3d7e0516bbf5d1abf4ee57adc09` | [archived ROM](https://web.archive.org/web/20240409191813id_/http://tiroms.weebly.com/uploads/1/1/0/5/110560031/ti84plus.rom) |

The TI-83 and TI-83 Plus MD5 values match the BIOS collection's known values
(`d4448d09bbfde687c04f9e3310e023ab`, `8011181f810b5ec4e9d6a03f0e14257a`).
The unarchived Weebly links returned 404. The TI-84 source was recovered through
the Internet Archive, with size and successful OS execution verified locally;
its authenticity is not independently guaranteed by a vendor signature.

KnightOS snapshot archive: https://www.ticalc.org/pub/83plus/os/knightos.zip,
SHA-256 `06be40b65c2c036302974090d712bc8adae16c754c27852ef91422fc35caad5f`.
The author-provided [archive page](https://www.ticalc.org/archives/files/fileinfo/463/46387.html)
describes included demos, Rubik and ZTetris. The archive's `dist/LICENSE`
grants MIT terms to the kernel and many programs, and requires consulting
`/etc/licenses` for other programs. This snapshot is used locally only;
component distribution rights have not been exhaustively audited.

`rom_smoke` booted the three TI OS images and generated `1+2 = 3` LCD images.
KnightOS TI84p booted to the `/bin/` file browser; no game-play claim is made.
These are host results, not RP2350 timing or hardware evidence.

## ti83-a2 games and protocol references

Two unmodified author-distributed GPL-3.0 games (Snake and 2048), including
corresponding Axe source programs/text, upstream README and LICENSE, are now
bundled under `assets/games/scott-ti-games`. Exact source commit:
https://github.com/scottmangiapane/ti-games/tree/6c48442b2bf973734927166187287cac26571055
Hashes and installation policy are in `assets/games/README.md`.
These are standalone monochrome assembly programs; no Shell is needed.

Protocol/key-code reference (not vendored or linked): TiLP libticalcs,
https://github.com/debrouxl/tilibs/tree/6dba390e7390c4b98ae96287b39a3971c331fbef/libticalcs/trunk/src
(`calc_73.cc`, `cmdz80.cc`, `dbus_pkt.cc`, `keys83p.h`). The private port
implements only single-program RAM transfer and remote execution over TilEm's
existing GrayLink API. CTS and ACK are short packets even with nonzero status
fields. The final execute key waits for receipt, not program completion.

The BASICOK import fixture is original project test code; it is not a game.
