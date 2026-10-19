#include "settings_menu.h"

#include <stdio.h>
#include <string.h>

static int failures;

static int draw_calls;
static int saw_save_star;
static int saw_palette_48;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static void test_common_option_order_and_visibility(void)
{
    retropicoes_settings_option_id_t items[16];
    const retropicoes_settings_option_id_t custom[] = {
        RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE,
        (retropicoes_settings_option_id_t)(RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE + 1)
    };
    const uint32_t capabilities = RETROPICOES_SETTINGS_CAP_QUIT |
        RETROPICOES_SETTINGS_CAP_RESET |
        RETROPICOES_SETTINGS_CAP_RETURN_LOADER |
        RETROPICOES_SETTINGS_CAP_SCANLINES |
        RETROPICOES_SETTINGS_CAP_FPS |
        RETROPICOES_SETTINGS_CAP_AUDIO |
        RETROPICOES_SETTINGS_CAP_MENU_COLORS |
        RETROPICOES_SETTINGS_CAP_CONTROLLER_TEST;
    const size_t count = retropicoes_settings_build_option_order(
        items, 16u, capabilities, custom, 2u);
    CHECK(count == 11u);
    CHECK(items[0] == RETROPICOES_SETTINGS_OPTION_QUIT);
    CHECK(items[1] == RETROPICOES_SETTINGS_OPTION_RESET);
    CHECK(items[2] == RETROPICOES_SETTINGS_OPTION_RETURN_LOADER);
    CHECK(items[3] == RETROPICOES_SETTINGS_OPTION_SCANLINES);
    CHECK(items[4] == RETROPICOES_SETTINGS_OPTION_FPS);
    CHECK(items[5] == RETROPICOES_SETTINGS_OPTION_AUDIO);
    CHECK(items[6] == RETROPICOES_SETTINGS_OPTION_MENU_FOREGROUND);
    CHECK(items[7] == RETROPICOES_SETTINGS_OPTION_MENU_BACKGROUND);
    CHECK(items[8] == RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST);
    CHECK(items[9] == RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE);
    CHECK(items[10] == RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE + 1);
    CHECK(retropicoes_settings_common_label(
              RETROPICOES_SETTINGS_OPTION_SAVE_LOAD) != NULL);
    CHECK(strcmp(retropicoes_settings_common_label(
                     RETROPICOES_SETTINGS_OPTION_QUIT),
                 RETROPICOES_SETTINGS_LABEL_QUIT_GAME) == 0);
    CHECK(strcmp(retropicoes_settings_common_label(
                     RETROPICOES_SETTINGS_OPTION_RETURN_LOADER),
                 RETROPICOES_SETTINGS_LABEL_RETURN_LOADER) == 0);
    CHECK(strcmp(retropicoes_settings_common_label(
                     RETROPICOES_SETTINGS_OPTION_FPS),
                 RETROPICOES_SETTINGS_LABEL_FPS) == 0);

    {
        const uint32_t all = RETROPICOES_SETTINGS_CAP_QUIT |
            RETROPICOES_SETTINGS_CAP_RESET |
            RETROPICOES_SETTINGS_CAP_RETURN_LOADER |
            RETROPICOES_SETTINGS_CAP_SAVE_LOAD |
            RETROPICOES_SETTINGS_CAP_SCREEN_MODE |
            RETROPICOES_SETTINGS_CAP_SCANLINES |
            RETROPICOES_SETTINGS_CAP_FPS |
            RETROPICOES_SETTINGS_CAP_AUDIO |
            RETROPICOES_SETTINGS_CAP_MENU_COLORS |
            RETROPICOES_SETTINGS_CAP_CONTROLLER_TEST;
        CHECK(retropicoes_settings_build_option_order(
                  items, 16u, all, NULL, 0u) == 11u);
        CHECK(items[0] == RETROPICOES_SETTINGS_OPTION_QUIT);
        CHECK(items[1] == RETROPICOES_SETTINGS_OPTION_RESET);
        CHECK(items[2] == RETROPICOES_SETTINGS_OPTION_RETURN_LOADER);
        CHECK(items[3] == RETROPICOES_SETTINGS_OPTION_SAVE_LOAD);
        CHECK(items[4] == RETROPICOES_SETTINGS_OPTION_SCREEN_MODE);
        CHECK(items[5] == RETROPICOES_SETTINGS_OPTION_SCANLINES);
        CHECK(items[6] == RETROPICOES_SETTINGS_OPTION_FPS);
        CHECK(items[7] == RETROPICOES_SETTINGS_OPTION_AUDIO);
        CHECK(items[8] == RETROPICOES_SETTINGS_OPTION_MENU_FOREGROUND);
        CHECK(items[9] == RETROPICOES_SETTINGS_OPTION_MENU_BACKGROUND);
        CHECK(items[10] == RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST);
    }
}

static void draw_text(retropicoes_menu_surface_t *surface, int x, int y,
                      const char *text, uint16_t color, unsigned scale,
                      size_t max_chars, void *context)
{
    (void)surface; (void)x; (void)y; (void)color;
    (void)scale; (void)max_chars; (void)context;
    if (text != NULL && strcmp(text, "SAVE*") == 0) saw_save_star = 1;
    if (text != NULL && strcmp(text, "48") == 0) saw_palette_48 = 1;
    ++draw_calls;
}

