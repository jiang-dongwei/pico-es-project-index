/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_adapter.h"

#include <stddef.h>
#include <string.h>

enum {
    SOUND3_UFO = 1u << 0,
    SOUND3_SHOT = 1u << 1,
    SOUND3_PLAYER_DIE = 1u << 2,
    SOUND3_INVADER_DIE = 1u << 3,
    SOUND5_FLEET_MASK = 0x0fu,
    SOUND5_UFO_HIT = 1u << 4
};

static int32_t decay(int32_t value, int32_t amount)
{
    return value > amount ? value - amount : 0;
}

static int32_t square(uint32_t phase, int32_t amplitude)
{
    return (phase & 0x80000000u) != 0u ? amplitude : -amplitude;
}

static int32_t noise(space_invaders_audio_t *audio, int32_t amplitude)
{
    uint32_t feedback = (audio->noise_lfsr ^
                         (audio->noise_lfsr >> 1)) & 1u;
    audio->noise_lfsr = (audio->noise_lfsr >> 1) | (feedback << 15);
    return (audio->noise_lfsr & 1u) != 0u ? amplitude : -amplitude;
}

static int16_t clamp16(int32_t value)
{
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return (int16_t)value;
}

void space_invaders_audio_init(space_invaders_audio_t *audio)
{
    if (audio == NULL) return;
    memset(audio, 0, sizeof(*audio));
    audio->noise_lfsr = 0xace1u;
}

void space_invaders_audio_events(space_invaders_audio_t *audio,
                                 uint8_t rising3, uint8_t falling3,
                                 uint8_t rising5, uint8_t falling5)
{
    if (audio == NULL) return;
    audio->sound_port3 |= rising3;
    audio->sound_port3 &= (uint8_t)~falling3;
    audio->sound_port5 |= rising5;
    audio->sound_port5 &= (uint8_t)~falling5;
    if ((rising3 & SOUND3_SHOT) != 0u) audio->shot_envelope = 12000;
    if ((rising3 & SOUND3_PLAYER_DIE) != 0u)
        audio->player_die_envelope = 15000;
    if ((rising3 & SOUND3_INVADER_DIE) != 0u)
        audio->invader_die_envelope = 11000;
    if ((rising5 & SOUND5_UFO_HIT) != 0u)
        audio->ufo_hit_envelope = 14000;
    if ((rising5 & SOUND5_FLEET_MASK) != 0u) {
        for (unsigned bit = 0; bit < 4u; ++bit) {
            if ((rising5 & (1u << bit)) != 0u)
                audio->fleet_step = (uint8_t)bit;
        }
        audio->fleet_envelope = 9000;
    }
}

void space_invaders_audio_render(space_invaders_audio_t *audio,
                                 int16_t *samples, size_t sample_count)
{
    static const uint32_t fleet_increment[4] = {
        27269626u, 32246067u, 37977808u, 44744621u
    };
    if (audio == NULL || samples == NULL) return;
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t mixed = 0;
        if ((audio->sound_port3 & SOUND3_UFO) != 0u) {
            uint32_t modulation = (audio->rendered_samples >> 7) & 0x3fu;
            audio->ufo_phase += 17530478u + (modulation << 13);
            mixed += square(audio->ufo_phase, 3500);
        }
        if (audio->shot_envelope != 0) {
            mixed += noise(audio, audio->shot_envelope / 2);
            audio->shot_envelope = decay(audio->shot_envelope, 18);
        }
        if (audio->player_die_envelope != 0) {
            mixed += noise(audio, audio->player_die_envelope / 2);
            audio->player_die_envelope =
                decay(audio->player_die_envelope, 7);
        }
        if (audio->invader_die_envelope != 0) {
            audio->tone_phase += 46747943u;
            mixed += square(audio->tone_phase,
                            audio->invader_die_envelope / 2);
            audio->invader_die_envelope =
                decay(audio->invader_die_envelope, 22);
        }
        if (audio->ufo_hit_envelope != 0) {
            mixed += noise(audio, audio->ufo_hit_envelope / 3);
            audio->ufo_hit_envelope = decay(audio->ufo_hit_envelope, 9);
        }
        if (audio->fleet_envelope != 0) {
            audio->tone_phase += fleet_increment[audio->fleet_step];
            mixed += square(audio->tone_phase, audio->fleet_envelope / 2);
            audio->fleet_envelope = decay(audio->fleet_envelope, 30);
        }
        samples[i] = clamp16(mixed);
        ++audio->rendered_samples;
    }
}
