/* The port's replacement for upstream src/backend/events/be_events_sdl.c.
 * See be_events_port.h for what that TU is expected to provide and why the
 * replacement is this short. */

/* The port replaces be_events_sdl.c, which is the only implementation of the
 * events-callback pair. Defining this macro without that TU leaves
 * BEL_ST_CheckForExitFromEventsCallback() and BEL_ST_EventsCallback() undefined
 * at link time, which is a confusing way to learn about it. */
#ifdef REFKEEN_CONFIG_EVENTS_CALLBACK
#error "REFKEEN_CONFIG_EVENTS_CALLBACK must stay undefined: the Pico port replaces be_events_sdl.c, its only implementation."
#endif

#include "be_st.h"
#include "be_st_sdl_private.h"

#include "backend/audio/be_audio_main_thread.h"
#include "backend/input/be_input.h"

/* Upstream states at the top of this header that it must be included *only*
 * from be_events_sdl.c, because it defines a non-static table at file scope.
 * This TU is that file's replacement, so it becomes the one legal includer.
 * Test-EventSourceContract.ps1 checks that no second includer appears. */
#include "backend/events/be_events_sdl_keymap.h"

#include "compat.h"

#include "be_events_port.h"
#include "event_source.h"
#include "frontend_hook.h"

static void (*g_refkeenAppQuitCallback)(void) = NULL;
static uint32_t g_refkeenPreviousMask;

/* Set by BE_ST_InitAudio() in be_audio_init.c. That is a portable TU the port
 * builds, so this symbol is available -- it is NOT one of the platform symbols
 * the port drops. be_audio_sdl.c only declares it extern, and no header declares
 * it at all, which is why upstream's own TU declares it locally as well
 * (be_events_sdl.c:50). */
extern bool g_sdlAudioSubsystemUp;

/* The compat layer exposes levels, one channel at a time, and owns the device
 * details; this TU only needs them as a mask, because everything downstream
 * reasons about edges. */
static uint32_t retropicoes_refkeen_compat_mask(void)
{
    uint32_t mask = 0u;
    unsigned channel;

    for (channel = 0u; channel < (unsigned)COMPAT_KEY_COUNT; ++channel)
        if (compat_key_down((compat_key_t)channel))
            mask |= 1u << channel;

    return mask;
}

void retropicoes_refkeen_event_source_reset(void)
{
    g_refkeenPreviousMask = 0u;
}

void retropicoes_refkeen_event_source_quit(void)
{
    void (*callback)(void) = g_refkeenAppQuitCallback;

    if (NULL == callback)
        return;

    /* Clear before running: the callback is Keen Dreams' ShutdownId(), which
     * saves the config and takes the engine apart. Re-entering it would be a
     * real bug rather than a theoretical one, because the exit paths that reach
     * here are also the ones a held button keeps re-triggering. */
    g_refkeenAppQuitCallback = NULL;
    callback();
}

void BE_ST_SetAppQuitCallback(void (*funcPtr)(void))
{
    g_refkeenAppQuitCallback = funcPtr;
}

