/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_AUDIO_ADAPTER_H
#define RETROPICOES_SPACE_INVADERS_AUDIO_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPACE_INVADERS_AUDIO_RATE 22050u

typedef struct {
    uint32_t ufo_phase;
    uint32_t tone_phase;
    uint32_t noise_lfsr;
    uint32_t rendered_samples;
    int32_t shot_envelope;
    int32_t player_die_envelope;
    int32_t invader_die_envelope;
    int32_t ufo_hit_envelope;
    int32_t fleet_envelope;
    uint8_t fleet_step;
    uint8_t sound_port3;
    uint8_t sound_port5;
} space_invaders_audio_t;

void space_invaders_audio_init(space_invaders_audio_t *audio);
void space_invaders_audio_events(space_invaders_audio_t *audio,
                                 uint8_t rising3, uint8_t falling3,
                                 uint8_t rising5, uint8_t falling5);
void space_invaders_audio_render(space_invaders_audio_t *audio,
                                 int16_t *samples, size_t sample_count);

#ifdef __cplusplus
}
#endif
#endif
