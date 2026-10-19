/* The port's use of the shared public frontend. See frontend.h for why this is
 * the port's own TU, why the pump runs from BE_ST_PollEvents(), and what is
 * deliberately absent. */

#include <stddef.h>

#include "be_events_port.h"
#include "compat.h"
#include "frontend.h"

/* One shell for the life of the process, like the sibling ports. Static: the
 * three shared components take it by pointer in the lifecycle calls, and nothing
 * outside this TU may rewrite the screen state behind the shell's own back. */
static retropicoes_frontend_shell_t g_shell;

/* Set only by boot(), and read by pump(). It is what keeps a Menu press during
 * the engine's pre-graphics text screens (InitGame's piracy screen, the /VER
 * and /? banners) from opening a page that would then render over a text mode
 * the port has not established yet. */
static int g_booted;

static int g_paused;
static int g_quitRequested;
static int g_loaderRequested;
static int g_shutdownDone;

/* ---------------------------------------------------------------------------
 * The six lifecycle callbacks.
 *
 * Three of them are inert on purpose and saying so is more useful than leaving
 * them out, because the shared shell treats "callback returned UNAVAILABLE"
 * and "callback returned OK" as different program states.
 * ------------------------------------------------------------------------- */

/* Keen Dreams has nothing to start: its data was checked before boot, and
 * kdreams_exe_main() is already running by the time the shell exists. Returning
 * OK is what makes the shell's screen RUNNING, which is the state every later
 * question reads. */
static retropicoes_menu_result_t shell_start(
    void *context, size_t source_index, retropicoes_rom_item_kind_t item_kind)
{
    (void)context; (void)source_index; (void)item_kind;
    return RETROPICOES_MENU_RESULT_OK;
}

/* The engine is blocked inside BE_ST_PollEvents() while the page is open, so
 * "paused" here means the audio path, not the engine's own loop. Exposing the
 * flag to the audio path is the port's half of the contract; the audio backend
 * (C13) is the other half. */
static retropicoes_menu_result_t shell_pause(void *context)
{
    (void)context;
    g_paused = 1;
    return RETROPICOES_MENU_RESULT_OK;
}

static retropicoes_menu_result_t shell_resume(void *context)
{
    (void)context;
    g_paused = 0;
    return RETROPICOES_MENU_RESULT_OK;
}

/* C1: no Reset. The shared page still offers the code path, and answering
 * UNAVAILABLE is how the shell learns to stay on the settings screen instead of
 * resuming a game that was never restarted. */
static retropicoes_menu_result_t shell_reset(void *context)
{
    (void)context;
    return RETROPICOES_MENU_RESULT_UNAVAILABLE;
}

/* "Quit game". Latching is the whole action: this callback runs inside the
 * engine's own frame, and tearing the engine down here would pull its managers
 * out from under the frame that is still executing. The runtime leaves the loop
 * on the next pump() that reports the QUIT bit. */
static retropicoes_menu_result_t shell_stop(void *context)
{
    (void)context;
    g_quitRequested = 1;
    return RETROPICOES_MENU_RESULT_OK;
}

static retropicoes_menu_result_t shell_loader(void *context)
{
    (void)context;
    g_quitRequested = 1;
    g_loaderRequested = 1;
    return RETROPICOES_MENU_RESULT_OK;
}

/* ---------------------------------------------------------------------------
 * Public surface.
 * ------------------------------------------------------------------------- */

retropicoes_frontend_settings_action_t retropicoes_refkeen_frontend_map_action(
    retropicoes_native_game_action_t action)
{
    /* Three destinations, three answers, no aliasing. The names that invite a
     * mistake -- the page's QUIT and the shell's ROM_BROWSER -- are the pair
     * worth reading twice: a native game's ROM list is empty, so "stop playing
     * this game" can only mean "there is no game running any more". */
    if (action == RETROPICOES_NATIVE_GAME_LOADER)
        return RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER;
    if (action == RETROPICOES_NATIVE_GAME_QUIT)
        return RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER;
    return RETROPICOES_FRONTEND_SETTINGS_RESUME;
}

