/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "input_adapter.h"

#include <stddef.h>
#include <string.h>

#include "compat.h"
#include "space_invaders_board.h"

static uint32_t key_bit(compat_key_t key)
{
    return 1u << (unsigned)key;
}

static bool down(uint32_t keys, compat_key_t key)
{
    return (keys & key_bit(key)) != 0u;
}

static bool update_start(space_invaders_input_state_t *state,
                         unsigned player, bool is_down)
{
    if (!is_down) {
        state->start_down[player] = false;
        state->start_armed[player] = true;
        return false;
    }
    const bool pressed = state->start_armed[player] &&
                         !state->start_down[player];
    state->start_down[player] = true;
    return pressed;
}

void space_invaders_input_init(space_invaders_input_state_t *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

space_invaders_input_result_t space_invaders_input_update(
    space_invaders_input_state_t *state,
    uint32_t player1_keys, uint32_t player2_keys, uint32_t now_ms)
{
    space_invaders_input_result_t result = {
        SPACE_INVADERS_INPUT_ALWAYS_ON, 0u
    };
    (void)now_ms;

    if (state == NULL)
        return result;
    if (down(player1_keys, COMPAT_KEY_LEFT))
        result.input1 |= SPACE_INVADERS_INPUT_LEFT;
    if (down(player1_keys, COMPAT_KEY_RIGHT))
        result.input1 |= SPACE_INVADERS_INPUT_RIGHT;
    if (down(player1_keys, COMPAT_KEY_JUMP) ||
        down(player1_keys, COMPAT_KEY_ACTION))
        result.input1 |= SPACE_INVADERS_INPUT_FIRE;

    if (down(player2_keys, COMPAT_KEY_LEFT))
        result.input2 |= SPACE_INVADERS_INPUT_LEFT;
    if (down(player2_keys, COMPAT_KEY_RIGHT))
        result.input2 |= SPACE_INVADERS_INPUT_RIGHT;
    if (down(player2_keys, COMPAT_KEY_JUMP) ||
        down(player2_keys, COMPAT_KEY_ACTION))
        result.input2 |= SPACE_INVADERS_INPUT_FIRE;

    if (down(player1_keys | player2_keys, COMPAT_KEY_BACK))
        result.input1 |= SPACE_INVADERS_INPUT_COIN;
    if (update_start(state, 0u, down(player1_keys, COMPAT_KEY_START)))
        result.input1 |= SPACE_INVADERS_INPUT_START_1P;
    if (update_start(state, 1u, down(player2_keys, COMPAT_KEY_START)))
        result.input1 |= SPACE_INVADERS_INPUT_START_2P;
    return result;
}
