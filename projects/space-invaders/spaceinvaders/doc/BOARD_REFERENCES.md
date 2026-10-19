# Space Invaders board references

No source from these board references is copied into the project.

| Reference | Revision used | Purpose |
| --- | --- | --- |
| `superzazu/invaders` | `78bcbad77dcf7e641485668b96b39a2ef4e06f38` | Readable memory, shifter, port and interrupt cross-check |
| MAME `src/mame/midw8080/8080bw.cpp` | Reviewed 2026-09-08 | Input/DIP and I/O-map cross-check |

The project implementation in `core/space_invaders_board.c` is independently
structured for RetroPicoES. MAME remains documentation only and is not a build
dependency.
