# RetroPicoES common menu contract

This directory is the platform-neutral boundary for the S14 common menu. It
does not depend on Pico SDK, `pico_shared`, FatFS, an emulator core, or the
frozen `compat` v1 ABI.

The core advertises each supported system-menu capability and supplies the
matching callback. The menu exposes an item only when both are present and
keeps the agreed order across emulators. `menu_input` adds controller labels,
first-active menu ownership, release isolation and directional repeat. It also
classifies the controller names already exposed by `pico_shared`, so all menu
implementations obtain labels from one table. The old Select+Start/Start-only
gesture helpers remain temporarily for unmigrated targets, but they are not a
public frontend entry and must not be used by new ports.

`menu_state` provides reusable wrapping and page navigation plus the protected
confirmation flow for reset and Loader return. `menu_render` fixes the 320x240
RGB565 layout, theme, selection rows, paging hints, footer and confirmation
overlay while accepting a platform text callback, so existing `compat` and
`pico_shared` fonts can be reused without copying a font into this module.

`menu_runtime` coordinates core actions and optional platform enter/leave
hooks. Controls, state and settings return to the menu; resume and reset restore
gameplay; ROM-list and Loader transfers close without restarting the old game.
Failed callbacks keep the menu active and return an explicit error event.

`settings_menu` is the shared Nintendo/Game Boy-style Settings implementation.
It owns the 11-row scrolling state, value-change events, fixed
`SAVE / CANCEL / DEFAULT` action row, contextual help, controller-aware button
hints and the black-on-white 320x240 renderer. Emulator adapters supply only
their visible item descriptors, current values and core-specific actions.

Atari 2600, CHIP-8 and Lynx use the shared Settings state and renderer as well
as the common input/list primitives. `digger/pico_shared/menu.cpp` is the second
integration style and reuses the same controller-profile resolver while
retaining its established character-cell renderer and feature-rich menus.
See `MIGRATION.md` for compat and pico_shared adoption recipes.

The complete ROM-selection page and application lifecycle now live in
`../frontend`. `common/menu` supplies UI primitives; cores must not assemble a
private ROM browser from those primitives and call it a public frontend.

Host verification:

```text
cmake -S common/menu -B _build/common-menu
cmake --build _build/common-menu
ctest --test-dir _build/common-menu --output-on-failure
```

On the project Windows toolchain, the direct strict host harness is:

```powershell
& common/menu/scripts/Test-CommonMenu.ps1
```
