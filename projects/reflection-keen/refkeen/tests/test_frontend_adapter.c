/* Host test for the port's use of the public frontend (S20-C / C11).
 *
 * WHAT IS REAL HERE, AND WHAT IS NOT
 * ----------------------------------
 * frontend_shell.c is the REAL shared component: this test links it, not a
 * copy, so every screen transition asserted below is the one the board runs.
 * The engine, the device and the shared Settings page are replaced by spies,
 * because none of the three exists on the host:
 *
 *   compat / the runtime   -- no panel, no SD card, no Menu button
 *   native_game_settings   -- the page itself is shared code with its own tests
 *                            (common/frontend/tests/test_settings_host.c); what
 *                            this test cares about is what the PORT does with
 *                            the page's three-way answer
 *
 * So the assertions are about the port's decisions, on top of the real state
 * machine: which callbacks fire, in what order, what is latched, what is
 * applied before the first frame, and -- the one that is easiest to get wrong
 * silently -- that a boot failure leaves the per-frame hook inert.
 *
 * The one assumption the spies encode is that the runtime's Menu accessor is a
 * CONSUMED rising edge, not a level. That is what makes the sibling ports'
 * identical call pattern work in production (frontend.h explains), and it is
 * the reason the port can call it from a function the engine invokes several
 * times per frame. The fake therefore reports a press exactly once per
 * requested press, and test_one_menu_press_runs_the_page_once asserts the
 * consequence rather than restating the assumption.
 */

#include <stdio.h>
#include <string.h>

#include "be_events_port.h"
#include "frontend.h"
#include "frontend_hook.h"
#include "frontend_shell.h"

/* ---------------------------------------------------------------------------
 * Spies standing in for the device, the runtime and the shared page.
 * ------------------------------------------------------------------------- */

static int g_settingsInitCalls;
static char g_settingsInitPath[128];

static int g_menuArm;      /* a press is waiting to be consumed */
static int g_menuReads;    /* times the accessor was called at all */

static int g_scanlinesArm; /* what the saved flags say */
static int g_scanlinesSetCalls;
static int g_scanlinesLastValue;

static int g_pageRuns;
static retropicoes_native_game_action_t g_pageAnswer;
/* Snapshot of the port's pause flag taken while the page is open, i.e. from
 * inside the only window in which the audio path would consult it. */
static int g_pausedSeenByPage;

static int g_quitHookCalls;

void retropicoes_native_game_settings_init(const char *save_path)
{
    ++g_settingsInitCalls;
    if (save_path != NULL) {
        strncpy(g_settingsInitPath, save_path, sizeof(g_settingsInitPath) - 1u);
        g_settingsInitPath[sizeof(g_settingsInitPath) - 1u] = '\0';
    }
}

int retropicoes_native_game_scanlines_enabled(void)
{
    return g_scanlinesArm;
}

retropicoes_native_game_action_t retropicoes_native_game_settings_run(void)
{
    ++g_pageRuns;
    /* The real page reads the saved flags again after its own rows changed
     * them; the port then has to re-apply scanlines. Move the flag here so the
     * port's re-read is observable. */
    g_scanlinesArm = 0;
    /* Read the port's pause flag from where the audio path would read it. */
    g_pausedSeenByPage = retropicoes_refkeen_frontend_is_paused();
    return g_pageAnswer;
}

int retropicoes_input_menu_pressed(void)
{
    ++g_menuReads;
    if (g_menuArm) {
        g_menuArm = 0; /* consumed: a press is reported once */
        return 1;
    }
    return 0;
}

void retropicoes_video_set_scanlines(int enabled)
{
    ++g_scanlinesSetCalls;
    g_scanlinesLastValue = enabled;
}

void retropicoes_refkeen_event_source_quit(void)
{
    ++g_quitHookCalls;
}

