/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_FRONTEND_H
#define RETROPICOES_SPACE_INVADERS_FRONTEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_adapter.h"
#include "input_adapter.h"
#include "space_invaders_board.h"
#include "video_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    space_invaders_board_t board;
    space_invaders_input_state_t input;
    space_invaders_audio_t audio;
    space_invaders_video_mode_t video_mode;
} space_invaders_frontend_t;

typedef struct {
    space_invaders_run_status_t status;
    uint32_t cpu_cycles;
    uint32_t cpu_instructions;
    bool frame_ready;
} space_invaders_frontend_result_t;

bool space_invaders_frontend_init(space_invaders_frontend_t *frontend);
space_invaders_rom_result_t space_invaders_frontend_load_roms(
    space_invaders_frontend_t *frontend,
    const space_invaders_rom_set_t *roms);
space_invaders_frontend_result_t space_invaders_frontend_frame(
    space_invaders_frontend_t *frontend,
    uint32_t player1_keys, uint32_t player2_keys, uint32_t now_ms,
    uint16_t *frame, unsigned stride,
    int16_t *audio_samples, size_t audio_sample_count);

#ifdef __cplusplus
}
#endif
#endif
