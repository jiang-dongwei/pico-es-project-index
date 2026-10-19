#include "menu_render.h"

#include <string.h>

#include "nintendo_font_8x8.h"

enum {
    TITLE_Y = 10,
    LIST_Y = 48,
    ROW_HEIGHT = 16,
    FOOTER_Y = 220
};

static void fill_rect(retropicoes_menu_surface_t *surface,
                      int x,
                      int y,
                      int width,
                      int height,
                      uint16_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width;
    int y1 = y + height;
    int py;

    if (x1 > (int)surface->width) {
        x1 = (int)surface->width;
    }
    if (y1 > (int)surface->height) {
        y1 = (int)surface->height;
    }
    for (py = y0; py < y1; ++py) {
        int px;
        uint16_t *line = surface->pixels + (size_t)py * surface->stride;
        for (px = x0; px < x1; ++px) {
            line[px] = color;
        }
    }
}

void retropicoes_menu_nintendo_draw_text(
    retropicoes_menu_surface_t *surface,
    int x,
    int y,
    const char *text,
    uint16_t color,
    unsigned scale,
    size_t max_chars,
    void *context)
{
    size_t count = 0u;
    (void)context;
    if (surface == NULL || surface->pixels == NULL || text == NULL) return;
    if (scale == 0u) scale = 1u;
    while (*text != '\0' && count < max_chars) {
        unsigned character = (unsigned char)*text++;
        unsigned row;
        if (character < RETROPICOES_FONT_FIRST_ASCII ||
            character >= RETROPICOES_FONT_FIRST_ASCII +
                             RETROPICOES_FONT_CHARACTER_COUNT) {
            character = '?';
        }
        for (row = 0u; row < 8u; ++row) {
            unsigned bits = retropicoes_nintendo_font_8x8[
                character - RETROPICOES_FONT_FIRST_ASCII +
                row * RETROPICOES_FONT_CHARACTER_COUNT];
            unsigned column;
            for (column = 0u; column < 8u; ++column) {
                if ((bits & (1u << column)) != 0u) {
                    fill_rect(surface,
                              x + (int)(column * scale),
                              y + (int)(row * scale),
                              (int)scale, (int)scale, color);
                }
            }
        }
        x += (int)(8u * scale);
        ++count;
    }
}

retropicoes_menu_renderer_t retropicoes_menu_nintendo_renderer(void)
{
    retropicoes_menu_renderer_t renderer;
    renderer.draw_text = retropicoes_menu_nintendo_draw_text;
    renderer.text_context = NULL;
    renderer.theme.background = 0xffffu;
    renderer.theme.foreground = 0x0000u;
    renderer.theme.muted = 0x8410u;
    renderer.theme.accent = 0x0000u;
    renderer.theme.selection_background = 0x0000u;
    renderer.theme.selection_foreground = 0xffffu;
    renderer.theme.danger = 0xf800u;
    return renderer;
}

static void border(retropicoes_menu_surface_t *surface,
                   int x,
                   int y,
                   int width,
                   int height,
                   uint16_t color)
{
    fill_rect(surface, x, y, width, 2, color);
    fill_rect(surface, x, y + height - 2, width, 2, color);
    fill_rect(surface, x, y, 2, height, color);
    fill_rect(surface, x + width - 2, y, 2, height, color);
}

static int valid(const retropicoes_menu_renderer_t *renderer,
                 const retropicoes_menu_surface_t *surface,
                 const retropicoes_menu_view_t *view)
{
    return renderer != NULL && renderer->draw_text != NULL &&
           surface != NULL && surface->pixels != NULL &&
           surface->width == RETROPICOES_MENU_WIDTH &&
           surface->height == RETROPICOES_MENU_HEIGHT &&
           surface->stride >= surface->width && view != NULL &&
           (view->row_count == 0u || view->rows != NULL) &&
           view->first_visible <= view->row_count &&
           (view->row_count == 0u || view->selected < view->row_count);
}

static size_t text_width(const char *text, unsigned scale, size_t max_chars)
{
    size_t length = text != NULL ? strlen(text) : 0u;
    if (length > max_chars) {
        length = max_chars;
    }
    return length * 6u * scale;
}

static void draw_centered(const retropicoes_menu_renderer_t *renderer,
                          retropicoes_menu_surface_t *surface,
                          int y,
                          const char *text,
                          uint16_t color,
                          unsigned scale,
                          size_t max_chars)
{
    size_t width = text_width(text, scale, max_chars);
    int x = width < surface->width
                ? ((int)surface->width - (int)width) / 2
                : 0;
    renderer->draw_text(surface, x, y, text, color, scale, max_chars,
                        renderer->text_context);
}