/* ---------------------------------------------------------------------------
 * The port's exported surface, written out here by hand.
 *
 * Check 1 of the gate reads the same list off the compiled object with nm. Two
 * derivations of one fact, and this one is the source-level half: taking each
 * address is what makes a name a *definition* rather than a declaration, which
 * is the distinction the C12 round got wrong four times by reading text.
 * ------------------------------------------------------------------------- */

typedef struct {
    const char *name;
    const void *address;
} contract_symbol_t;

static const contract_symbol_t kSymbols[] = {
    { "retropicoes_refkeen_frontend_boot",
      (const void *)(retropicoes_refkeen_frontend_boot) },
    { "retropicoes_refkeen_frontend_pump",
      (const void *)(retropicoes_refkeen_frontend_pump) },
    { "retropicoes_refkeen_frontend_finished",
      (const void *)(retropicoes_refkeen_frontend_finished) },
    { "retropicoes_refkeen_frontend_shutdown",
      (const void *)(retropicoes_refkeen_frontend_shutdown) },
    { "retropicoes_refkeen_frontend_map_action",
      (const void *)(retropicoes_refkeen_frontend_map_action) },
    { "retropicoes_refkeen_frontend_is_paused",
      (const void *)(retropicoes_refkeen_frontend_is_paused) },
    { "retropicoes_refkeen_frontend_quit_requested",
      (const void *)(retropicoes_refkeen_frontend_quit_requested) },
    { "retropicoes_refkeen_frontend_loader_requested",
      (const void *)(retropicoes_refkeen_frontend_loader_requested) }
};

#define SYMBOL_COUNT (sizeof(kSymbols) / sizeof(kSymbols[0]))

/* ---------------------------------------------------------------------------
 * Harness.
 * ------------------------------------------------------------------------- */

static unsigned g_checks;
static unsigned g_failures;

static void ok(int condition, const char *what)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        printf("FAIL %s\n", what);
    }
}

/* Every test starts from a known device and a cold frontend. The frontend's own
 * statics cannot be reset from here (that is the point -- they are the port's
 * state), so the tests run in one order and each one states what it needs. */
static void reset_device(void)
{
    g_menuArm = 0;
    g_menuReads = 0;
    g_pageAnswer = RETROPICOES_NATIVE_GAME_RESUME;
    g_pausedSeenByPage = -1;
    g_scanlinesArm = 1;
}

static int boot_ok(void)
{
    return retropicoes_refkeen_frontend_boot() ==
           RETROPICOES_REFKEEN_FRONTEND_BOOT_OK;
}

/* ---------------------------------------------------------------------------
 * Tests.
 * ------------------------------------------------------------------------- */

/* A Menu press before the frontend booted must not open a page, and must not
 * burn the press either: the accessor is consumed on read, so a hook that read
 * it before checking its own guard would lose the player's press without ever
 * showing anything. This is the first test to run, and it is the only one that
 * runs before boot(). */
static void test_pump_is_inert_before_boot(void)
{
    reset_device();
    g_pageRuns = 0;
    g_menuArm = 1;

    ok(retropicoes_refkeen_frontend_pump() == RETROPICOES_REFKEEN_FRONTEND_IDLE,
       "before boot the hook reports IDLE even with a press waiting");
    ok(g_pageRuns == 0, "before boot no page runs");
    ok(g_menuReads == 0,
       "before boot the Menu accessor is not even read, so the press survives");
    ok(!retropicoes_refkeen_frontend_quit_requested(),
       "nothing is latched before boot");
    ok(!retropicoes_refkeen_frontend_is_paused(),
       "the pause flag starts clear");
}

/* The saved settings file has to be read, and its scanline answer applied,
 * BEFORE the first frame is presented -- which is why boot() does not delegate
 * the ordering to the caller. */