int retropicoes_refkeen_frontend_boot(void)
{
    const retropicoes_frontend_lifecycle_t lifecycle = {
        RETROPICOES_FRONTEND_LIFECYCLE_VERSION,
        sizeof(retropicoes_frontend_lifecycle_t),
        NULL,
        shell_start, shell_pause, shell_resume, shell_reset,
        shell_stop, shell_loader
    };

    /* The page reads its own file and reports the saved flags, so it has to run
     * before the first frame, and before anything asks whether scanlines are on.
     * settings.dat is created by the page on its first save; a missing file is
     * not an error, it is the defaults. */
    retropicoes_native_game_settings_init(RETROPICOES_REFKEEN_SETTINGS_PATH);
    retropicoes_video_set_scanlines(
        retropicoes_native_game_scanlines_enabled());

    if (!retropicoes_frontend_shell_init(&g_shell, &lifecycle))
        return RETROPICOES_REFKEEN_FRONTEND_BOOT_SHELL_INIT;

    /* RETROPICOES_ROM_ITEM_DEMO, not ROM: a native game has no file behind it,
     * and the sibling ports pass the same value for the same reason. */
    if (retropicoes_frontend_shell_launch(&g_shell, 0u,
            RETROPICOES_ROM_ITEM_DEMO) != RETROPICOES_MENU_RESULT_OK)
        return RETROPICOES_REFKEEN_FRONTEND_BOOT_SHELL_LAUNCH;

    g_booted = 1;
    return RETROPICOES_REFKEEN_FRONTEND_BOOT_OK;
}

unsigned retropicoes_refkeen_frontend_pump(void)
{
    unsigned bits = RETROPICOES_REFKEEN_FRONTEND_IDLE;

    if (!g_booted)
        return bits;

    /* One read, one page. This is the consumed rising edge the header explains;
     * calling the accessor anywhere else in this TU would consume it before it
     * is acted on, and a reader of the runtime's side of the contract would have
     * no way to see that from here. */
    if (retropicoes_input_menu_pressed() &&
        retropicoes_frontend_shell_open_settings(&g_shell) ==
            RETROPICOES_MENU_RESULT_OK)
    {
        /* The page polls the device itself, so no compat_poll_input() here --
         * and it presents each of its frames, which is why the port needs no
         * frame save/restore around it (frontend.h). */
        const retropicoes_native_game_action_t action =
            retropicoes_native_game_settings_run();

        /* The player may have toggled Scanlines while the page was open. */
        retropicoes_video_set_scanlines(
            retropicoes_native_game_scanlines_enabled());

        (void)retropicoes_frontend_shell_settings_action(
            &g_shell, retropicoes_refkeen_frontend_map_action(action));

        bits |= RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN;
    }

    /* Reported on every later call as well, not only on the one that latched
     * them: a caller that misses the first is otherwise a caller that never
     * leaves. Reading them costs the caller nothing, and the flags never clear
     * because a quit cannot be undone. */
    if (g_quitRequested)
        bits |= RETROPICOES_REFKEEN_FRONTEND_QUIT;
    if (g_loaderRequested)
        bits |= RETROPICOES_REFKEEN_FRONTEND_LOADER;

    return bits;
}

retropicoes_menu_result_t retropicoes_refkeen_frontend_finished(void)
{
    if (!g_booted)
        return RETROPICOES_MENU_RESULT_INVALID;
    if (g_shell.screen != RETROPICOES_FRONTEND_RUNNING)
        return RETROPICOES_MENU_RESULT_INVALID;
    return retropicoes_frontend_shell_game_finished(&g_shell);
}

void retropicoes_refkeen_frontend_shutdown(void)
{
    if (g_shutdownDone)
        return;
    g_shutdownDone = 1;
    /* Empty for most data versions: kd_main.c only registers the app quit
     * callback for BE_GAMEVER_KDREAMS2015 (kd_main.c:522), and the helper
     * answers "no callback" without complaint. That is upstream's decision and
     * not something to paper over here -- for the other versions the config is
     * never written on exit, which is what the DOS originals did. */
    retropicoes_refkeen_event_source_quit();
}

int retropicoes_refkeen_frontend_is_paused(void)
{
    return g_paused;
}

int retropicoes_refkeen_frontend_quit_requested(void)
{
    return g_quitRequested;
}

int retropicoes_refkeen_frontend_loader_requested(void)
{
    return g_loaderRequested;
}
