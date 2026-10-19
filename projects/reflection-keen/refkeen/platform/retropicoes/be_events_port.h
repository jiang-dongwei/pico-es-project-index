/* The port's replacement for upstream src/backend/events/be_events_sdl.c.
 *
 * WHAT UPSTREAM EXPECTS FROM THAT TU
 * ----------------------------------
 * Dropping be_events_sdl.c from the build removes exactly two symbols the rest
 * of the engine references (verified by scanning the upstream tree):
 *
 *   BE_ST_PollEvents()                        declared in be_st.h, called from
 *                                             be_input.c (4x), be_timing.c (8x)
 *                                             and kdreams/id_us.c
 *   BE_ST_SetAppQuitCallback(void (*)(void))   declared in be_st.h, called from
 *                                             kdreams/kd_main.c with ShutdownId
 *
 * Two more -- BEL_ST_CheckForExitFromEventsCallback() and BEL_ST_EventsCallback()
 * -- live behind REFKEEN_CONFIG_EVENTS_CALLBACK. That macro belongs to the
 * mobile/background-app ports and is deliberately left undefined here (see
 * refkeen_config.h); a compile-time guard below enforces it, so defining it
 * later produces a clear error instead of two undefined symbols at link time.
 *
 * WHAT THE REPLACEMENT DOES NOT DO
 * --------------------------------
 * be_events_sdl.c is 660 lines, and all but a handful of them are desktop or
 * alt-control concerns the port has no counterpart for: mouse and touch input,
 * the on-screen keyboard, joystick/gamepad enumeration and sensor handling,
 * window resize/expose/fullscreen events, and the ALT control scheme. None of
 * those reach the engine through the keyboard path, so the replacement is a
 * straight-line pipeline with no event loop at all.
 *
 * The one non-obvious piece that IS kept is upstream's key-repeat emulation:
 * SDL's own repeat is ignored (be_events_sdl.c does the same) and the repeat is
 * synthesised from state be_input.c owns. See be_events_port.c.
 */

#ifndef REFKEEN_RETROPICOES_BE_EVENTS_PORT_H
#define REFKEEN_RETROPICOES_BE_EVENTS_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forget the previous compat mask, so the next poll reports the currently held
 * keys as fresh presses instead of as edges against a stale frame.
 *
 * Upstream does the equivalent at startup -- be_startup_sdl.c memsets
 * g_sdlEmuKeyboardStateByScanCode[] -- and the port needs it for the same
 * reason plus two of its own: a Loader handoff, and returning from the shared
 * Settings page, both leave the engine's view of the keyboard cleared while the
 * player may still be holding a direction.
 *
 * Call this whenever the engine's keyboard state is cleared. */
void retropicoes_refkeen_event_source_reset(void);

/* Run the callback registered through BE_ST_SetAppQuitCallback(), if any, and
 * forget it.
 *
 * Upstream fires it from its SDL_EVENT_QUIT handler and then calls
 * BE_ST_QuickExit(). A 320x240 panel has no window to close, so the port's exit
 * paths (a Loader return, a Menu+Start hold, a fatal error) are the only ones
 * that exist, and they belong to the port's runtime rather than to this TU.
 * This is the hook they call. It is one-shot on purpose: the registered
 * callback is Keen Dreams' ShutdownId(), which saves the config, and running it
 * twice would re-enter the engine's shutdown. */
void retropicoes_refkeen_event_source_quit(void);

#ifdef __cplusplus
}
#endif

#endif /* REFKEEN_RETROPICOES_BE_EVENTS_PORT_H */
