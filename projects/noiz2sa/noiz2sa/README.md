# noiz2sa for Retro_PicoES v2.1

Native RP2350B port of Kenta Cho's noiz2sa (BSD-2-Clause) for `HW_CONFIG=16`.
S17-A establishes the pinned upstream baseline; the video/audio/input front
end, HW16 firmware and SD delivery follow in S17-B through S17-F.

See:

- `doc/RETROPICOES_V21_S17A_HOST_BUILD.md` — pinned sources, port layout and
  the host smoke build;
- `doc/RETROPICOES_V21_S17D_BOUNDARY.md` — S17-D target-boundary probe and
  the remaining HW16 integration handoff;
- `doc/RETROPICOES_V21_LICENSES.md` — per-component license inventory and the
  game-data boundary;
- `doc/RETROPICOES_V21_S17E_SD_INSTALL.md` — SD installation procedure and
  content-readiness gate;
- `.emv2/discussion/2026-09-16-noiz2sa-retropicoes-port/` — requirements,
  technical plan (full-frame 2:1 downscale presentation), milestones and
  hardware alignment.
