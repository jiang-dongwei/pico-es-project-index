/* The port's use of the shared public frontend (S20-C / C11).
 *
 * WHAT THIS IS AND WHY IT IS THE PORT'S OWN TU
 * -------------------------------------------
 * Keen Dreams is a single native game, so it adopts the native-game shape of
 * the public frontend -- exactly like digger and xrick:
 *
 *   frontend_shell.c      owns the screen state machine (browser / running /
 *                         settings / stopped) and calls six lifecycle callbacks
 *   native_game_settings.c  owns the one Settings page a native game shows
 *                         (Scanlines / FPS overlay / Audio / Resume / Quit /
 *                         Return to Loader)
 *   settings_host.c       owns the list-and-action navigation of that page
 *
 * None of those may be edited by a port (they are shared), and none of them
 * knows what a "Keen Dreams" is: every per-core decision arrives as a callback
 * or as a build-system entry. This TU is where the port supplies those
 * decisions. It is the only file in the port that names the public frontend.
 *
 * WHY THE MENU IS PUMPED FROM BE_ST_PollEvents()
 * ---------------------------------------------
 * The sibling ports (digger, xrick) own their frame loop, so they call the
 * shared Settings page at a frame boundary of their own choosing. This port
 * does not: upstream's kdreams_exe_main() owns the loop (DemoLoop in
 * kd_demo.c), and the port may not edit an upstream TU. The only per-frame
 * call-out the port owns is BE_ST_PollEvents(), from be_events_port.c --
 * upstream calls it from be_input.c, be_timing.c and id_us.c, i.e. several
 * times per frame, always between engine work. That is where the pump below is
 * invoked, and why the pump reports what happened instead of acting on its own:
 * only the caller knows it is inside BE_ST_PollEvents() and must therefore
 * reseed the engine's view of the keyboard afterwards.
 *
 * ONE MENU PRESS, ONE SETTINGS PAGE
 * ---------------------------------
 * retropicoes_input_menu_pressed() is a consumed rising edge, not a level --
 * that is what makes digger's and xrick's identical one-shot call pattern work
 * in production, and it is what this port relies on too: BE_ST_PollEvents()
 * runs several times per frame, and a level would open the page once per call.
 * The second half of the same property matters just as much: the Settings page
 * itself closes on RETROPICOES_UI_MENU (settings_menu.c:125), which
 * native_game_settings.c feeds from that same accessor, so a *level* would
 * close the page on its first frame. Both halves are the same assumption and
 * neither is verifiable without the RetroPicoES runtime; see frontend-adoption.md
 * 6 for the coordination item.
 *
 * WHAT THE PORT DELIBERATELY DOES NOT DO
 * --------------------------------------
 * No page-code copies and no page fix-ups: the port does not allocate a frame
 * save buffer around the page or restore the picture afterwards. xrick and
 * digger do, because they present a picture and then keep it on screen while
 * the page is open. Keen Dreams redraws its whole view every frame from its own
 * video buffers, so the next engine frame after the page returns overwrites
 * whatever the page left -- and it is upstream's own VW_ code that does it. A
 * save buffer would cost about 150 KB of a budget that is already 3.23 x SRAM.
 *
 * No Reset. C1 already decided this (input-contract.md 2): the shared page has
 * no Reset semantics for a native game, and the capability matrix says so. The
 * reset callback exists only because the lifecycle struct requires one, and it
 * answers UNAVAILABLE.
 *
 * No ROM browser. A single native game has no ROM to browse, and the shared
 * adoption gate asserts that refkeen must not link rom_browser.c. This TU
 * references no retropicoes_rom_* symbol, and the gate checks that against the
 * compiled object rather than against this sentence.
 */

#ifndef REFKEEN_RETROPICOES_FRONTEND_H
#define REFKEEN_RETROPICOES_FRONTEND_H

#include "frontend_hook.h"
#include "frontend_shell.h"
#include "native_game_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * The RetroPicoES runtime, as this port uses it.
 *
 * These are supplied by the board-side backend, not by this repository's
 * portable layer. The sibling ports declare the same four names in their own
 * platform/retropicoes/{input_backend,video_backend,runtime}.h copies -- there
 * is no shared header for them -- so the declarations live here, and
 * Test-FrontendAdapterContract.ps1 cross-checks their signatures against
 * digger's and xrick's copies instead of letting this port invent an interface.
 * ------------------------------------------------------------------------- */

