# S17-D target architecture

The noiz2sa port keeps the game archive independent of Pico SDK and board
drivers. The target adapter implements the existing compat v1 contract and
delegates board work to the pinned `pico_shared` tree.

```text
noiz2sa engine + SDL shim
        |
        v
port/target/compat_pico.c
        |
        +-- target_video_pico.c  -> pico_shared HSTX queue
        +-- target_audio_pico.c  -> pico_shared HDMI audio queue
        +-- target_input_pico.c  -> pico_shared USB/NES state
        +-- target_fs_pico.c     -> pico_shared FatFS mount
        +-- target_runtime.c     -> clocks, PSRAM, timing, Loader return
        |
        v
Pinned pico_shared + Pico SDK (unchanged)
```

The adapter is private to this port. It must not add symbols to `compat/` or
modify `pico_shared`; those are integration-owned layers. The first target
milestone is a normal-address ELF/UF2 that initializes storage, video, input,
audio, and the noiz2sa loop. Loader relocation and hardware acceptance follow
only after that build is reproducible.

## Contracts

- Video framebuffer: 320x240 RGB565, submitted only through the shared HSTX
  presentation API; no direct HSTX register access in the game archive.
- Storage: `/data/noiz2sa/` for read-only assets and `/SAVES/noiz2sa/` for
  preferences; no host `stdio` or POSIX directory API on target.
- Input: compat keys are sampled once per game frame from the shared input
  snapshot; edge state is computed in the adapter.
- Audio: the engine callback feeds the shared HDMI queue; the adapter owns
  format conversion and drop counters.
- Large allocations: `compat_large_*` uses PSRAM; hot frame exchange remains
  in SRAM through the shared video backend.
