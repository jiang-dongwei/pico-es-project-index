# RetroPicoES v2.1 S17-D target boundary (noiz2sa)

This is a structural portability gate, not an RP2350B firmware build.  It
proves what the `noiz2sa` worktree can prove before the main integration window
adds the shared `pico_shared` and Pico SDK pieces.

## Boundary under test

`noiz2sa_game` contains the pinned noiz2sa engine, BulletML, stb_vorbis, and
the port's SDL/screen/sound adapters.  The target build must reuse that archive
unchanged, while supplying an HW16 backend for the `compat.h` contract.

`noiz2sa_stub_backend` is an ISO-C structural substitute for that future
backend.  It depends only on `retropicoes_compat_v1`; it has no engine or port
headers and no OS-specific headers.  `noiz2sa_target_probe` links the same
`noiz2sa_game` archive against this stub instead of `noiz2sa_host_backend`.

Run the gate after the regular host build:

```powershell
cd noiz2sa
.\scripts\build-host.ps1
.\scripts\test-target-boundary.ps1
```

The probe is successful only when it prints:

```text
NOIZ2SA TARGET BOUNDARY: PASS framebuffer=666d0addd757559a nonzero=76800
```

The script checks all of the following:

- `noiz2sa_stub_backend` has no undefined host-OS symbols and no host-OS
  headers in its dependency closure;
- the probe itself has no host-OS headers;
- the port-owned units in `noiz2sa_game` have no host-OS headers;
- the known upstream header gap is unchanged; and
- the stub-backend replay produces the same RGB565 framebuffer FNV-1a hash as
  the host replay.

The normal upstream termination path is `exit(1)`.  As with the host smoke
runner, the script uses the printed `PASS` marker rather than the executable's
exit code.

## Directory portability closed

The pinned-engine patch routes BulletML XML enumeration in
`barragemanager.cc` through `compat_dir_open`, `compat_dir_read`, and
`compat_dir_close`.  It consequently no longer includes `dirent.h` or calls
`opendir` / `readdir` / `closedir` directly.  The structural stub reads a
fixture-generated `.compat-dir` manifest using ISO C so this route is exercised
without accidentally importing a host directory API.  A production S17-D
backend supplies the same contract through FatFS.

The boundary baseline deliberately permits no remaining host-OS headers in
upstream translation units.  Any future one fails the gate.

This worktree does not implement the real target backend.  It needs the
main-window-owned `pico_shared` integration and must supply FatFS, HSTX/HDMI,
audio, monotonic Pico timing, input, and target-memory allocation there.
