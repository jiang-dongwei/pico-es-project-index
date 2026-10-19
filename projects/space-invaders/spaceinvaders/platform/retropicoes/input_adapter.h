/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_INPUT_ADAPTER_H
#define RETROPICOES_SPACE_INVADERS_INPUT_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool start_down[2];
    bool start_armed[2];
} space_invaders_input_state_t;

typedef struct {
    uint8_t input1;
    uint8_t input2;
} space_invaders_input_result_t;

void space_invaders_input_init(space_invaders_input_state_t *state);
space_invaders_input_result_t space_invaders_input_update(
    space_invaders_input_state_t *state,
    uint32_t player1_keys, uint32_t player2_keys, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
