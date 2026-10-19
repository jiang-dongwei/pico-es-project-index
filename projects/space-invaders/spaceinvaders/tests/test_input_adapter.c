/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "compat.h"
#include "input_adapter.h"
#include "space_invaders_board.h"

#include <stdio.h>

static unsigned failures;
#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static uint32_t key(compat_key_t value)
{
    return 1u << (unsigned)value;
}

int main(void)
{
    space_invaders_input_state_t state;
    space_invaders_input_result_t result;

    space_invaders_input_init(&state);
    /* Release once arms Start and discards a Loader launch press. */
    result = space_invaders_input_update(&state, key(COMPAT_KEY_START), 0u, 0u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) == 0u);
    result = space_invaders_input_update(&state, 0u, 0u, 10u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) == 0u);

    result = space_invaders_input_update(
        &state, key(COMPAT_KEY_LEFT) | key(COMPAT_KEY_JUMP) |
                key(COMPAT_KEY_ACTION) | key(COMPAT_KEY_BACK),
        key(COMPAT_KEY_RIGHT) | key(COMPAT_KEY_ACTION), 20u);
    CHECK((result.input1 & (SPACE_INVADERS_INPUT_LEFT |
                            SPACE_INVADERS_INPUT_FIRE |
                            SPACE_INVADERS_INPUT_COIN)) ==
          (SPACE_INVADERS_INPUT_LEFT | SPACE_INVADERS_INPUT_FIRE |
           SPACE_INVADERS_INPUT_COIN));
    CHECK((result.input2 & (SPACE_INVADERS_INPUT_RIGHT |
                            SPACE_INVADERS_INPUT_FIRE)) ==
          (SPACE_INVADERS_INPUT_RIGHT | SPACE_INVADERS_INPUT_FIRE));

    /* P1 Start is emitted immediately on a fresh press, not on release. */
    result = space_invaders_input_update(&state, key(COMPAT_KEY_START), 0u, 100u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) != 0u);
    result = space_invaders_input_update(&state, key(COMPAT_KEY_START), 0u, 150u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) == 0u);
    result = space_invaders_input_update(&state, 0u, 0u, 200u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) == 0u);

    /* P2 Start selects the two-player input on a fresh press. */
    result = space_invaders_input_update(&state, 0u, key(COMPAT_KEY_START), 300u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_2P) != 0u);

    /* Holding Start cannot swallow the start pulse or trigger a hidden action. */
    (void)space_invaders_input_update(&state, 0u, 0u, 400u);
    (void)space_invaders_input_update(&state, key(COMPAT_KEY_START), 0u, 1000u);
    result = space_invaders_input_update(&state, key(COMPAT_KEY_START), 0u,
                                         2499u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) == 0u);
    result = space_invaders_input_update(&state, key(COMPAT_KEY_START), 0u,
                                         2500u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) == 0u);
    result = space_invaders_input_update(&state, 0u, 0u, 3010u);
    CHECK((result.input1 & SPACE_INVADERS_INPUT_START_1P) == 0u);

    if (failures != 0u) return 1;
    puts("Space Invaders input adapter checks passed");
    return 0;
}
