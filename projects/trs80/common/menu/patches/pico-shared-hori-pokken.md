# pico_shared HORI Pokken menu-input patch

This patch is required by Nintendo-style frontends that vendor an older
`pico_shared` snapshot without explicit support for USB VID:PID `0f0d:0092`.
Without it the controller falls through to the generic joystick path and its
physical A/B buttons are interpreted in reverse by the Settings menu.

Apply the already-integrated implementation from this workspace's
`digger/pico_shared` snapshot to the target frontend's private `pico_shared`
copy:

- add `GamePadState::updateFromHoriPokkenReport()` from `gamepad.cpp/.h`;
- recognize `0f0d:0092` as `HORI Pokken` during HID mount;
- route its reports through `updateFromHoriPokkenReport()` before the generic
  Nintendo and HID paths.

The canonical report mapping is:

| Report field | Physical control | Internal menu button |
| --- | --- | --- |
| byte 0, bit `0x04` | A | `Button::A` (Select/Confirm) |
| byte 0, bit `0x02` | B | `Button::B` (Back/Cancel) |
| byte 1, bit `0x01` | Minus | `Button::SELECT` |
| byte 1, bit `0x02` | Plus | `Button::START` |
| byte 2, low nibble | D-pad hat | Direction bits |

Do not implement this by changing `ABSWAPPED` or swapping emulator gameplay
buttons. The correction belongs only to the device decoder/menu boundary.

Validated on 2026-09-15 with a host report test and a complete HW16
bootloader-address Game Boy build. The resulting UF2 starts at `0x10080000`.
