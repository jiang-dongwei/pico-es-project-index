/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_STUDIO2_SETTINGS_ADAPTER_H
#define RETROPICOES_STUDIO2_SETTINGS_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { RETROPICOES_STUDIO2_THEME_COLOR_COUNT = 64 };

typedef struct {
    uint8_t audio_enabled;
    uint8_t scanlines_enabled;
    uint8_t keypad_bank;
    uint8_t foreground_color;
    uint8_t background_color;
    uint8_t keypad_player;
    uint8_t reserved[1];
} retropicoes_studio2_settings_t;

void retropicoes_studio2_settings_defaults(retropicoes_studio2_settings_t *settings);
int retropicoes_studio2_settings_sanitize(retropicoes_studio2_settings_t *settings);
uint16_t retropicoes_studio2_theme_color(unsigned index);
const char *retropicoes_studio2_theme_color_name(unsigned index);

#ifdef __cplusplus
}
#endif
#endif
