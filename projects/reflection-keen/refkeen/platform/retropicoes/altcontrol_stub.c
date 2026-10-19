/* S20-C / C12 -- definitions for the altcontrol subsystem the port does not
 * build. The reasoning, the scope cut and the measured symbol set are in
 * altcontrol_stub.h; this file is the mechanical half.
 *
 * Two TUs are replaced here, and the order below follows them so a reviewer can
 * diff the pair against upstream:
 *
 *   src/kdreams/altcontroller.c                       -> 22 of its 24 exports
 *   src/backend/input/be_input_controller_mappings.c  -> 10 of its 15 exports
 *
 * STORAGE
 *
 * Nine BE_ST_ControllerMapping objects are needed (6 game tables, the two
 * identity-compared backend mappings, and the current mapping) at 6,204 B each,
 * because BE_MAX_KEY_ID is 250 and every mapping carries keys[250]. That is
 * 55,836 B of zero-initialised RAM, against the 74,927 B of writable data the
 * two replaced TUs cost on this target -- measured per object, because getting
 * the second half wrong is easy: 56,244 B .data + 35 B .bss for
 * altcontroller.c, and 4 B .data + 18,644 B .bss for
 * be_input_controller_mappings.c (those 4 bytes are its initialised
 * g_sdlControllerMappingActualCurr pointer, and leaving them out is how the
 * total ends up 4 B short of the recorded 74,927).
 *
 * So the stub is not free, and it is written plainly rather than cleverly:
 * distinct non-const objects with the types the callers declare. The
 * alternatives were considered and rejected:
 *
 *   * making them const would move ~37 KiB to flash, but upstream's callers
 *     declare them non-const, so the port would be defining a different type
 *     from its own declarations -- a latent bug traded for RAM.
 *   * aliasing the six game tables onto one object (nothing compares their
 *     addresses, so it would be unobservable) would save ~31 KiB more, but it
 *     makes the storage layout surprising for a saving the PSRAM budget can
 *     afford.
 *
 * These nine symbols belong on the PSRAM placement list in S20-D, like the
 * other large zero-initialised objects.
 */

#include "altcontrol_stub.h"

/* ------------------------------------------------------------------------- */
/* Game half -- replaces src/kdreams/altcontroller.c                         */
/* ------------------------------------------------------------------------- */

/* BE_ST_CTRL_MAP_VALUESET destinations and the keyboard-override flags. These
 * are REAL state, not placeholders: game TUs read and write them (id_in.c,
 * kd_play.c), and they are only meaningful while a mapping points at them,
 * which in this port is never. Zero is the correct initial value -- upstream
 * zero-initialises them too, and the [2] elements are the jump/throw pair
 * (id_us.c:1587 calls Alt "Throwing" and Ctrl "Jumping"; do not swap them).
 *
 * They are small (6 int + 7 bool), and they are deliberately not dropped: the
 * game reads them on paths that are still live. */
int g_binding_value_button[2], g_binding_value_stats,
    g_binding_value_up, g_binding_value_down, g_binding_value_left, g_binding_value_right;

bool g_keybind_used_button[2], g_keybind_used_stats,
     g_keybind_used_up, g_keybind_used_down, g_keybind_used_left, g_keybind_used_right;

/* The six tables the game hands to PrepareControllerMapping(). See the header
 * for why they are distinct writable objects and not aliases.
 *
 * g_ingame_altcontrol_mapping_gameplay and ..._funckeys are NOT here: they are
 * referenced only from inside the replaced altcontroller.c (by
 * RefKeen_PrepareAltControllerScheme), so no TU the port builds asks for them.
 * They are listed as dropped in BoundaryFacts.psd1, not overlooked. */
BE_ST_ControllerMapping g_ingame_altcontrol_mapping_notenoughmemorytostart;
BE_ST_ControllerMapping g_ingame_altcontrol_mapping_inackback;
BE_ST_ControllerMapping g_ingame_altcontrol_mapping_simpledialog;
BE_ST_ControllerMapping g_ingame_altcontrol_mapping_demoloop;
BE_ST_ControllerMapping g_ingame_altcontrol_mapping_menu;
BE_ST_ControllerMapping g_ingame_altcontrol_mapping_menu_help;

/* Called once by be_gamedefs_prep.c through the Keen Dreams patch table. Its
 * upstream body builds the mapping tables and wires the on-screen controls; the
 * port's tables are empty by construction, so there is nothing to build. */
void RefKeen_PrepareAltControllerScheme(void)
{
}

/* Called from id_in.c (582) / id_in.c (876) when the mouse appears or vanishes,
 * to swap in mouse-aware gamepad mappings. No mappings exist here, so neither
 * has anything to do. */
void FinalizeControlPanelMappingsByMousePresence(bool withmouse)
{
	(void)withmouse;
}

