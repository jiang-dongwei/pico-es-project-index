/* Host test for the port's event source: compat mask edges -> engine key events.
 *
 * WHAT THIS CAN AND CANNOT REACH
 * ------------------------------
 * be_events_port.c itself cannot be built on the host: it includes upstream's
 * engine headers (be_st.h, be_input.h) and the positional keymap. What IS
 * host-buildable is the arithmetic between them -- given two masks, which
 * events, in which order -- and that is where every off-by-one would live.
 *
 * So this file tests event_source.c against exact literals, and then does
 * something stronger: it *simulates* the engine's own per-scancode state machine
 * (be_input.c keeps g_sdlEmuKeyboardStateByScanCode[] and ignores a reported
 * state equal to the stored one) and asserts that replaying the events from a
 * state consistent with the previous mask lands exactly on the current mask.
 * That formulation is independent of how the events are produced, so it catches
 * a whole class of mistakes the literal checks would not: dropped channels,
 * duplicated events, and an order that leaves a released key stuck down.
 *
 * The remaining hop -- SDL scancode -> DOS scancode through
 * be_events_sdl_keymap.h -> BEL_ST_HandleEmuKeyboardEvent -- is not reachable
 * here at all. It is pinned by Test-InputScancodeContract.ps1 (which re-derives
 * every DOS value from upstream's own sources) and by Test-EventSourceContract
 * .ps1 (which proves every scancode this port can emit is actually mapped).
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "compat.h"
#include "event_source.h"
#include "input_mapping.h"

/* SDL's scancode space, as the keymap array is declared. The simulation needs
 * it to model the engine's own per-scancode state. */
#define SIM_SCANCODE_COUNT 512u

static const char *const g_channelNames[COMPAT_KEY_COUNT] = {
    "LEFT", "RIGHT", "UP", "DOWN", "JUMP", "ACTION", "START", "BACK"
};

static uint32_t bit_of(compat_key_t channel)
{
    return 1u << (unsigned)channel;
}

static size_t collect(uint32_t current, uint32_t previous,
                      retropicoes_refkeen_key_event_t *out, size_t capacity)
{
    return retropicoes_refkeen_key_events_from_masks(current, previous, out,
                                                    capacity);
}

/* --- Known vectors -------------------------------------------------------- */