static void test_navigation_and_actions(void)
{
    retropicoes_settings_menu_state_t state;
    retropicoes_settings_event_t event;
    CHECK(retropicoes_settings_menu_open(&state, 13u, 11u) == 1);
    CHECK(state.list.selected == 0u && state.list.first_visible == 0u);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_UP);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_REDRAW);
    CHECK(state.on_action_row == 1);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_DOWN);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_REDRAW);
    CHECK(state.on_action_row == 0 && state.list.selected == 0u);
    for (size_t i = 0u; i < 11u; ++i)
        (void)retropicoes_settings_menu_update(&state, RETROPICOES_UI_DOWN);
    CHECK(state.list.selected == 11u && state.list.first_visible == 1u);
    (void)retropicoes_settings_menu_update(&state, RETROPICOES_UI_DOWN);
    CHECK(state.list.selected == 12u && state.list.first_visible == 2u);
    (void)retropicoes_settings_menu_update(&state, RETROPICOES_UI_DOWN);
    CHECK(state.on_action_row == 1);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_LEFT);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_REDRAW);
    CHECK(state.action == RETROPICOES_SETTINGS_ACTION_DEFAULT);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_DEFAULT && state.open == 1);
    (void)retropicoes_settings_menu_update(&state, RETROPICOES_UI_RIGHT);
    CHECK(state.action == RETROPICOES_SETTINGS_ACTION_SAVE);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_SAVE && state.open == 0);
}

static void test_change_activate_and_cancel(void)
{
    retropicoes_settings_menu_state_t state;
    retropicoes_settings_event_t event;
    (void)retropicoes_settings_menu_open(&state, 2u, 11u);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_LEFT);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_CHANGE);
    CHECK(event.item == 0u && event.delta == -1);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE);
    event = retropicoes_settings_menu_update(&state, RETROPICOES_UI_CANCEL);
    CHECK(event.type == RETROPICOES_SETTINGS_EVENT_CANCEL && state.open == 0);
}

static void test_render(void)
{
    uint16_t pixels[RETROPICOES_MENU_WIDTH * RETROPICOES_MENU_HEIGHT];
    uint16_t colors[64];
    retropicoes_settings_item_view_t items[] = {
        {"AUDIO", "ON", "ENABLE AUDIO"},
        {"CONTROLLER TEST", "", "TEST CONTROLLER"}
    };
    retropicoes_settings_menu_state_t state;
    retropicoes_settings_view_t view = {
        items, 2u, NULL, colors, 64u, 15u, 48u, 1
    };
    retropicoes_menu_renderer_t renderer;
    retropicoes_menu_surface_t surface = {
        pixels, RETROPICOES_MENU_WIDTH,
        RETROPICOES_MENU_WIDTH, RETROPICOES_MENU_HEIGHT
    };
    memset(&renderer, 0, sizeof(renderer));
    memset(colors, 0, sizeof(colors));
    colors[48] = 0xffffu;
    renderer.draw_text = draw_text;
    (void)retropicoes_settings_menu_open(&state, 2u, 11u);
    draw_calls = 0;
    saw_save_star = 0;
    saw_palette_48 = 0;
    CHECK(retropicoes_settings_menu_render(&renderer, &surface, &state,
                                           &view) == 1);
    CHECK(draw_calls >= 10);
    CHECK(saw_save_star == 1);
    CHECK(saw_palette_48 == 1);
    CHECK(pixels[0] == 0xffffu);
    CHECK(pixels[24u * RETROPICOES_MENU_WIDTH] == 0x0000u);
    CHECK(pixels[24u * RETROPICOES_MENU_WIDTH + 319u] == 0x0000u);
    CHECK(pixels[31u * RETROPICOES_MENU_WIDTH + 319u] == 0x0000u);
    CHECK(pixels[32u * RETROPICOES_MENU_WIDTH] == 0xffffu);
}

static void test_nintendo_renderer(void)
{
    uint16_t pixels[RETROPICOES_MENU_WIDTH * RETROPICOES_MENU_HEIGHT];
    retropicoes_menu_surface_t surface = {
        pixels, RETROPICOES_MENU_WIDTH,
        RETROPICOES_MENU_WIDTH, RETROPICOES_MENU_HEIGHT
    };
    retropicoes_menu_renderer_t renderer =
        retropicoes_menu_nintendo_renderer();
    memset(pixels, 0xff, sizeof(pixels));
    renderer.draw_text(&surface, 0, 0, "A", 0x0000u, 1u, 1u,
                       renderer.text_context);
    CHECK(renderer.draw_text == retropicoes_menu_nintendo_draw_text);
    CHECK(pixels[0] == 0xffffu);
    CHECK(pixels[2] == 0x0000u);
}

int main(void)
{
    test_common_option_order_and_visibility();
    test_navigation_and_actions();
    test_change_activate_and_cancel();
    test_render();
    test_nintendo_renderer();
    if (failures != 0) return 1;
    puts("common Settings menu tests: PASS");
    return 0;
}
