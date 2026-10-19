/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings_adapter.h"

#include <string.h>
#include "nintendo_palette.h"

void retropicoes_ti83_settings_defaults(retropicoes_ti83_settings_t *settings)
{
    if (settings == NULL) return;
    memset(settings, 0, sizeof(*settings));
    settings->audio_enabled = 0u;
    settings->foreground_color = RETROPICOES_NINTENDO_BLACK;
    settings->background_color = RETROPICOES_NINTENDO_WHITE;

}

int retropicoes_ti83_settings_sanitize(retropicoes_ti83_settings_t *settings)
{
    int changed = 0;
    if (settings == NULL) return -1;
#define BOOL_FIELD(field) do { if (settings->field > 1u) { settings->field = 1u; changed = 1; } } while (0)
    BOOL_FIELD(audio_enabled);
    BOOL_FIELD(scanlines_enabled);
    BOOL_FIELD(fps_overlay_enabled);
#undef BOOL_FIELD
    if (settings->controller_mode > 1u) { settings->controller_mode = 0u; changed = 1; }
    if (settings->foreground_color >= RETROPICOES_TI83_THEME_COLOR_COUNT) {
        settings->foreground_color = RETROPICOES_NINTENDO_BLACK; changed = 1;
    }
    if (settings->background_color >= RETROPICOES_TI83_THEME_COLOR_COUNT) {
        settings->background_color = RETROPICOES_NINTENDO_WHITE; changed = 1;
    }
    if (settings->foreground_color == settings->background_color) {
        settings->foreground_color =
            settings->background_color == RETROPICOES_NINTENDO_BLACK
                ? RETROPICOES_NINTENDO_WHITE
                : RETROPICOES_NINTENDO_BLACK;
        changed = 1;
    }
    memset(settings->reserved, 0, sizeof(settings->reserved));
    return changed;
}

uint16_t retropicoes_ti83_theme_color(unsigned index)
{
    return retropicoes_nintendo_palette_color(index);
}

const char *retropicoes_ti83_theme_color_name(unsigned index)
{
    return retropicoes_nintendo_palette_name(index);
}