void UpdateGameplayMappingsByMousePresence(bool withmouse)
{
	(void)withmouse;
}

/* Called from kd_play.c (250, 1978) when the gameplay/menu context is set up. */
void PrepareGamePlayControllerMapping(void)
{
}

/* ------------------------------------------------------------------------- */
/* Backend half -- replaces src/backend/input/be_input_controller_mappings.c */
/* ------------------------------------------------------------------------- */

/* The current mapping, and the port's single empty one.
 *
 * The pointer is non-NULL from the very first instruction the engine can run:
 * it is initialised statically, not by BE_ST_AltControlScheme_Reset(). That
 * matters, because be_video_ui.c dereferences it (->mbuttons, ->pbuttons,
 * ->defaultMapping, ->showUi, ->touchMappings, ->absoluteFingerPositioning,
 * ->grabMouse) without a null check, and be_startup.c's call to Reset() happens
 * well after the first mapping lookup could.
 *
 * An all-zero mapping is a valid one: every mapClass field reads back as
 * BE_ST_CTRL_MAP_NONE, which is why that enumerator is pinned to 0. */
static BE_ST_ControllerMapping g_picoAltControlSchemeDefaultMapping;

const BE_ST_ControllerMapping *g_sdlControllerMappingActualCurr =
	&g_picoAltControlSchemeDefaultMapping;

/* Identity markers, not mappings. Upstream compares pointers against these two
 * addresses in be_input_ui.c (40, 76, 107, 138), be_video_ui.c and -- in the TU
 * the port replaced in C9 -- be_events_sdl.c, to decide whether the on-screen
 * keyboard or the debug-keys UI is showing.
 *
 * They have to exist and have to be distinct addresses, or those comparisons
 * cannot be written at all. They are never current in this port (see below), so
 * every such comparison is false and both UIs stay off. */
BE_ST_ControllerMapping g_beStControllerMappingTextInput;
BE_ST_ControllerMapping g_beStControllerMappingDebugKeys;

/* Called by be_video_ui.c whenever the engine wants the mapping's binary state
 * cleared (upstream: simulate key releases so nothing sticks). With no mapping
 * holding anything, no state can be stuck, and both release helpers it would
 * call are in the replaced TU. */
void BEL_ST_AltControlScheme_ClearBinaryStates(void)
{
}

/* The one mapping-entry point the engine calls directly: be_video_ui.c reaches
 * it through the header (247, 1168, 1197, 1262, 1293, 1367, 1458) for
 * mbuttons, defaultMapping, pbuttons and the touch-control mappings.
 *
 * Returning false without touching *lastMappingStatePtr is exactly what
 * upstream does for BE_ST_CTRL_MAP_NONE -- that arm returns before the function
 * assigns the new state, so a zeroed mapping and this stub are
 * indistinguishable from the caller's side. */
bool BEL_ST_AltControlScheme_HandleEntry(const BE_ST_ControllerSingleMap *map,
                                         int32_t value, int32_t *lastMappingStatePtr)
{
	(void)map;
	(void)value;
	(void)lastMappingStatePtr;
	return false;
}

/* The mapping stack. Game code brackets menus and dialogs with Push/Pop
 * (id_in.c:1004, id_us.c:1270/2618/3801, kd_demo.c:650/807, kd_main.c:469/733,
 * kd_play.c:1887), and be_startup.c calls Reset() once, right before the game's
 * main function runs.
 *
 * Upstream keeps an 8-deep stack of mapping pointers and errors out on
 * overflow/underflow. Tracking it faithfully would mean copying that stack into
 * port-owned code in order to select between mappings that are all empty, so
 * the stack is not reproduced; the calls are balanced by the game and each is
 * inert here. The consequences are stated in the header: nothing moves the
 * current mapping, so the on-screen keyboard is unreachable.
 *
 * Reset keeps its meaning as "make sure a valid mapping is current", which is
 * the part be_startup.c depends on. */
void BE_ST_AltControlScheme_Push(void)
{
}

void BE_ST_AltControlScheme_Pop(void)
{
}

void BE_ST_AltControlScheme_Reset(void)
{
	g_sdlControllerMappingActualCurr = &g_picoAltControlSchemeDefaultMapping;
}

void BE_ST_AltControlScheme_PrepareControllerMapping(const BE_ST_ControllerMapping *mapping)
{
	/* Deliberately does not adopt the argument. See the header. */
	(void)mapping;
}

/* Called from id_us.c to feed an absolute pointer position (touch input). This
 * port has no touch input, and the two globals it would update
 * (g_sdlVirtualMouseCursorState, defined in be_input.c) are also written by the
 * path be_video.c uses for the mouse. Leaving them alone is the correct
 * behaviour for a device that never produces an absolute position. */
void BE_ST_AltControlScheme_UpdateVirtualMouseCursor(int x, int y)
{
	(void)x;
	(void)y;
}