static void test_boot_applies_the_saved_flags_before_launch(void)
{
    reset_device();
    g_settingsInitCalls = 0;
    g_settingsInitPath[0] = '\0';
    g_scanlinesSetCalls = 0;
    g_scanlinesLastValue = -1;

    ok(boot_ok(), "boot() reaches RUNNING");
    ok(g_settingsInitCalls == 1, "the settings file is initialised exactly once");
    ok(strcmp(g_settingsInitPath, RETROPICOES_REFKEEN_SETTINGS_PATH) == 0,
       "the public settings file is /SAVES/refkeen/settings.dat");
    ok(g_scanlinesSetCalls == 1, "scanlines are applied once at boot");
    ok(g_scanlinesLastValue == 1,
       "the value applied is the saved one (1), not the default");
}

/* The engine's hook is called several times per frame with no press, and that
 * has to mean nothing at all. */
static void test_pump_without_a_press_does_nothing(void)
{
    reset_device();
    g_pageRuns = 0;
    ok(retropicoes_refkeen_frontend_pump() == RETROPICOES_REFKEEN_FRONTEND_IDLE,
       "no Menu press means IDLE, and IDLE is zero");
    ok(g_menuReads == 1, "the Menu accessor is read once per pump");
    ok(g_pageRuns == 0, "no page ran");
}
/* One press, one page -- even though the engine calls the hook several times
 * per frame. */
static void test_one_menu_press_runs_the_page_once(void)
{
    unsigned bits;
    int scansBefore;

    reset_device();
    g_pageRuns = 0;
    g_scanlinesSetCalls = 0;

    g_menuArm = 1;
    bits = retropicoes_refkeen_frontend_pump();
    ok(bits & RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN,
       "a press is reported as SETTINGS_RAN");
    ok(g_pageRuns == 1, "the page ran exactly once");

    scansBefore = g_scanlinesSetCalls;
    g_menuArm = 1; /* a second, later press */
    bits = retropicoes_refkeen_frontend_pump();
    ok((bits & RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN) != 0u,
       "a second press opens the page again");
    ok(g_pageRuns == 2, "the page is not sticky");
    ok(g_scanlinesSetCalls == scansBefore + 1,
       "scanlines are re-applied after the page closes");

    /* The page answer was RESUME, so the game is running again and nothing was
     * latched. */
    ok(!retropicoes_refkeen_frontend_quit_requested(),
       "resuming does not latch a quit");
    ok(!retropicoes_refkeen_frontend_is_paused(),
       "the shell's resume callback cleared the pause flag");
}

/* "Quit game" reaches the shell's stop callback, and the caller is told on the
 * same call and on every later one. */
static void test_quit_latches_and_is_reported_every_time(void)
{
    unsigned bits;

    reset_device();
    g_pageAnswer = RETROPICOES_NATIVE_GAME_QUIT;
    g_menuArm = 1;

    bits = retropicoes_refkeen_frontend_pump();
    ok(bits & RETROPICOES_REFKEEN_FRONTEND_QUIT, "the quit bit is reported");
    ok((bits & RETROPICOES_REFKEEN_FRONTEND_LOADER) == 0u,
       "quitting is not returning to the loader");
    ok(retropicoes_refkeen_frontend_quit_requested(), "the quit request latched");

    /* Every later pump keeps saying so, even with no further press: a caller
     * that missed the first call must not play on forever. */
    bits = retropicoes_refkeen_frontend_pump();
    ok(bits & RETROPICOES_REFKEEN_FRONTEND_QUIT,
       "the quit bit stays reported on later calls");
    ok((bits & RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN) == 0u,
       "and no page ran in that call");
}

/* Return to Loader is a quit plus a flag, and it is the only answer that sets
 * that flag. */
static void test_loader_return_sets_both_bits(void)
{
    unsigned bits;

    reset_device();
    g_pageAnswer = RETROPICOES_NATIVE_GAME_LOADER;
    g_menuArm = 1;

    ok(!retropicoes_refkeen_frontend_loader_requested(),
       "nothing has asked for the loader yet");
    bits = retropicoes_refkeen_frontend_pump();
    ok(bits & RETROPICOES_REFKEEN_FRONTEND_LOADER, "the loader bit is reported");
    ok(bits & RETROPICOES_REFKEEN_FRONTEND_QUIT,
       "the loader bit comes with the quit bit: the loop still has to end");
    ok(retropicoes_refkeen_frontend_loader_requested(),
       "the loader request latched");
}

