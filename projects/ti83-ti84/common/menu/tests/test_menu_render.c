#include "menu_render.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

enum { PIXELS = RETROPICOES_MENU_WIDTH * RETROPICOES_MENU_HEIGHT };

typedef struct text_log {
    unsigned calls;
    char left_label[16];
    char right_label[16];
} text_log_t;

static void draw_text(retropicoes_menu_surface_t *surface,
                      int x,
                      int y,
                      const char *text,
                      uint16_t color,
                      unsigned scale,
                      size_t max_chars,
                      void *context)
{
    text_log_t *log = (text_log_t *)context;
    size_t i;

    ++log->calls;
    if (x == 10 && y == 220) {
        snprintf(log->left_label, sizeof(log->left_label), "%s", text);
    }
    if (x == 178 && y == 220) {
        snprintf(log->right_label, sizeof(log->right_label), "%s", text);
    }
    for (i = 0u; text != NULL && text[i] != '\0' && i < max_chars; ++i) {
        int px = x + (int)(i * 6u * scale);
        if (px >= 0 && y >= 0 && px < (int)surface->width &&
            y < (int)surface->height) {
            surface->pixels[(size_t)y * surface->stride + (size_t)px] = color;
        }
    }
}

static retropicoes_menu_renderer_t renderer(text_log_t *log)
{
    retropicoes_menu_renderer_t value;
    value.draw_text = draw_text;
    value.text_context = log;
    value.theme = retropicoes_menu_default_theme();
    return value;
}

static int test_generic_layout_and_bounds(void)
{
    static uint16_t storage[PIXELS + 2u];
    const char *rows[12] = {
        "ROW 0", "ROW 1", "ROW 2", "ROW 3", "ROW 4", "ROW 5",
        "ROW 6", "ROW 7", "ROW 8", "ROW 9", "ROW 10", "ROW 11"
    };
    retropicoes_menu_surface_t surface = {
        storage + 1, RETROPICOES_MENU_WIDTH,
        RETROPICOES_MENU_WIDTH, RETROPICOES_MENU_HEIGHT
    };
    retropicoes_menu_view_t view = {
        "SYSTEM MENU", rows, 12u, 0u, 0u, 10u,
        retropicoes_menu_controller_labels(RETROPICOES_CONTROLLER_NINTENDO),
        NULL
    };
    text_log_t log;
    retropicoes_menu_renderer_t render;

    memset(storage, 0xA5, sizeof(storage));
    memset(&log, 0, sizeof(log));
    render = renderer(&log);
    CHECK(retropicoes_menu_render(&render, &surface, &view));
    CHECK(storage[0] == 0xA5A5u);
    CHECK(storage[PIXELS + 1u] == 0xA5A5u);
    CHECK(surface.pixels[34u * surface.stride + 8u] == render.theme.accent);
    CHECK(surface.pixels[45u * surface.stride + 9u] ==
          render.theme.selection_background);
    CHECK(strcmp(log.left_label, "A") == 0);
    CHECK(strcmp(log.right_label, "B") == 0);
    CHECK(log.calls >= 20u);
    return 0;
}

static int test_validation(void)
{
    uint16_t pixel = 0u;
    const char *row = "ROW";
    retropicoes_menu_surface_t surface = {&pixel, 1u, 1u, 1u};
    retropicoes_menu_view_t view = {
        "MENU", &row, 1u, 0u, 0u, 1u, NULL, NULL
    };
    text_log_t log = {0};
    retropicoes_menu_renderer_t render = renderer(&log);

    CHECK(!retropicoes_menu_render(&render, &surface, &view));
    surface.width = RETROPICOES_MENU_WIDTH;
    surface.height = RETROPICOES_MENU_HEIGHT;
    surface.stride = RETROPICOES_MENU_WIDTH - 1u;
    CHECK(!retropicoes_menu_render(&render, &surface, &view));
    return 0;
}

static retropicoes_menu_result_t callback(void *context)
{
    (void)context;
    return RETROPICOES_MENU_RESULT_OK;
}

static int test_system_confirmation_overlay(void)
{
    static uint16_t pixels[PIXELS];
    retropicoes_menu_surface_t surface = {
        pixels, RETROPICOES_MENU_WIDTH, RETROPICOES_MENU_WIDTH,
        RETROPICOES_MENU_HEIGHT
    };
    retropicoes_menu_ops_t ops;
    retropicoes_system_menu_state_t state;
    text_log_t log = {0};
    retropicoes_menu_renderer_t render = renderer(&log);
    unsigned i;

    memset(&ops, 0, sizeof(ops));
    ops.contract_version = RETROPICOES_MENU_CONTRACT_VERSION;
    ops.struct_size = sizeof(ops);
    ops.capabilities = RETROPICOES_MENU_CAP_RESUME |
                       RETROPICOES_MENU_CAP_RESET;
    ops.resume = callback;
    ops.reset = callback;
    CHECK(retropicoes_system_menu_open(&state, &ops, 7u));
    for (i = 0u; i < 1u; ++i) {
        (void)retropicoes_system_menu_update(&state, RETROPICOES_UI_DOWN);
    }
    CHECK(state.items[state.list.selected] == RETROPICOES_MENU_ITEM_RESET);
    (void)retropicoes_system_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(state.confirmation_active);
    CHECK(retropicoes_system_menu_render(
        &render, &surface, &state,
        retropicoes_menu_controller_labels(RETROPICOES_CONTROLLER_XBOX),
        "QUICK MENU"));
    CHECK(pixels[72u * RETROPICOES_MENU_WIDTH + 24u] == render.theme.danger);
    CHECK(strcmp(log.left_label, "A") == 0);
    CHECK(strcmp(log.right_label, "B") == 0);
    return 0;
}

int main(void)
{
    CHECK(test_generic_layout_and_bounds() == 0);
    CHECK(test_validation() == 0);
    CHECK(test_system_confirmation_overlay() == 0);
    puts("common menu render tests: PASS");
    return 0;
}
