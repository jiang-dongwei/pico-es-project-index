/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_PERF_REPORT_H
#define RETROPICOES_SPACE_INVADERS_PERF_REPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t input_us;
    uint32_t step_us;
    uint32_t draw_us;
    uint32_t present_us;
    uint32_t audio_us;
    uint32_t total_us;
    uint32_t displayed_frames;
    uint32_t refresh_frames;
    uint32_t audio_underflow;
    uint32_t audio_dropped;
    uint32_t video_dropped;
    uint32_t resync_count;
} space_invaders_perf_frame_t;

typedef struct {
    uint64_t window_start_us;
    uint64_t sums[6];
    uint32_t frames;
    uint32_t displayed_frames;
    uint32_t refresh_frames;
    uint32_t over_budget;
    uint32_t peak_us;
    uint32_t previous_audio_underflow;
    uint32_t previous_audio_dropped;
    uint32_t previous_video_dropped;
    uint32_t previous_resync_count;
    uint32_t uart_dropped;
} space_invaders_perf_t;

void space_invaders_perf_init(space_invaders_perf_t *perf, uint64_t now_us);
void space_invaders_perf_add(space_invaders_perf_t *perf,
                             const space_invaders_perf_frame_t *frame);
void space_invaders_perf_note_uart_drop(space_invaders_perf_t *perf);
size_t space_invaders_perf_format(space_invaders_perf_t *perf,
                                  const space_invaders_perf_frame_t *latest,
                                  uint64_t now_us,
                                  char *output, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
