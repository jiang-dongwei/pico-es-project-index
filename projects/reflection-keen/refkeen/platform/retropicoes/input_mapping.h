/* Translation of the normalised compat input channels into the encoding the
 * Reflection Keen engine expects to be fed.
 *
 * THE CONTRACT
 * ------------
 * The port replaces src/backend/events/be_events_sdl.c -- the eighth platform
 * TU, the one the "directly includes SDL3" criterion cannot see because it
 * reaches SDL through its own header. That TU reads SDL scancodes and looks
 * them up in src/backend/events/be_events_sdl_keymap.h, so the port's event
 * source must hand over values in the same encoding or upstream's table cannot
 * be reused verbatim.
 *
 * This module is the pure part of that: compat channel -> SDL scancode. It has
 * no hardware dependency and is unit-tested on the host (refkeen/tests/).
 *
 * WHY SDL SCANCODES AND NOT DOS SCANCODES
 * ---------------------------------------
 * Two independent places in upstream speak SDL scancodes, so a DOS-only port
 * would have to fork both:
 *
 *   * live events      -- be_events_sdl_keymap.h is indexed by SDL scancode
 *                         (283 entries, ordinal == scancode value, measured)
 *   * config file      -- id_us.c stores the ten keyboard bindings of the 2015
 *                         release as int16 SDL scancodes and converts them with
 *                         RefKeen_TranslateScancodeFromSDL2ToDOS()
 *
 * CHANNEL CHOICE
 * --------------
 * The six channels that have an engine default are bound to exactly those
 * defaults, taken from id_in.c's `KbdDefs[0] = {0x1d,0x38,0x47,0x48,0x49,0x4b,
 * 0x4d,0x4f,0x50,0x51}` (button0, button1, upleft, up, upright, left, right,
 * downleft, down, downright):
 *
 *   channel           SDL        DOS    game meaning (id_us.c control panel)
 *   ACTION (A)        LCTRL      0x1d   "Define Key for Jumping"
 *   JUMP   (B)        LALT       0x38   "Define Key for Throwing"
 *   UP/DOWN/LEFT/RIGHT arrows    0x48/0x50/0x4b/0x4d
 *   START             RETURN     0x1c   status / confirm
 *   BACK              ESCAPE     0x01   the game's own control panel
 *
 * Binding to the engine's own defaults means the game's control panel and the
 * config round-trip stay the single source of truth, and a user who rebinds
 * keys in the game's own menu keeps that binding.
 *
 * NAMING TRAP -- READ BEFORE "FIXING" THIS
 * ----------------------------------------
 * The compat channel called JUMP ends up on Alt, which Keen Dreams calls
 * "Throwing". That inversion is deliberate and required: common/menu's
 * MIGRATION.md fixes the menu convention (printed A = confirm, printed B =
 * cancel) and states that the core's gameplay mapping must NOT be changed to
 * obtain it -- the menu layer resolves ACTION/JUMP per controller profile in
 * retropicoes_menu_face_actions_from_compat(). Swapping the two game bindings
 * here to make them read correctly would violate that.
 *
 * DIAGONALS ARE NOT SYNTHESISED, ON PURPOSE
 * -----------------------------------------
 * id_in.c computes vertical and horizontal motion independently (the `up`/`down`
 * chain and the `left`/`right` chain are separate), so holding two arrow
 * scancodes already yields diagonal movement. The engine's separate diagonal
 * bindings (Home/PgUp/End/PgDn by default) are an alternative source, not a
 * requirement, so no extra keys are emitted.
 *
 * The physical Menu/Home button does NOT belong here. It never reaches the
 * engine; it is sampled as a separate rising edge by
 * retropicoes_input_menu_pressed() and goes straight to the shared Settings
 * page, exactly as in the digger and xrick ports.
 */

#ifndef REFKEEN_RETROPICOES_INPUT_MAPPING_H
#define REFKEEN_RETROPICOES_INPUT_MAPPING_H

#include <stddef.h>
#include <stdint.h>

#include "compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One SDL scancode per compat channel, so this is the worst-case list length
 * retropicoes_refkeen_scancodes_from_mask() can ever be asked to produce. */
#define RETROPICOES_REFKEEN_SCANCODE_MAX ((size_t)COMPAT_KEY_COUNT)

/* SDL scancode driven by a single channel, or SDL_SCANCODE_UNKNOWN (0) if the
 * channel is out of range. Out-of-range input is answered, not asserted: this
 * runs every frame. */
uint16_t retropicoes_refkeen_scancode_for(compat_key_t key);

/* Expands a bitmask of compat channels (bit n == channel n) into the SDL
 * scancodes to feed the engine this frame, in ascending channel order.
 * Returns the number written; never writes more than `capacity` entries.
 * A channel with no scancode is skipped rather than producing a 0 entry. */
size_t retropicoes_refkeen_scancodes_from_mask(uint32_t compat_mask,
                                               uint16_t *out, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* REFKEEN_RETROPICOES_INPUT_MAPPING_H */
