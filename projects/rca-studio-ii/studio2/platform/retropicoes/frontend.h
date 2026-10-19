/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_STUDIO2_FRONTEND_H
#define RETROPICOES_STUDIO2_FRONTEND_H

#include <stdint.h>

#include "studio2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t frames;
    uint32_t instructions;
    uint32_t audio_samples;
    uint32_t audio_samples_dropped;
    uint32_t audio_samples_underflow;
    uint32_t last_input_us;
    uint32_t last_present_us;
    uint32_t last_step_us;
    uint32_t last_draw_us;
    uint32_t last_audio_us;
    uint32_t last_frame_us;
    uint32_t peak_frame_us;
    uint32_t over_budget_frames;
    uint8_t keypad_bank;
    uint16_t fps_tenths;
} retropicoes_studio2_frontend_stats_t;

typedef struct {
    uint8_t audio_enabled;
    uint8_t scanlines_enabled;
    uint8_t keypad_bank;
    uint8_t keypad_player;
} retropicoes_studio2_frontend_config_t;

typedef struct {
    uint32_t raw_mask, source_masks[4], raw_seen, events;
    uint32_t ef_cycles[2], video_hash, frames;
    uint16_t keys[2], seen[2], pc, setup_frames;
    uint8_t usb_connected, player, bank, latch;
} retropicoes_studio2_input_debug_t;

int retropicoes_studio2_frontend_start(const char *rom_path, int profile, char *error,
                                       unsigned error_capacity);
/* 0=continue, 1=ROM exit opcode, 2=Menu/Select opened Settings. */
int retropicoes_studio2_frontend_run_frame(void);
int retropicoes_studio2_frontend_pause(void);
int retropicoes_studio2_frontend_resume(void);
void retropicoes_studio2_frontend_stop(void);
void retropicoes_studio2_frontend_get_stats(retropicoes_studio2_frontend_stats_t *stats);
void retropicoes_studio2_frontend_get_input_debug(retropicoes_studio2_input_debug_t *debug);
int retropicoes_studio2_frontend_can_start_tennis(void);
int retropicoes_studio2_frontend_start_tennis(void);
void retropicoes_studio2_frontend_set_config(const retropicoes_studio2_frontend_config_t *config);

#ifdef __cplusplus
}
#endif
#endif
