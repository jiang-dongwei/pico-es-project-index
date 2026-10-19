/* The one thing the C9 event TU needs to know about the public frontend.
 *
 * WHY THIS IS A SEPARATE HEADER
 * -----------------------------
 * The frontend's per-frame hook has to be called from BE_ST_PollEvents(), which
 * lives in be_events_port.c -- the port's replacement for upstream's event TU
 * (see be_events_port.h for why the engine owns the loop and the port may not
 * edit an upstream TU).
 *
 * That TU's only job is the keyboard pipeline, and its dependency surface is a
 * measured contract: it includes nothing from the shared frontend. Pulling in
 * frontend.h would drag frontend_shell.h and native_game_settings.h (and through
 * it compat.h) into a TU whose job never looks at a settings page, and it would
 * change the include list every gate that compiles that TU has to use. So the
 * bits and the one prototype live here, with no includes at all, and frontend.h
 * includes this header rather than redeclaring them -- one fact, one place.
 */

#ifndef REFKEEN_RETROPICOES_FRONTEND_HOOK_H
#define REFKEEN_RETROPICOES_FRONTEND_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif

/* What one pump() call may have done.
 *
 * Returned as bits rather than as a single value because the caller has two
 * different obligations, and a caller that handles only the first is a bug that
 * silently keeps playing after the player asked to leave. */
enum {
    RETROPICOES_REFKEEN_FRONTEND_IDLE = 0u,
    /* The shared Settings page ran and returned in this call. The caller must
     * reseed the engine's view of the keyboard: the page consumed the device
     * input itself, so what the engine believes is held is now stale.
     * retropicoes_refkeen_event_source_reset() (be_events_port.h) is the
     * documented way to say exactly that. */
    RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN = 1u << 0,
    /* "Quit game" was chosen. The game loop has to end. */
    RETROPICOES_REFKEEN_FRONTEND_QUIT = 1u << 1,
    /* "Return to Loader" was chosen. The game loop has to end and the port must
     * hand control back to the bootloader instead of restarting. */
    RETROPICOES_REFKEEN_FRONTEND_LOADER = 1u << 2
};

/* The per-frame hook. Inert until boot() has succeeded, so a Menu press during
 * the engine's pre-graphics text screens cannot open a page on top of them.
 * Returns a bitmask of the enum above. Defined in frontend.c. */
unsigned retropicoes_refkeen_frontend_pump(void);

#ifdef __cplusplus
}
#endif

#endif /* REFKEEN_RETROPICOES_FRONTEND_HOOK_H */