static void draw_footer(const retropicoes_menu_renderer_t *renderer,
                        retropicoes_menu_surface_t *surface,
                        const retropicoes_controller_labels_t *labels)
{
    const retropicoes_controller_labels_t *shown = labels;

    if (shown == NULL) {
        shown = retropicoes_menu_controller_labels(
            RETROPICOES_CONTROLLER_UNKNOWN);
    }
    renderer->draw_text(surface, 10, FOOTER_Y, shown->confirm,
                        renderer->theme.accent, 1u, 10u,
                        renderer->text_context);
    renderer->draw_text(surface, 76, FOOTER_Y, "SELECT",
                        renderer->theme.muted, 1u, 8u,
                        renderer->text_context);
    renderer->draw_text(surface, 178, FOOTER_Y, shown->cancel,
                        renderer->theme.accent, 1u, 10u,
                        renderer->text_context);
    renderer->draw_text(surface, 250, FOOTER_Y, "BACK",
                        renderer->theme.muted, 1u, 6u,
                        renderer->text_context);
}

int retropicoes_menu_render(const retropicoes_menu_renderer_t *renderer,
                            retropicoes_menu_surface_t *surface,
                            const retropicoes_menu_view_t *view)
{
    size_t visible;
    size_t row;

    if (!valid(renderer, surface, view)) {
        return 0;
    }

    fill_rect(surface, 0, 0, (int)surface->width, (int)surface->height,
              renderer->theme.background);
    draw_centered(renderer, surface, TITLE_Y,
                  view->title != NULL ? view->title : "SYSTEM MENU",
                  renderer->theme.accent, 2u, 25u);
    fill_rect(surface, 8, 34, 304, 2, renderer->theme.accent);

    visible = view->row_count - view->first_visible;
    if (visible > view->visible_rows) {
        visible = view->visible_rows;
    }
    for (row = 0u; row < visible; ++row) {
        size_t index = view->first_visible + row;
        int y = LIST_Y + (int)row * ROW_HEIGHT;
        int selected = index == view->selected;
        if (selected) {
            fill_rect(surface, 8, y - 3, 304, 14,
                      renderer->theme.selection_background);
        }
        renderer->draw_text(surface, 12, y, selected ? ">" : " ",
                            selected ? renderer->theme.accent
                                     : renderer->theme.muted,
                            1u, 1u, renderer->text_context);
        renderer->draw_text(surface, 24, y,
                            view->rows[index] != NULL ? view->rows[index] : "",
                            selected ? renderer->theme.selection_foreground
                                     : renderer->theme.foreground,
                            1u, 46u, renderer->text_context);
    }

    if (view->first_visible > 0u) {
        renderer->draw_text(surface, 278, 38, "PREV",
                            renderer->theme.muted, 1u, 4u,
                            renderer->text_context);
    }
    if (view->first_visible + visible < view->row_count) {
        renderer->draw_text(surface, 278, 204, "NEXT",
                            renderer->theme.muted, 1u, 4u,
                            renderer->text_context);
    }
    draw_footer(renderer, surface, view->labels);

    if (view->confirmation != NULL) {
        fill_rect(surface, 24, 72, 272, 96,
                  renderer->theme.selection_background);
        border(surface, 24, 72, 272, 96, renderer->theme.danger);
        draw_centered(renderer, surface, 84, "CONFIRM",
                      renderer->theme.danger, 2u, 18u);
        draw_centered(renderer, surface, 116, view->confirmation,
                      renderer->theme.selection_foreground, 1u, 42u);
        draw_centered(renderer, surface, 142, "SELECT OR GO BACK",
                      renderer->theme.muted, 1u, 32u);
    }
    return 1;
}

retropicoes_menu_theme_t retropicoes_menu_default_theme(void)
{
    retropicoes_menu_theme_t theme = {
        0x0000u, 0xFFFFu, 0xBDF7u, 0x5D7Fu,
        0x2104u, 0xFFFFu, 0xF800u
    };
    return theme;
}

int retropicoes_system_menu_render(
    const retropicoes_menu_renderer_t *renderer,
    retropicoes_menu_surface_t *surface,
    const retropicoes_system_menu_state_t *state,
    const retropicoes_controller_labels_t *labels,
    const char *title)
{
    const char *rows[RETROPICOES_MENU_MAX_SYSTEM_ITEMS];
    retropicoes_menu_view_t view;
    size_t i;

    if (state == NULL || !state->open) {
        return 0;
    }
    for (i = 0u; i < state->item_count; ++i) {
        rows[i] = retropicoes_menu_item_name(state->items[i]);
    }
    view.title = title;
    view.rows = rows;
    view.row_count = state->item_count;
    view.selected = state->list.selected;
    view.first_visible = state->list.first_visible;
    view.visible_rows = retropicoes_menu_list_visible_count(&state->list);
    view.labels = labels;
    view.confirmation = state->confirmation_active
                            ? retropicoes_menu_item_name(
                                  state->confirmation_item)
                            : NULL;
    return retropicoes_menu_render(renderer, surface, &view);
}
