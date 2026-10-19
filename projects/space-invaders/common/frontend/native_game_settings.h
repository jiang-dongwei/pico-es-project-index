#ifndef RETROPICOES_NATIVE_GAME_SETTINGS_H
#define RETROPICOES_NATIVE_GAME_SETTINGS_H

#include "compat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum retropicoes_native_game_action {
    RETROPICOES_NATIVE_GAME_RESUME = 0,
    RETROPICOES_NATIVE_GAME_QUIT,
    RETROPICOES_NATIVE_GAME_LOADER
} retropicoes_native_game_action_t;

/* Shared Settings page for single native games using compat v1. */
void retropicoes_native_game_settings_init(const char *save_path);
int retropicoes_native_game_audio_enabled(void);
int retropicoes_native_game_scanlines_enabled(void);
int retropicoes_native_game_fps_enabled(void);
void retropicoes_native_game_draw_fps(compat_framebuffer_t *framebuffer);
retropicoes_native_game_action_t retropicoes_native_game_settings_run(void);

#ifdef __cplusplus
}
#endif
#endif

