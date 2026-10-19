/* Expansion of one frame's compat key edges into the key events the Reflection
 * Keen engine has to be fed, in the only encoding its own table is indexed by.
 *
 * WHERE THIS SITS
 * ---------------
 * The port replaces src/backend/events/be_events_sdl.c (the replacement itself
 * is refkeen/platform/retropicoes/be_events_port.c). The keyboard path of that
 * TU is four hops, each owned by a different party:
 *
 *   device        -> compat mask        (compat.h; the port's input backend)
 *   compat mask   -> SDL scancode        (input_mapping.c, S20-C/C8)
 *   SDL scancode  -> DOS key event       (be_events_sdl_keymap.h, positional)
 *   DOS key event -> engine              (BEL_ST_HandleEmuKeyboardEvent, be_input.c)
 *
 * This module is the second step plus the edge arithmetic. It touches no device,
 * no clock and no engine state, which is what makes it unit-testable on the host
 * (refkeen/tests/test_event_source.c).
 *
 * EDGES, NOT LEVELS
 * -----------------
 * BEL_ST_HandleEmuKeyboardEvent() is an *event* entry point: be_input.c keeps
 * g_sdlEmuKeyboardStateByScanCode[] and drops a reported state that equals the
 * stored one. So the port has to report transitions, and the previous mask must
 * be remembered by the caller -- exactly as the xrick/digger/sdlpop ports do
 * with their retropicoes_input_edge_mask(). This module deliberately keeps no
 * state of its own, so a caller that forgets to store the mask fails visibly
 * (every frame reports a press) rather than subtly.
 *
 * RELEASES ARE EMITTED BEFORE PRESSES
 * -----------------------------------
 * One channel can never be both released and pressed in a single step, so the
 * order between *different* channels is unobservable by the engine. It is pinned
 * anyway, for one reason: if a future edit ever maps two channels onto the same
 * scancode, the last event for that scancode wins. Emitting releases first makes
 * the last event the one consistent with the current mask, so the engine cannot
 * be left holding a key that is no longer held. The host test asserts this.
 *
 * WHAT IS DELIBERATELY ABSENT
 * ---------------------------
 * No key-repeat emulation. Upstream keeps it in BE_ST_PollEvents() and reads
 * shared state owned by be_input.c, so it stays there instead of being
 * reinvented once per frame here.
 *
 * No diagonal synthesis. id_in.c runs its vertical and horizontal chains
 * independently, so holding two directions already yields a diagonal; the
 * engine's own diagonal bindings are an alternative source, not a requirement.
 *
 * No menu handling. The physical Menu/Home button never reaches the engine:
 * it is sampled as its own rising edge (retropicoes_input_menu_pressed() in the
 * sibling ports) and goes straight to the shared Settings page.
 */

#ifndef REFKEEN_RETROPICOES_EVENT_SOURCE_H
#define REFKEEN_RETROPICOES_EVENT_SOURCE_H

#include <stddef.h>
#include <stdint.h>

#include "input_mapping.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One keyboard transition, in the encoding be_events_sdl_keymap.h is indexed by.
 * `scancode` is an SDL scancode, `pressed` is non-zero for a key going down. */
typedef struct {
    uint16_t scancode;
    uint8_t pressed;
} retropicoes_refkeen_key_event_t;

/* Worst case is one release per channel plus one press per channel, but a single
 * channel contributes to only one of the two lists per step, so the bound is the
 * same as the scancode list's. */
#define RETROPICOES_REFKEEN_KEY_EVENT_MAX RETROPICOES_REFKEEN_SCANCODE_MAX

/* Writes the transitions between `previous_mask` and `current_mask` into `out`,
 * in ascending compat channel order, releases first (see the header comment).
 * Bit n is channel n; bits at or above COMPAT_KEY_COUNT are ignored, and a
 * channel with no scancode is skipped rather than emitted as scancode 0.
 *
 * Returns the number of events written; never more than `capacity`, and 0 when
 * `capacity` is 0 or `out` is NULL. Out-of-range input is answered rather than
 * asserted: this runs every frame. */
size_t retropicoes_refkeen_key_events_from_masks(
    uint32_t current_mask, uint32_t previous_mask,
    retropicoes_refkeen_key_event_t *out, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* REFKEEN_RETROPICOES_EVENT_SOURCE_H */