/* Menu/Home, sampled as its own rising edge and consumed by this read. Never
 * forwarded to the engine: it is not one of the compat channels. */
int retropicoes_input_menu_pressed(void);

/* Scanlines are a display property of the runtime, not of the engine. */
void retropicoes_video_set_scanlines(int enabled);

/* Read once at startup, and acted on once at teardown, by the port's main(). */
int retropicoes_launched_from_bootloader(void);
void retropicoes_return_to_bootloader(void);

/* ---------------------------------------------------------------------------
 * The public frontend, from the port's side.
 *
 * pump() and the bits it returns are in frontend_hook.h, which this header
 * includes: the C9 event TU reaches for them too, and must be able to do so
 * without acquiring this file's dependencies. The banner above explains why the
 * pump runs from there rather than from a loop the port owns.
 * ------------------------------------------------------------------------- */

/* Where boot() got to, so a failure on the board says which step it was
 * without a debugger. Mirrors the sibling ports' numeric exit codes. */
typedef enum retropicoes_refkeen_frontend_boot_stage {
    RETROPICOES_REFKEEN_FRONTEND_BOOT_OK = 0,
    RETROPICOES_REFKEEN_FRONTEND_BOOT_SHELL_INIT = 1,
    RETROPICOES_REFKEEN_FRONTEND_BOOT_SHELL_LAUNCH = 2
} retropicoes_refkeen_frontend_boot_stage_t;

/* The public settings file for this core. /SAVES/refkeen is the app-data root
 * the path mapping gave g_be_appDataPath (path-mapping.md 3); the game's own
 * CONFIG.KDR / KDREAMS.CFG lives one level deeper, per data version, and is
 * upstream's file, not this one's. */
#define RETROPICOES_REFKEEN_SETTINGS_PATH "/SAVES/refkeen/settings.dat"

/* Settings page first, then the display property, then the shell, then launch.
 * Order matters: the shell's launch calls the start callback, and the page's
 * saved flags must be in effect before the first frame is presented. Returns
 * one of retropicoes_refkeen_frontend_boot_stage_t: OK, or the stage that
 * failed. The stage code is the whole answer -- after OK the shell is RUNNING
 * by construction, and after a failure there is nothing to ask it. */
int retropicoes_refkeen_frontend_boot(void);

/* The engine's main has returned. Closes the lifecycle out. Returns INVALID if
 * boot() never succeeded or the game was not running. */
retropicoes_menu_result_t retropicoes_refkeen_frontend_finished(void);

/* Run the engine's own quit callback (Keen Dreams registers ShutdownId, which
 * saves the configuration) exactly once. The port's main() calls this when the
 * game loop has been left, whichever way it was left.
 *
 * be_events_port.h owns the underlying one-shot helper; this wrapper adds the
 * second half of the same guarantee, because the loader-return path and the
 * quit path are separate in the runtime and both reach teardown. */
void retropicoes_refkeen_frontend_shutdown(void);

/* Pure. The page's three-destination answer is not the shell's five-way one,
 * and the two that look similar are not the same: the page's "Quit game" means
 * "stop this native game", which for a game with no ROM list is the shell's
 * ROM_BROWSER (game inactive, nothing left to play), not RESUME. Split out so
 * the mapping is testable with no shell, no engine and no device. */
retropicoes_frontend_settings_action_t retropicoes_refkeen_frontend_map_action(
    retropicoes_native_game_action_t action);

/* Read by the audio path while the Settings page is open: the lifecycle's pause
 * callback sets it, resume clears it. The page can also mute from its own
 * Audio row, which is a separate flag owned by native_game_settings.c; this one
 * is "the player is in a menu", not "the player wants no sound". */
int retropicoes_refkeen_frontend_is_paused(void);

/* Latched by the stop / return_loader callbacks. The runtime reads them after
 * leaving the game loop; they never clear, because a quit is not undoable. */
int retropicoes_refkeen_frontend_quit_requested(void);
int retropicoes_refkeen_frontend_loader_requested(void);

#ifdef __cplusplus
}
#endif

#endif /* REFKEEN_RETROPICOES_FRONTEND_H */
