# RetroPicoES common frontend

This directory is the platform-neutral owner of the emulator-facing product
flow. It depends on `common/menu`, but not on the Pico SDK, FatFS,
`pico_shared`, an emulator core, or a native game.

`rom_browser` owns the shared ROM-selection state, paging, A/Open, B/Back,
Menu/Settings events and the Nintendo-style 320x240 layout. A core supplies
only a stable item count, item descriptors and the current path. When a source
is empty, the browser keeps the same screen and injects the clearly-labelled
`DEMO / TEST GAME (SIMULATED)` entry required by the product contract.

`frontend_shell` owns the only supported high-level flow:

```text
ROM browser -> running game -> Settings
     ^                              |
     +--------- ROM browser --------+
                    or
               Return Loader
```

The same `SETTINGS` state is entered from the browser and from gameplay. Only
the lifecycle callbacks differ: gameplay is paused before Settings and is
resumed, reset or stopped after the user chooses an action.

`settings_host` now owns the reusable Settings polling/navigation loop. A
core adapter supplies the option list, values, rendering data and event
callbacks; it must not create a second Settings navigation loop. This host is
tested independently and has not yet been connected to firmware.

The first migration target is Xbox/XInput. Other existing device drivers stay
in the tree unchanged; this module consumes logical UI actions and therefore
does not duplicate hardware mappings.

Host verification:

```powershell
& common/frontend/scripts/Test-CommonFrontend.ps1
```

ROM-browser adopters exist, but the shared lifecycle and Settings host are
not yet wired into firmware. Treat those migrations as separate acceptance
steps rather than assuming host tests prove device behavior.