/* Pause and resume follow the shell, and the flag is readable from outside the
 * shell because the audio path is what consumes it. The observable window is
 * while the page is open -- which is exactly when the audio path would look. */
static void test_the_pause_flag_follows_the_shell(void)
{
    reset_device();
    g_pageAnswer = RETROPICOES_NATIVE_GAME_RESUME;
    g_pausedSeenByPage = -1;
    g_menuArm = 1;
    (void)retropicoes_refkeen_frontend_pump();

    ok(g_pausedSeenByPage == 1,
       "the shell's pause callback ran: the page opens with the port paused");
    ok(!retropicoes_refkeen_frontend_is_paused(),
       "and the resume path cleared it again, so the flag is not stuck on");
}

/* The three-way answer maps onto three distinct shell answers, and never onto
 * the shell's Reset -- which is the whole of C1's decision, measured rather
 * than restated. */
static void test_the_action_mapping_is_the_recorded_one(void)
{
    ok(retropicoes_refkeen_frontend_map_action(RETROPICOES_NATIVE_GAME_RESUME) ==
           RETROPICOES_FRONTEND_SETTINGS_RESUME,
       "page Resume -> shell RESUME");
    ok(retropicoes_refkeen_frontend_map_action(RETROPICOES_NATIVE_GAME_QUIT) ==
           RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER,
       "page Quit -> shell ROM_BROWSER (a native game has no list to return to)");
    ok(retropicoes_refkeen_frontend_map_action(RETROPICOES_NATIVE_GAME_LOADER) ==
           RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER,
       "page Return to Loader -> shell RETURN_LOADER");

    ok(retropicoes_refkeen_frontend_map_action(RETROPICOES_NATIVE_GAME_RESUME) !=
           RETROPICOES_FRONTEND_SETTINGS_RESET &&
       retropicoes_refkeen_frontend_map_action(RETROPICOES_NATIVE_GAME_QUIT) !=
           RETROPICOES_FRONTEND_SETTINGS_RESET &&
       retropicoes_refkeen_frontend_map_action(RETROPICOES_NATIVE_GAME_LOADER) !=
           RETROPICOES_FRONTEND_SETTINGS_RESET,
       "C1: no answer from the page reaches the shell's Reset");
}

/* The engine's teardown runs once, whichever way the loop was left. The runtime
 * has two exit paths (quit and loader return) and both reach teardown. */
static void test_shutdown_runs_the_engine_teardown_once(void)
{
    g_quitHookCalls = 0;
    retropicoes_refkeen_frontend_shutdown();
    retropicoes_refkeen_frontend_shutdown();
    retropicoes_refkeen_frontend_shutdown();
    ok(g_quitHookCalls == 1,
       "the engine's quit hook runs exactly once, not once per call");
}

/* finished() is the shell's own question, so it answers INVALID once the shell
 * is no longer RUNNING -- and it must not report OK twice. It is run while the
 * game really is running, because a second call in a non-running state would
 * pass this without proving anything. */
static void test_finished_closes_the_lifecycle_once(void)
{
    ok(retropicoes_refkeen_frontend_finished() == RETROPICOES_MENU_RESULT_OK,
       "with the game running, finished() closes it out and reports OK");
    ok(retropicoes_refkeen_frontend_finished() == RETROPICOES_MENU_RESULT_INVALID,
       "and cannot report OK a second time");
    ok(retropicoes_refkeen_frontend_finished() == RETROPICOES_MENU_RESULT_INVALID,
       "nor a third: the shell is not RUNNING any more");
}

/* The exported surface, and the lifecycle constants the port was written
 * against. These are printed as CONTRACT lines for the gate to compare with its
 * own record. */
