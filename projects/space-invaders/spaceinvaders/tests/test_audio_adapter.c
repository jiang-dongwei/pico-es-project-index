/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio_adapter.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static int has_signal(const int16_t *samples, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (samples[i] != 0) return 1;
    return 0;
}

int main(void)
{
    space_invaders_audio_t audio;
    int16_t samples[512];
    int16_t first_ufo[64];

    space_invaders_audio_init(&audio);
    memset(samples, 0x55, sizeof(samples));
    space_invaders_audio_render(&audio, samples, 64u);
    CHECK(!has_signal(samples, 64u));

    space_invaders_audio_events(&audio, 0x01u, 0u, 0u, 0u);
    space_invaders_audio_render(&audio, first_ufo, 64u);
    CHECK(has_signal(first_ufo, 64u));
    space_invaders_audio_events(&audio, 0u, 0x01u, 0u, 0u);
    space_invaders_audio_render(&audio, samples, 64u);
    CHECK(!has_signal(samples, 64u));

    space_invaders_audio_events(&audio, 0x02u, 0u, 0u, 0u);
    space_invaders_audio_render(&audio, samples, 64u);
    CHECK(has_signal(samples, 64u));
    CHECK(audio.shot_envelope < 12000);

    space_invaders_audio_init(&audio);
    space_invaders_audio_events(&audio, 0x0cu, 0u, 0x11u, 0u);
    space_invaders_audio_render(&audio, samples, 128u);
    CHECK(has_signal(samples, 128u));
    CHECK(audio.player_die_envelope > 0 &&
          audio.invader_die_envelope > 0 && audio.ufo_hit_envelope > 0);
    CHECK(audio.fleet_step == 0u && audio.fleet_envelope > 0);

    for (unsigned fleet = 0; fleet < 4u; ++fleet) {
        space_invaders_audio_init(&audio);
        space_invaders_audio_events(&audio, 0u, 0u,
                                    (uint8_t)(1u << fleet), 0u);
        space_invaders_audio_render(&audio, samples, 64u);
        CHECK(audio.fleet_step == fleet && has_signal(samples, 64u));
    }

    if (failures != 0u) return 1;
    puts("Space Invaders procedural audio checks passed");
    return 0;
}
