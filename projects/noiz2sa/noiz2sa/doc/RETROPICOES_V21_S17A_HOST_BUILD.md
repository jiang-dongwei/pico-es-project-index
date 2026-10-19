# RetroPicoES v2.1 S17-A host build (noiz2sa)

S17-A pins the upstream engine and proves that the pinned noiz2sa core plus
libBulletML compile and run headlessly on the host through the port shim.

## What is fixed

| Dependency | Version | Verification |
| --- | --- | --- |
| noiz2sa engine | `M-HT/noiz2sa` commit `2253d9cee2026311a5af9e4eeba66707263b5d9e` | git checkout + `rev-parse` check inside the bootstrap |
| libBulletML | 0.0.6 tarball | SHA-256 checked at download |
| stb_vorbis | v1.22 raw file | SHA-256 checked at download |
| compat v1 | workspace `compat/` v1.0.0 | consumed unmodified via `add_subdirectory` |

## Port layout

```text
noiz2sa/
  CMakeLists.txt            host smoke target (S17-A); HW16 targets come in S17-D
  cmake/                    bootstraps (engine, libBulletML, stb_vorbis) + pins
  patches/retropicoes-v2.1/noiz2sa-engine-main.patch
  port/include/SDL.h        SDL shim: the engine's SDL surface is tiny, and the
                            shim is found via the include path, so upstream
                            sources stay unmodified
  port/include/noiz2sa_port.h          engine <-> shim boundary
  port/include/noiz2sa_port_testhooks.h  host deterministic replay hooks (S17-B/C)
  port/include/noiz2sa_port_target.h     target-only surface: downscale mode,
                            path mapping (S17-D/F)
  port/src/sdl_shim.c       shim implementation + portable noiz2sa_port_getw/putw
  port/src/screen.c         S17-C screen.h contract: 640x480 index compose ->
                            palette RGB565 -> 2:1 downscale (replaced the S17-A
                            no-op stub, which was removed in the S17-C cleanup)
  port/src/sound_stub.c     S17-A no-op soundmanager.h contract (S17-C)
  port/host/compat_host.c   libc backend for compat.h (host only)
  port/host/noiz2sa_host_main.c  headless deterministic smoke runner
```

The only upstream patch renames `main` to `noiz2sa_main` in `src/noiz2sa.c`
so the port owns the entry point on both host and target. It is verified with
`git apply --check` / `--reverse --check` on every configure.

## Build and run (PowerShell)

```powershell
cd noiz2sa
cmake -S . -B build-host -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-host -j
.\build-host\noiz2sa_host_smoke.exe ..\_deps\noiz2sa
```

Expected output:

```text
NOIZ2SA HOST SMOKE: PASS frames=300
```

The runner chdir's into the pinned engine checkout so the upstream data
directories (`zako/`, `middle/`, `boss/`, `images/`, `sounds/`) resolve, runs
300 deterministic engine frames through attract mode, then verifies that the
clean-exit path wrote the versioned preference file. Determinism comes from
the shim tick source advancing one 16 ms interval per `SDL_GetTicks` call,
which pins the `srand(SDL_GetTicks())` seed chain.
