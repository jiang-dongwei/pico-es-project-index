/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "frontend.h"

#include <stddef.h>
#include <string.h>

bool space_invaders_frontend_init(space_invaders_frontend_t *frontend)
{
    if (frontend == NULL) return false;
    memset(frontend, 0, sizeof(*frontend));
    if (!space_invaders_board_init(&frontend->board)) return false;
    space_invaders_input_init(&frontend->input);
    space_invaders_audio_init(&frontend->audio);
    frontend->video_mode = SPACE_INVADERS_VIDEO_CLASSIC_OVERLAY;
    return true;
}

space_invaders_rom_result_t space_invaders_frontend_load_roms(
    space_invaders_frontend_t *frontend,
    const space_invaders_rom_set_t *roms)
{
    if (frontend == NULL)
        return space_invaders_validate_roms(NULL);
    space_invaders_input_init(&frontend->input);
    space_invaders_audio_init(&frontend->audio);
    return space_invaders_board_load_roms(&frontend->board, roms);
}

space_invaders_frontend_result_t space_invaders_frontend_frame(
    space_invaders_frontend_t *frontend,
    uint32_t player1_keys, uint32_t player2_keys, uint32_t now_ms,
    uint16_t *frame, unsigned stride,
    int16_t *audio_samples, size_t audio_sample_count)
{
    space_invaders_frontend_result_t result;
    space_invaders_input_result_t input;
    uint8_t rising3 = 0u, falling3 = 0u, rising5 = 0u, falling5 = 0u;

    memset(&result, 0, sizeof(result));
    if (frontend == NULL || !frontend->board.rom_ready) {
        result.status = SPACE_INVADERS_RUN_ROM_NOT_READY;
        return result;
    }
    input = space_invaders_input_update(&frontend->input,
                                        player1_keys, player2_keys, now_ms);
    space_invaders_board_set_inputs(&frontend->board,
                                    input.input1, input.input2);
    result.status = SPACE_INVADERS_RUN_OK;
    for (unsigned half = 0; half < 2u; ++half) {
        space_invaders_half_frame_result_t half_result =
            space_invaders_board_run_half_frame(&frontend->board);
        if (half_result.status != SPACE_INVADERS_RUN_OK) {
            result.status = half_result.status;
            return result;
        }
        result.cpu_cycles += half_result.executed_cycles;
        result.cpu_instructions += half_result.instructions;
        result.frame_ready |= half_result.frame_complete;
    }
    space_invaders_board_take_sound_edges(
        &frontend->board, &rising3, &falling3, &rising5, &falling5);
    space_invaders_audio_events(&frontend->audio,
                                rising3, falling3, rising5, falling5);
    if (audio_samples != NULL && audio_sample_count != 0u)
        space_invaders_audio_render(&frontend->audio,
                                    audio_samples, audio_sample_count);
    if (frame != NULL &&
        !space_invaders_video_compose(
            &frontend->board.memory[SPACE_INVADERS_VRAM_START],
            SPACE_INVADERS_VRAM_BYTES, frame, stride,
            frontend->video_mode)) {
        result.status = SPACE_INVADERS_RUN_CPU_STALLED;
        result.frame_ready = false;
    }
    return result;
}
