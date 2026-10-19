/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_TI83_SETTINGS_ADAPTER_H
#define RETROPICOES_TI83_SETTINGS_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RETROPICOES_TI83_THEME_COLOR_COUNT = 64 };

typedef struct {
    uint8_t audio_enabled;
    uint8_t scanlines_enabled;
    uint8_t fps_overlay_enabled;
    uint8_t controller_mode;
    uint8_t foreground_color;
    uint8_t background_color;

    uint8_t reserved[6];
} retropicoes_ti83_settings_t;

void retropicoes_ti83_settings_defaults(retropicoes_ti83_settings_t *settings);
int retropicoes_ti83_settings_sanitize(retropicoes_ti83_settings_t *settings);
uint16_t retropicoes_ti83_theme_color(unsigned index);
const char *retropicoes_ti83_theme_color_name(unsigned index);

#ifdef __cplusplus
}
#endif
#endif