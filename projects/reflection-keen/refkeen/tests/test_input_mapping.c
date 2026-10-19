/* Host test for the Keen Dreams input contract.
 *
 * Two layers are checked here:
 *
 *   1. The port's channel -> SDL scancode table, against exact literals. This
 *      catches a silent renumbering of a channel, which would otherwise only
 *      show up as a dead or wrong key on hardware.
 *   2. The *declared* DOS scancode contract -- the value the engine must end up
 *      seeing once upstream's be_events_sdl_keymap.h has converted the SDL
 *      scancode. This file cannot compute that conversion without dragging the
 *      whole engine header chain into a host build, so instead every channel
 *      must declare its expected DOS scancode here, and
 *      refkeen/scripts/Test-InputScancodeContract.ps1 re-derives each value
 *      from upstream's own sources (id_in.c defaults, be_events_sdl_keymap.h,
 *      be_st.h) and fails if this table is wrong. Neither layer alone is
 *      enough; together they pin the chain end to end.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "compat.h"
#include "input_mapping.h"

/* The DOS scancodes the engine is expected to observe, with provenance:
 *
 *   0x1d / 0x38 / 0x48 / 0x50 / 0x4b / 0x4d
 *       src/kdreams/id_in.c:65  KbdDefs[0] = {0x1d,0x38,0x47,0x48,0x49,0x4b,
 *                                             0x4d,0x4f,0x50,0x51}
 *       fields: button0, button1, upleft, up, upright, left, right,
 *               downleft, down, downright
 *   0x1c  BE_ST_SC_ENTER  (src/be_st.h DOS set-1 enum)
 *   0x01  BE_ST_SC_ESC    (same)
 *
 * The diagonal defaults 0x47/0x49/0x4f/0x51 are deliberately absent: the port
 * does not emit them (see the diagonals note in input_mapping.h).
 */
typedef struct refkeen_channel_contract {
    compat_key_t channel;
    uint16_t sdl_scancode;
    uint8_t dos_scancode;
} refkeen_channel_contract_t;

static const refkeen_channel_contract_t g_contract[] = {
    {COMPAT_KEY_LEFT, 80u, 0x4bu},   /* SDL_SCANCODE_LEFT  -> 'left'  */
    {COMPAT_KEY_RIGHT, 79u, 0x4du},  /* SDL_SCANCODE_RIGHT -> 'right' */
    {COMPAT_KEY_UP, 82u, 0x48u},     /* SDL_SCANCODE_UP    -> 'up'    */
    {COMPAT_KEY_DOWN, 81u, 0x50u},   /* SDL_SCANCODE_DOWN  -> 'down'  */
    {COMPAT_KEY_JUMP, 226u, 0x38u},  /* SDL_SCANCODE_LALT  -> button1 */
    {COMPAT_KEY_ACTION, 224u, 0x1du},/* SDL_SCANCODE_LCTRL -> button0 */
    {COMPAT_KEY_START, 40u, 0x1cu},  /* SDL_SCANCODE_RETURN */
    {COMPAT_KEY_BACK, 41u, 0x01u},   /* SDL_SCANCODE_ESCAPE */
};

#define CONTRACT_COUNT (sizeof(g_contract) / sizeof(g_contract[0]))

static void test_channel_table_matches_contract(void)
{
    size_t i;

    for (i = 0u; i < CONTRACT_COUNT; ++i) {
        assert(retropicoes_refkeen_scancode_for(g_contract[i].channel) ==
               g_contract[i].sdl_scancode);
        /* A declared DOS scancode of 0 would mean "the engine sees nothing",
         * i.e. a key that looks mapped but is dead. */
        assert(g_contract[i].dos_scancode != 0u);
    }

    /* Every channel of the compat enum must be covered, so adding a channel
     * cannot silently fall through to the default 0 entry in the port table. */
    for (i = 0u; i < (size_t)COMPAT_KEY_COUNT; ++i) {
        size_t seen = 0u;
        size_t j;
        for (j = 0u; j < CONTRACT_COUNT; ++j) {
            if ((size_t)g_contract[j].channel == i)
                ++seen;
        }
        assert(seen == 1u);
        assert(retropicoes_refkeen_scancode_for((compat_key_t)i) != 0u);
    }
}

static void test_out_of_range_channel(void)
{
    /* Answered, not asserted: this runs every frame. */
    assert(retropicoes_refkeen_scancode_for(COMPAT_KEY_COUNT) == 0u);
    assert(retropicoes_refkeen_scancode_for((compat_key_t)999) == 0u);
}

static void test_mask_expansion(void)
{
    uint16_t out[RETROPICOES_REFKEEN_SCANCODE_MAX];
    size_t n;
    size_t i;

    /* Empty mask: nothing to emit, and nothing written. */
    for (i = 0u; i < sizeof(out) / sizeof(out[0]); ++i)
        out[i] = 0xffffu;
    assert(retropicoes_refkeen_scancodes_from_mask(0u, out,
                                                   sizeof(out) / sizeof(out[0])) == 0u);
    assert(out[0] == 0xffffu); /* untouched */

    /* Ascending channel order, which is what makes the result deterministic. */
    n = retropicoes_refkeen_scancodes_from_mask(
        (1u << COMPAT_KEY_UP) | (1u << COMPAT_KEY_LEFT), out,
        sizeof(out) / sizeof(out[0]));
    assert(n == 2u);
    assert(out[0] == 80u); /* LEFT comes before UP in compat_key_t */
    assert(out[1] == 82u);

    /* The full set must fit the advertised bound exactly -- if the bound were
     * ever under-stated, the last channel would be silently dropped. */
    n = retropicoes_refkeen_scancodes_from_mask(0xffffffffu, out,
                                                sizeof(out) / sizeof(out[0]));
    assert(n == (size_t)COMPAT_KEY_COUNT);

    /* Truncation is reported by the returned count, and the buffer beyond the
     * capacity is not written. */
    out[1] = 0xffffu;
    n = retropicoes_refkeen_scancodes_from_mask(0xffffffffu, out, 1u);
    assert(n == 1u);
    assert(out[0] == 80u);   /* LEFT */
    assert(out[1] == 0xffffu);

    /* A NULL destination is tolerated rather than dereferenced. */
    assert(retropicoes_refkeen_scancodes_from_mask(0xffffffffu, NULL, 8u) == 0u);
}

/* xrick and digger both expose a MENU channel for the settings entry. Keen
 * Dreams must not: the physical Menu button is a separate rising edge, so no
 * compat channel carries it and none may be silently repurposed for it. */
static void test_menu_button_is_not_a_compat_channel(void)
{
    assert((size_t)COMPAT_KEY_COUNT == CONTRACT_COUNT);
}

int main(void)
{
    test_channel_table_matches_contract();
    test_out_of_range_channel();
    test_mask_expansion();
    test_menu_button_is_not_a_compat_channel();

    puts("REFKEEN INPUT MAPPING: PASS");
    return 0;
}
