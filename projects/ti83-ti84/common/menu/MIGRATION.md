# Common menu migration guide

This module standardizes menu behavior, not gameplay controls. Keep every
core's gameplay mapping private and translate only menu input to
`retropicoes_ui_actions_t`.

## compat-based core

1. Link the `common/menu` and `common/frontend` targets, or add their complete
   source lists through the shared CMake helper. Do not copy individual source
   lists into each new core.
2. Provide a small adapter like `chip8/platform/retropicoes/menu_adapter.c`.
   Use `retropicoes_menu_face_actions_from_compat()` with the active controller
   profile so a printed A is `CONFIRM` and a printed B is `CANCEL` on both
   normalized Xbox input and direct Nintendo/HORI or NES/SNES input. Map
   Select/Back to `MENU`, Start/Plus to `AUX`, and directions directly. Do not
   change the core's gameplay mapping to obtain this menu convention.
3. Declare only capabilities that have non-null callbacks. Never expose a
   placeholder State or Settings row. The five-row system menu is only a
   lifecycle/callback prototype, not the final emulator UI.
4. Feed the normalized physical Menu/Guide edge directly into `MENU` before
   gameplay consumes it. Migrated product menus must not require Select+Start
   or retain a long-Start fallback; those legacy gestures are not part of the
   unified interaction contract.
5. Pause audio/core scheduling in the lifecycle `enter_menu` hook and restore
   them in `leave_menu`. ROM List and Return Loader are terminal transfers and
   intentionally do not resume the old core.
6. Set `RETROPICOES_ENABLE_DIRECT_LOADER_RETURN=0` for the migrated target so
   the platform cannot bypass the confirmed Return Loader action.

## pico_shared core

1. Link `menu_input.c` and include `common/menu` from the shared target.
2. Convert the normalized menu mask with
   `retropicoes_menu_actions_from_standard_pad()` and use logical actions in
   the top-level ROM menu.
3. Track the first controller that operates the menu with
   `retropicoes_menu_owner_update()`. Resolve its USB name through
   `retropicoes_menu_controller_profile_from_name()` and obtain prompts from
   the common label table.
4. Preserve established artwork, recent-ROM, save-state and settings screens.
   Move them behind the public system-menu capabilities incrementally rather
   than replacing the mature `pico_shared/menu.cpp` implementation.

## Product-level migration target

The target experience is the mature pico-snesPlus, pico-smsplus and
pico-peanutGB flow: launch into a ROM browser, open a full Settings page from
the browser or from gameplay, edit a working copy with fixed SAVE/CANCEL/DEFAULT
actions, and persist only supported settings per emulator. Preserve directory
navigation, metadata, recently played games, controller tests and core-specific
options where the core implements them.

The public ROM page is `common/frontend/rom_browser`; using only
`menu_state`/`menu_render` to construct a core-private lookalike does not satisfy
the migration contract. The public application transition owner is
`common/frontend/frontend_shell`; a core frame callback must report a request
instead of nesting Settings or ROM-browser execution inside the core.

Migrate and accept one emulator at a time. Do not roll the five-row prototype
out to another emulator as its final UI.

## Capability checklist

The prototype callback order is Resume, Controls, State, Settings, Reset, ROM
List and Return Loader. A product adapter may present these inside the full
Settings page instead of as a separate top-level menu. Unsupported entries are
hidden. Reset and Return Loader always pass through confirmation. A failed
callback must leave the menu open and report an error; it must never silently
continue or reboot.

Before hardware testing, run:

```powershell
& common/menu/scripts/Test-CommonMenu.ps1
& new-core-template/scripts/test-template.ps1
```

The representative RP2350 build, controller matrix, audio-resume check and
Loader-return confirmation belong to the hardware acceptance step.
