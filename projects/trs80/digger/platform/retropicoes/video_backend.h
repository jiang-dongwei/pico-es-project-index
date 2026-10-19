#ifndef SDLPOP_RETROPICOES_VIDEO_BACKEND_H
#define SDLPOP_RETROPICOES_VIDEO_BACKEND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t hstx_frames;
    uint32_t published_frames;
    uint32_t displayed_frames;
    uint32_t dropped_frames;
    uint32_t resync_count;
} retropicoes_video_stats_t;

int retropicoes_video_init(void);
void retropicoes_video_shutdown(void);
int retropicoes_video_ready(void);
void retropicoes_video_get_stats(retropicoes_video_stats_t *stats);
void retropicoes_video_set_scanlines(int enabled);

#ifdef __cplusplus
}
#endif

#endif
