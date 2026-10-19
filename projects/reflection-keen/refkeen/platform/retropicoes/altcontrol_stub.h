/* S20-C / C12 -- the port's replacement for the altcontrol subsystem.
 *
 * This header exists so that the stub's contract is stated once, next to the
 * definitions, and can be included by the host test that pins it.
 *
 * WHY A STUB AT ALL
 *
 * platform-boundary.md 8.2 decided, on 2026-09-23, to leave the altcontrol
 * subsystem out of the port. It is the "alternative controllers" feature --
 * gamepad mapping menus, on-screen touch controls, an on-screen keyboard -- that
 * ReflectionHLE added for desktop players. Keen Dreams (1991, DOS) has none of
 * it, and this port's target is a handheld whose buttons map onto the emulated
 * keyboard anyway.
 *
 * The decision removes two TUs from the build:
 *
 *   src/kdreams/altcontroller.c                    (game half: mapping tables)
 *   src/backend/input/be_input_controller_mappings.c (backend half: the stack,
 *                                                      the UI entry points)
 *
 * and the consequence is that everything those two TUs exported has to come
 * from the port instead. Sources of "everything" is measured, not guessed:
 *
 *   39 global definitions between them (arm-none-eabi-nm --defined-only -g)
 *   32 are asked for by a TU the port does build   -> REQUIRED, defined here
 *    7 are not asked for by anything the port builds -> dropped, listed in
 *      BoundaryFacts.psd1 (AltControlStub.DroppedSymbols) with the reason
 *
 * The measurement compiles all 62 TUs of the Keen Dreams target and intersects
 * the excluded TUs' definitions with the undefined symbols of the resulting
 * objects. Text was tried first and was wrong in both directions: a header
 * declaration is not a reference, a prototype inside a .c file is not a
 * definition, and the other games' TUs (id91_11/, wolf3d/) are not part of this
 * target at all, so counting them invented 6 references no target can have.
 *
 * WHAT THE STUB DOES, AND WHAT IT DELIBERATELY DOES NOT
 *
 * Every mapping table is a zero-initialised BE_ST_ControllerMapping. That is not
 * a placeholder: BE_ST_CTRL_MAP_NONE is defined as 0 precisely so that a
 * zero-initialised mapping is a valid empty mapping (be_st.h:178). Upstream's
 * own BEL_ST_AltControlScheme_HandleAnyEntry() returns false without touching
 * its in/out state for every BE_ST_CTRL_MAP_NONE entry, so an empty table makes
 * the engine behave exactly as if the mapping existed and held nothing.
 *
 * The one deviation beyond "the tables are empty" is that the alternative
 * scheme is not merely empty but unreachable: Push/Pop/PrepareControllerMapping
 * do not move g_sdlControllerMappingActualCurr, so it never comes to point at
 * g_beStControllerMappingTextInput or ...DebugKeys, and the on-screen keyboard
 * and debug-keys UI that those pointer comparisons gate stay off. That is
 * intended -- those UIs are part of the same cut feature -- but it is a
 * behavioural difference, so it is stated rather than left implicit, and
 * refkeen/tests/test_altcontrol_stub.c asserts it.
 *
 * The reason for not tracking the mapping stack faithfully (upstream keeps an
 * 8-deep pointer stack with error paths) is that with no non-empty mapping the
 * stack can never change what the engine observes, while a copied stack would
 * add port-owned code -- and port-owned copies of engine behaviour are exactly
 * what this project tries not to accumulate.
 */

#ifndef REFKEEN_PICO_ALTCONTROL_STUB_H
#define REFKEEN_PICO_ALTCONTROL_STUB_H

#include "be_st.h"
#include "kdreams/altcontroller.h"
#include "backend/input/be_input_controller_mappings.h"

/* The six game-side mapping tables. Upstream declares each one locally, at its
 * point of use, in the TU that uses it (id_in.c:1004, id_us.c:2616, id_us.c:3799,
 * kd_demo.c:621, kd_main.c:467, kd_main.c:732, kd_play.c:1885); there is no
 * shared header. They are declared here because the situation is now the mirror
 * image: no TU the port builds defines them.
 *
 * The game only ever takes their address and passes it to
 * BE_ST_AltControlScheme_PrepareControllerMapping(), which takes a
 * const BE_ST_ControllerMapping * and, in this port, does nothing with it.
 * Nothing dereferences them, and nothing compares their addresses.
 *
 * The type is non-const because that is how the callers declare them; giving
 * the port's definitions a different qualifier would be a type mismatch across
 * TUs. Keep them writable objects -- see the storage note in the .c file. */
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_notenoughmemorytostart;
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_inackback;
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_simpledialog;
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_demoloop;
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_menu;
extern BE_ST_ControllerMapping g_ingame_altcontrol_mapping_menu_help;

/* Declared locally by its one caller (id_in.c:875), not in a header. Same
 * reason as the tables above. */
void UpdateGameplayMappingsByMousePresence(bool withmouse);

/* Upstream declares this once per game, in the gamedefs patcher table that
 * replaces the original call site (src/backend/gamedefs/be_gamedefs_kdreams.h).
 * Keen Dreams is the only game this port builds, so one declaration is enough;
 * those headers are per-game patch tables and including them here would drag in
 * machinery this file has no use for. */
void RefKeen_PrepareAltControllerScheme(void);

#endif /* REFKEEN_PICO_ALTCONTROL_STUB_H */
