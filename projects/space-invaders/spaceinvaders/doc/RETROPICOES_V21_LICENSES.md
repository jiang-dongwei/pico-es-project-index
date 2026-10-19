# S12 license inventory

| Path | Origin | License | Treatment |
| --- | --- | --- | --- |
| `third_party/superzazu8080/i8080.c` | superzazu/8080 | MIT | Unmodified pinned import |
| `third_party/superzazu8080/i8080.h` | superzazu/8080 | MIT | Unmodified pinned import |
| `third_party/superzazu8080/LICENSE` | superzazu/8080 | MIT | Preserved verbatim |
| `core/space_invaders_cpu.*` | RetroPicoES | GPL-3.0-or-later | Project wrapper |
| `core/space_invaders_board.*` | RetroPicoES | GPL-3.0-or-later | Single-board implementation |
| `platform/retropicoes/**` | RetroPicoES | GPL-3.0-or-later | Video, input, audio, state, performance and frontend adapters |
| `../arcade-template/**` | RetroPicoES | GPL-3.0-or-later | Thin reusable contract |
| `tests/**` | RetroPicoES | GPL-3.0-or-later | Self-generated tests; no ROM data |

MAME and `superzazu/invaders` are board-behaviour references only. Their source
files are not copied into this stage. No game data is distributed.