static void report_contract(void)
{
    const retropicoes_frontend_lifecycle_t probe = {
        RETROPICOES_FRONTEND_LIFECYCLE_VERSION,
        sizeof(retropicoes_frontend_lifecycle_t),
        NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };
    size_t i;

    printf("CONTRACT symbols=%u\n", (unsigned)SYMBOL_COUNT);
    for (i = 0; i < SYMBOL_COUNT; ++i)
        printf("CONTRACT symbol=%s\n", kSymbols[i].name);
    printf("CONTRACT lifecycle_version=%u\n",
           (unsigned)RETROPICOES_FRONTEND_LIFECYCLE_VERSION);
    printf("CONTRACT lifecycle_size=%u\n", (unsigned)sizeof(probe));
    printf("CONTRACT settings_path=%s\n", RETROPICOES_REFKEEN_SETTINGS_PATH);
    printf("CONTRACT boot_stage_ok=%u\n",
           (unsigned)RETROPICOES_REFKEEN_FRONTEND_BOOT_OK);
    printf("CONTRACT map_resume=%u\n",
           (unsigned)retropicoes_refkeen_frontend_map_action(
               RETROPICOES_NATIVE_GAME_RESUME));
    printf("CONTRACT map_quit=%u\n",
           (unsigned)retropicoes_refkeen_frontend_map_action(
               RETROPICOES_NATIVE_GAME_QUIT));
    printf("CONTRACT map_loader=%u\n",
           (unsigned)retropicoes_refkeen_frontend_map_action(
               RETROPICOES_NATIVE_GAME_LOADER));
    printf("CONTRACT bit_idle=%u\n",
           (unsigned)RETROPICOES_REFKEEN_FRONTEND_IDLE);
    printf("CONTRACT bit_settings=%u\n",
           (unsigned)RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN);
    printf("CONTRACT bit_quit=%u\n", (unsigned)RETROPICOES_REFKEEN_FRONTEND_QUIT);
    printf("CONTRACT bit_loader=%u\n",
           (unsigned)RETROPICOES_REFKEEN_FRONTEND_LOADER);
}

int main(void)
{
    size_t i, j;

    /* The one premise the whole stub rests on, asserted rather than written
     * down: two distinct names must not alias one address, or "which symbol is
     * this" would have no answer. */
    for (i = 0; i < SYMBOL_COUNT; ++i) {
        ok(kSymbols[i].address != NULL, kSymbols[i].name);
        for (j = i + 1u; j < SYMBOL_COUNT; ++j) {
            ok(kSymbols[i].address != kSymbols[j].address,
               "two contract symbols share an address");
        }
    }

    /* Order matters, and it is load-bearing in two places:
     *
     *   * test_pump_is_inert_before_boot is the only test that runs before
     *     boot(), and boot() is what arms the hook -- so it has to be first.
     *   * the shell has four screens, and the tests that need RUNNING say so by
     *     running while it still is; the two that need it after finished() closed
     *     it out re-boot, which is what the board's runtime does after a Loader
     *     return anyway. */
    test_pump_is_inert_before_boot();

    test_boot_applies_the_saved_flags_before_launch();
    test_pump_without_a_press_does_nothing();
    test_one_menu_press_runs_the_page_once();
    test_the_pause_flag_follows_the_shell();
    test_finished_closes_the_lifecycle_once();

    ok(boot_ok(), "boot() is repeatable: the runtime reboots after a loader return");

    test_quit_latches_and_is_reported_every_time();
    test_loader_return_sets_both_bits();

    test_the_action_mapping_is_the_recorded_one();
    test_shutdown_runs_the_engine_teardown_once();

    report_contract();

    if (g_failures != 0u) {
        printf("REFKEEN FRONTEND ADAPTER: FAIL (%u of %u assertion(s))\n",
               g_failures, g_checks);
        return 1;
    }
    printf("REFKEEN FRONTEND ADAPTER: PASS (%u assertions)\n", g_checks);
    return 0;
}