static void test_no_edges_means_no_events(void)
{
    retropicoes_refkeen_key_event_t out[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    size_t i;

    for (i = 0u; i < (size_t)RETROPICOES_REFKEEN_KEY_EVENT_MAX; ++i) {
        out[i].scancode = 0xffffu;
        out[i].pressed = 0xffu;
    }

    assert(collect(0u, 0u, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) == 0u);
    assert(out[0].scancode == 0xffffu); /* untouched */

    /* Holding a key is not an event: the engine must not be told again. */
    assert(collect(bit_of(COMPAT_KEY_UP) | bit_of(COMPAT_KEY_JUMP),
                   bit_of(COMPAT_KEY_UP) | bit_of(COMPAT_KEY_JUMP),
                   out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) == 0u);
}

static void test_press_only(void)
{
    retropicoes_refkeen_key_event_t out[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    size_t n;

    n = collect(bit_of(COMPAT_KEY_UP), 0u, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX);
    assert(n == 1u);
    assert(out[0].scancode == 82u);  /* SDL_SCANCODE_UP */
    assert(out[0].pressed == 1u);
}

static void test_release_only(void)
{
    retropicoes_refkeen_key_event_t out[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    size_t n;

    n = collect(0u, bit_of(COMPAT_KEY_ACTION), out,
                RETROPICOES_REFKEEN_KEY_EVENT_MAX);
    assert(n == 1u);
    assert(out[0].scancode == 224u); /* SDL_SCANCODE_LCTRL */
    assert(out[0].pressed == 0u);
}

/* The ordering rule from event_source.h, pinned. Note that "ascending channel
 * order" is NOT "ascending scancode order": LEFT is channel 0 and RIGHT is
 * channel 1, but RIGHT's scancode (79) is lower than LEFT's (80). Asserting the
 * scancodes in this order is what makes the rule unambiguous. */
static void test_releases_precede_presses_and_order_is_by_channel(void)
{
    retropicoes_refkeen_key_event_t out[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    const uint32_t previous = bit_of(COMPAT_KEY_LEFT) | bit_of(COMPAT_KEY_UP);
    const uint32_t current = bit_of(COMPAT_KEY_RIGHT) | bit_of(COMPAT_KEY_DOWN);
    size_t n;

    n = collect(current, previous, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX);
    assert(n == 4u);

    assert(out[0].pressed == 0u); /* releases first ... */
    assert(out[0].scancode == 80u); /* LEFT  (channel 0) */
    assert(out[1].pressed == 0u);
    assert(out[1].scancode == 82u); /* UP    (channel 2) */
    assert(out[2].pressed == 1u);   /* ... then presses */
    assert(out[2].scancode == 79u); /* RIGHT (channel 1) */
    assert(out[3].pressed == 1u);
    assert(out[3].scancode == 81u); /* DOWN  (channel 3) */
}

/* --- Capacity handling ---------------------------------------------------- */

static void test_capacity_and_null(void)
{
    retropicoes_refkeen_key_event_t out[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    const uint32_t all = 0xffffffffu;

    /* Capacity 0 and a NULL destination are answered, not dereferenced. */
    assert(collect(all, 0u, out, 0u) == 0u);
    assert(collect(all, 0u, NULL, RETROPICOES_REFKEEN_KEY_EVENT_MAX) == 0u);
    assert(collect(all, 0u, NULL, 0u) == 0u);

    /* The whole keyboard going down at once must fit the advertised bound
     * exactly; an under-stated bound would silently drop the last channel. */
    assert(collect(all, 0u, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) ==
           (size_t)COMPAT_KEY_COUNT);

    /* Truncation is reported through the count, and nothing past the capacity is
     * written. */
    out[1].scancode = 0xffffu;
    out[1].pressed = 0xffu;
    assert(collect(all, 0u, out, 1u) == 1u);
    assert(out[0].scancode == 80u);   /* LEFT is channel 0 */
    assert(out[0].pressed == 1u);
    assert(out[1].scancode == 0xffffu);
    assert(out[1].pressed == 0xffu);

    /* Both classes at once: 8 releases + 8 presses must also fit. */
    assert(collect(0u, all, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) ==
           (size_t)COMPAT_KEY_COUNT);
    assert(collect(all, 0u, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) +
           collect(0u, all, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) ==
           2u * (size_t)COMPAT_KEY_COUNT);
}

/* --- Input hygiene -------------------------------------------------------- */

static void test_bits_above_the_channel_count_are_ignored(void)
{
    retropicoes_refkeen_key_event_t out[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    const uint32_t garbage = 0xffffff00u; /* every bit at or above channel 8 */

    /* A stray high bit must not become a scancode, and must not perturb the
     * channels that do exist. `mask = 0xffffffff` is the realistic case: the
     * compat layer hands over whatever the backend accumulated. */
    assert(collect(garbage, 0u, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) == 0u);
    assert(collect(0u, garbage, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX) == 0u);
    assert(collect(0xffffffffu, 0xffffffffu, out,
                   RETROPICOES_REFKEEN_KEY_EVENT_MAX) == 0u);
}

/* --- Independent formulation: simulate the engine's state machine --------- */

/* Replay the events from a state consistent with `previous` and require the
 * result to agree with `current` for every channel. This does not know how the
 * events were produced, so a missing, duplicated or mis-ordered event shows up
 * as a disagreement rather than as a wrong literal. */
static void expect_replay_lands_on_current(uint32_t current, uint32_t previous)
{
    retropicoes_refkeen_key_event_t out[RETROPICOES_REFKEEN_KEY_EVENT_MAX];
    uint8_t state[SIM_SCANCODE_COUNT];
    size_t n;
    size_t i;
    unsigned channel;

    for (i = 0u; i < SIM_SCANCODE_COUNT; ++i)
        state[i] = 0u;

    n = collect(current, previous, out, RETROPICOES_REFKEEN_KEY_EVENT_MAX);
    assert(n <= (size_t)RETROPICOES_REFKEEN_KEY_EVENT_MAX);

    /* Precondition: the engine's view before this step. */
    for (channel = 0u; channel < (unsigned)COMPAT_KEY_COUNT; ++channel) {
        if (0u != (previous & (1u << channel))) {
            uint16_t sc = retropicoes_refkeen_scancode_for((compat_key_t)channel);
            assert(sc != 0u);
            state[sc] = 1u;
        }
    }

    /* Apply, exactly as BEL_ST_HandleEmuKeyboardEvent() would. */
    for (i = 0u; i < n; ++i) {
        assert(out[i].scancode < SIM_SCANCODE_COUNT);
        state[out[i].scancode] = (0u != out[i].pressed) ? 1u : 0u;
    }

    /* Postcondition: per-channel agreement with the current level view. This is
     * the property that matters -- the engine must not disagree with reality. */
    for (channel = 0u; channel < (unsigned)COMPAT_KEY_COUNT; ++channel) {
        uint16_t sc = retropicoes_refkeen_scancode_for((compat_key_t)channel);
        const uint8_t want = (0u != (current & (1u << channel))) ? 1u : 0u;
        assert(state[sc] == want);
    }
}

static void test_replay_covers_the_interesting_mask_pairs(void)
{
    unsigned a;
    unsigned b;

    /* Exhaustive over pairs of channels (release one, press another), which is
     * every shape a step can take: nothing, press, release, or a swap. */
    for (a = 0u; a <= (unsigned)COMPAT_KEY_COUNT; ++a) {
        for (b = 0u; b <= (unsigned)COMPAT_KEY_COUNT; ++b) {
            const uint32_t previous = (a < (unsigned)COMPAT_KEY_COUNT) ? (1u << a) : 0u;
            const uint32_t current = (b < (unsigned)COMPAT_KEY_COUNT) ? (1u << b) : 0u;
            expect_replay_lands_on_current(current, previous);
        }
    }

    /* And the extremes, where a bound or an off-by-one would show up. */
    expect_replay_lands_on_current(0u, 0u);
    expect_replay_lands_on_current(0xffffffffu, 0u);
    expect_replay_lands_on_current(0u, 0xffffffffu);
    expect_replay_lands_on_current(0xffffffffu, 0xffffffffu);
    expect_replay_lands_on_current(0x00ffu, 0xff00u);
    expect_replay_lands_on_current(0xff00u, 0x00ffu);
}

/* A channel's scancode must be unique across channels, otherwise the replay
 * invariant above would be satisfiable by an order that is wrong in general.
 * Keen Dreams needs this anyway: two channels on one key would make the game's
 * control panel unbindable. */
static void test_scancodes_are_unique(void)
{
    unsigned i;
    unsigned j;

    for (i = 0u; i < (unsigned)COMPAT_KEY_COUNT; ++i) {
        const uint16_t a = retropicoes_refkeen_scancode_for((compat_key_t)i);
        assert(a != 0u);
        for (j = i + 1u; j < (unsigned)COMPAT_KEY_COUNT; ++j) {
            const uint16_t b = retropicoes_refkeen_scancode_for((compat_key_t)j);
            assert(a != b);
        }
    }
}

int main(void)
{
    test_no_edges_means_no_events();
    test_press_only();
    test_release_only();
    test_releases_precede_presses_and_order_is_by_channel();
    test_capacity_and_null();
    test_bits_above_the_channel_count_are_ignored();
    test_replay_covers_the_interesting_mask_pairs();
    test_scancodes_are_unique();

    /* Facts the gate re-derives from source, printed from the compiled module so
     * the two must agree. */
    printf("CONTRACT channels=%d\n", (int)COMPAT_KEY_COUNT);
    printf("CONTRACT event_max=%d\n", (int)RETROPICOES_REFKEEN_KEY_EVENT_MAX);
    printf("CONTRACT scancode_max=%d\n", (int)RETROPICOES_REFKEEN_SCANCODE_MAX);
    {
        unsigned channel;
        for (channel = 0u; channel < (unsigned)COMPAT_KEY_COUNT; ++channel) {
            printf("CONTRACT channel=%s scancode=%u\n", g_channelNames[channel],
                   (unsigned)retropicoes_refkeen_scancode_for(
                       (compat_key_t)channel));
        }
    }

    puts("REFKEEN EVENT SOURCE: PASS");
    return 0;
}
