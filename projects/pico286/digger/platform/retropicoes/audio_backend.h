#ifndef SDLPOP_RETROPICOES_AUDIO_BACKEND_H
#define SDLPOP_RETROPICOES_AUDIO_BACKEND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t callback_calls;
    uint32_t source_samples;
    uint32_t output_frames;
    uint32_t packets_pushed;
    uint32_t queue_full_events;
    uint32_t underruns;
    uint32_t queue_level;
    uint32_t source_sample_rate;
    uint32_t hdmi_sample_rate;
    uint8_t source_channels;
    uint8_t ready;
    uint8_t running;
} retropicoes_audio_stats_t;

int retropicoes_audio_ready(void);
void retropicoes_audio_get_stats(retropicoes_audio_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