void BE_ST_PollEvents(void)
{
    static bool first_poll_logged;
    static uint32_t last_heartbeat_ms;
    static uint32_t polls_since_heartbeat;
    if (!first_poll_logged) {
        first_poll_logged = true;
        printf("REFKEEN STAGE: first event poll\n");
    }
    retropicoes_refkeen_key_event_t events[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    uint32_t currentMask, previousMask;
    unsigned frontendBits;
    size_t count, index;

    g_sdlLastPollEventsTime = BEL_ST_GetTicksMS();
    ++polls_since_heartbeat;
    if (g_sdlLastPollEventsTime - last_heartbeat_ms >= 5000u) {
        printf("REFKEEN HEARTBEAT: ms=%u polls=%u previous_keys=%08x\n",
               (unsigned)g_sdlLastPollEventsTime,
               (unsigned)polls_since_heartbeat,
               (unsigned)g_refkeenPreviousMask);
        last_heartbeat_ms = g_sdlLastPollEventsTime;
        polls_since_heartbeat = 0u;
    }

    /* The engine calls BE_ST_PollEvents() from several places per frame (the
     * input layer, the timing layer and id_us.c all do), so the device is
     * sampled here rather than from a port-side runtime loop. Re-polling is
     * harmless, and the edge detection below makes the repeated calls collapse
     * to "no events" after the first one, which is exactly what upstream's
     * event queue did. */
    compat_poll_input();

    /* The public frontend's per-frame hook. Keen Dreams' own main loop owns the
     * frame, and the port may not edit an upstream TU, so this call-out is the
     * only place on that loop the port owns -- which is why it lives here and
     * not in a runtime loop like the sibling ports have. See frontend.h.
     *
     * The device is already sampled, which is what the Menu accessor the hook
     * reads needs; the hook is inert until the frontend has booted. */
    frontendBits = retropicoes_refkeen_frontend_pump();
    if (frontendBits & RETROPICOES_REFKEEN_FRONTEND_SETTINGS_RAN)
    {
        /* The shared Settings page polled the device itself while it was open,
         * so the engine's stored view of the keyboard is stale. Upstream clears
         * the same state before handing over a Loader, and the C9 header
         * documents this as the second reason that reset exists. Reseeding here
         * also means a direction the player was holding when the page opened is
         * reported to the engine as a fresh press rather than as nothing. */
        retropicoes_refkeen_event_source_reset();
    }
    if (frontendBits & (RETROPICOES_REFKEEN_FRONTEND_QUIT |
                        RETROPICOES_REFKEEN_FRONTEND_LOADER))
    {
        /* Upstream's own exit primitive, from the window-close path of its
         * event TU (be_events_sdl.c:529). The port supplies it -- be_startup_sdl.c
         * is one of the TUs it replaces -- and it does not return, so the game
         * loop is left from here and the port's main() finishes the teardown.
         * Whether to hand control to the bootloader is main()'s read of
         * retropicoes_refkeen_frontend_loader_requested(). */
        BE_ST_QuickExit();
    }

    previousMask = g_refkeenPreviousMask;
    currentMask = retropicoes_refkeen_compat_mask();
    g_refkeenPreviousMask = currentMask;

    count = retropicoes_refkeen_key_events_from_masks(
        currentMask, previousMask, events, RETROPICOES_REFKEEN_KEY_EVENT_MAX);
    if (count)
        printf("REFKEEN INPUT EDGE: ms=%u previous=%08x current=%08x count=%u\n",
               (unsigned)g_sdlLastPollEventsTime,
               (unsigned)previousMask, (unsigned)currentMask,
               (unsigned)count);

    for (index = 0; index < count; ++index) {
        /* Indexed by SDL scancode, positionally -- the property the whole design
         * rests on, and the reason input_mapping.c speaks SDL scancodes. */
        emulatedDOSKeyEvent dosKeyEvent = sdlKeyMappings[events[index].scancode];

        /* An unmapped scancode is {false, 0}. be_input.c would treat DOS
         * scancode 0 as a real key, so it must be filtered here; the contract
         * gate also proves no scancode this port can emit is unmapped. */
        if (0 == dosKeyEvent.dosScanCode)
            continue;

        BEL_ST_HandleEmuKeyboardEvent((0 != events[index].pressed), false,
                                      dosKeyEvent);
    }

    /* Key repeat emulation, kept in step with upstream's own loop at the end of
     * BE_ST_PollEvents() in be_events_sdl.c. The state and the timings live in
     * be_input.c / be_input.h and are shared with BEL_ST_HandleEmuKeyboardEvent,
     * so this must not grow a private copy of them; only the trigger belongs
     * here. */
    if (g_sdlEmuKeyboardLastPressedScanCode &&
        ((g_sdlLastPollEventsTime - g_sdlEmuKeyboardLastScanCodePressTime) >=
         g_sdlEmuKeyboardLastScanCodePressTimeDelay))
    {
        emulatedDOSKeyEvent dosKeyEvent;
        dosKeyEvent.isSpecial = g_sdlEmuKeyboardLastPressedIsSpecial;
        dosKeyEvent.dosScanCode = g_sdlEmuKeyboardLastPressedScanCode;

        BEL_ST_HandleEmuKeyboardEvent(true, true, dosKeyEvent);

        g_sdlEmuKeyboardLastScanCodePressTime +=
            g_sdlEmuKeyboardLastScanCodePressTimeDelay;
        g_sdlEmuKeyboardLastScanCodePressTimeDelay =
            BE_ST_CONTROLLER_DIGIACTION_REPEAT_RATE_MS;
    }

    /* The manual audio pump at the end of BE_ST_PollEvents(), with upstream's
     * own condition. BE_ST_FILL_AUDIO_IN_MAIN_THREAD is a port-supplied macro
     * and this port does not set it (measured: it is defined nowhere under
     * refkeen/), so the #else arm is the one that compiles. Keeping the first
     * arm means that if the port ever sets the macro, the behaviour follows
     * upstream instead of silently staying behind.
     *
     * The #else arm is not a formality. BE_ST_PrepareForManualAudioCallbackCall()
     * advances the mixer by elapsed time, so calling it while the audio subsystem
     * is up would advance the emulated sound chips a second time per tick, next
     * to whoever is already driving the mixer. Upstream's comment says what the
     * guard is for: with audio disabled, at least run the sound callback, so no
     * loop gets stuck waiting for sound playback to complete. Which arm the port
     * lands on is therefore the port's audio backend's decision, expressed
     * through BEL_ST_InitAudioSubsystem()'s return value -- not a decision for
     * the event source to pre-empt. */
#ifdef BE_ST_FILL_AUDIO_IN_MAIN_THREAD
    BE_ST_PrepareForManualAudioCallbackCall();
#else
    if (!g_sdlAudioSubsystemUp)
        BE_ST_PrepareForManualAudioCallbackCall();
#endif
}
