#ifndef RETROPICOES_COMMON_MENU_RENDER_H
#define RETROPICOES_COMMON_MENU_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "menu_input.h"
#include "menu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_MENU_WIDTH 320u
#define RETROPICOES_MENU_HEIGHT 240u

typedef struct retropicoes_menu_surface {
    uint16_t *pixels;
    size_t stride;
    size_t width;
    size_t height;
} retropicoes_menu_surface_t;

typedef struct retropicoes_menu_theme {
    uint16_t background;
    uint16_t foreground;
    uint16_t muted;
    uint16_t accent;
    uint16_t selection_background;
    uint16_t selection_foreground;
    uint16_t danger;
} retropicoes_menu_theme_t;

typedef void (*retropicoes_menu_draw_text_fn)(
    retropicoes_menu_surface_t *surface,
    int x,
    int y,
    const char *text,
    uint16_t color,
    unsigned scale,
    size_t max_chars,
    void *context);

typedef struct retropicoes_menu_renderer {
    retropicoes_menu_draw_text_fn draw_text;
    void *text_context;
    retropicoes_menu_theme_t theme;
} retropicoes_menu_renderer_t;

typedef struct retropicoes_menu_view {
    const char *title;
    const char *const *rows;
    size_t row_count;
    size_t selected;
    size_t first_visible;
    size_t visible_rows;
    const retropicoes_controller_labels_t *labels;
    const char *confirmation;
} retropicoes_menu_view_t;

/* Nintendo-style 40x30 character renderer used by the reference
 * pico_shared Settings screen. Coordinates are pixel coordinates. */
void retropicoes_menu_nintendo_draw_text(
    retropicoes_menu_surface_t *surface,
    int x,
    int y,
    const char *text,
    uint16_t color,
    unsigned scale,
    size_t max_chars,
    void *context);
retropicoes_menu_renderer_t retropicoes_menu_nintendo_renderer(void);

retropicoes_menu_theme_t retropicoes_menu_default_theme(void);
int retropicoes_menu_render(const retropicoes_menu_renderer_t *renderer,
                            retropicoes_menu_surface_t *surface,
                            const retropicoes_menu_view_t *view);
int retropicoes_system_menu_render(
    const retropicoes_menu_renderer_t *renderer,
    retropicoes_menu_surface_t *surface,
    const retropicoes_system_menu_state_t *state,
    const retropicoes_controller_labels_t *labels,
    const char *title);

#ifdef __cplusplus
}
#endif

#endif

