/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_STATE_ADAPTER_H
#define RETROPICOES_SPACE_INVADERS_STATE_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "space_invaders_board.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPACE_INVADERS_STATE_VERSION 1u
#define SPACE_INVADERS_SAVE_ROOT "/SAVES/spaceinvaders"

typedef enum {
    SPACE_INVADERS_STATE_OK = 0,
    SPACE_INVADERS_STATE_BUFFER_TOO_SMALL,
    SPACE_INVADERS_STATE_BAD_MAGIC,
    SPACE_INVADERS_STATE_BAD_VERSION,
    SPACE_INVADERS_STATE_BAD_SIZE,
    SPACE_INVADERS_STATE_ROM_MISMATCH,
    SPACE_INVADERS_STATE_BAD_CRC,
    SPACE_INVADERS_STATE_VALUE_OUT_OF_RANGE
} space_invaders_state_status_t;

size_t space_invaders_state_size(void);
space_invaders_state_status_t space_invaders_state_save(
    const space_invaders_board_t *board, uint8_t *output, size_t capacity,
    size_t *written);
space_invaders_state_status_t space_invaders_state_load(
    space_invaders_board_t *board, const uint8_t *data, size_t size);
bool space_invaders_state_path(char *output, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
