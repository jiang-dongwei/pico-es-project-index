/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_PERF_REPORT_C_H
#define RETROPICOES_PERF_REPORT_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t last_input_us;
    uint32_t last_step_us;
    uint32_t last_draw_us;
    uint32_t last_present_us;
    uint32_t last_audio_us;
    uint32_t last_frame_us;
    uint32_t audio_samples_underflow;
    uint32_t audio_samples_dropped;
} retropicoes_perf_frame_t;

void retropicoes_perf_begin(const char *name, uint64_t now,
                            uint32_t budget_us, uint32_t target_fps10);
void retropicoes_perf_frame(const retropicoes_perf_frame_t *frame,
                            uint64_t now);
void retropicoes_perf_pump(void);

#ifdef __cplusplus
}
#endif

#endif
