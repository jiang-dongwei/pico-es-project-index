/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TI83_FRONTEND_H
#define TI83_FRONTEND_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t frames, audio_samples_underflow, audio_samples_dropped;
    uint32_t last_input_us, last_step_us, last_draw_us, last_present_us, last_audio_us, last_frame_us;
} retropicoes_ti83_frontend_stats_t;
typedef struct { uint8_t audio_enabled, scanlines_enabled, fps_overlay_enabled, controller_mode; } retropicoes_ti83_frontend_config_t;
int retropicoes_ti83_frontend_start(const char *, char *, unsigned);
int retropicoes_ti83_frontend_run_frame(void);
int retropicoes_ti83_frontend_pause(void);
int retropicoes_ti83_frontend_resume(void);
void retropicoes_ti83_frontend_stop(void);
void retropicoes_ti83_frontend_get_stats(retropicoes_ti83_frontend_stats_t *);
void retropicoes_ti83_frontend_set_config(const retropicoes_ti83_frontend_config_t *);
#ifdef __cplusplus
}
#endif
#endif