#include "rom_browser.h"

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

typedef struct fake_source {
    size_t count;
    const char *path;
    const char *names[8];
    retropicoes_rom_item_kind_t kinds[8];
} fake_source_t;

static size_t fake_count(void *context)
{
    return ((fake_source_t *)context)->count;
}

static int fake_item(void *context,
                     size_t index,
                     retropicoes_rom_item_view_t *item)
{
    fake_source_t *source = (fake_source_t *)context;
    if (index >= source->count) return 0;
    item->label = source->names[index];
    item->kind = source->kinds[index];
    return 1;
}

static const char *fake_path(void *context)
{
    return ((fake_source_t *)context)->path;
}

static retropicoes_rom_source_t make_source(fake_source_t *context)
{
    retropicoes_rom_source_t source;
    memset(&source, 0, sizeof(source));
    source.version = RETROPICOES_ROM_SOURCE_VERSION;
    source.struct_size = sizeof(source);
    source.context = context;
    source.title = "-- TEST CORE --";
    source.empty_message = "NO TEST ROMS FOUND";
    source.demo_label = "DEMO / TEST GAME (SIMULATED)";
    source.count = fake_count;
    source.item = fake_item;
    source.path = fake_path;
    return source;
}

static int test_empty_source_injects_demo(void)
{
    fake_source_t source_data = {0u, "/roms/TEST", {0}, {0}};
    retropicoes_rom_source_t source = make_source(&source_data);
    retropicoes_rom_browser_t browser;
    retropicoes_rom_item_view_t item;
    retropicoes_rom_browser_event_t event;
    size_t source_index = 0u;

    CHECK(retropicoes_rom_browser_open(&browser, &source, 0u));
    CHECK(browser.showing_demo == 1);
    CHECK(browser.list.item_count == 1u);
    CHECK(retropicoes_rom_browser_item(&browser, 0u, &item, &source_index));
    CHECK(item.kind == RETROPICOES_ROM_ITEM_DEMO);
    CHECK(source_index == RETROPICOES_ROM_BROWSER_DEMO_INDEX);
    CHECK(strcmp(item.label, "DEMO / TEST GAME (SIMULATED)") == 0);
    event = retropicoes_rom_browser_update(&browser,
                                           RETROPICOES_UI_CONFIRM);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_OPEN);
    CHECK(event.item_kind == RETROPICOES_ROM_ITEM_DEMO);
    CHECK(event.source_index == RETROPICOES_ROM_BROWSER_DEMO_INDEX);
    return 0;
}

static int test_navigation_and_public_actions(void)
{
    fake_source_t source_data = {
        3u, "/roms/TEST", {"FOLDER", "ONE.BIN", "TWO.BIN"},
        {RETROPICOES_ROM_ITEM_DIRECTORY, RETROPICOES_ROM_ITEM_ROM,
         RETROPICOES_ROM_ITEM_ROM}
    };
    retropicoes_rom_source_t source = make_source(&source_data);
    retropicoes_rom_browser_t browser;
    retropicoes_rom_browser_event_t event;

    CHECK(retropicoes_rom_browser_open(&browser, &source, 2u));
    event = retropicoes_rom_browser_update(&browser, RETROPICOES_UI_DOWN);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_REDRAW);
    CHECK(browser.list.selected == 1u);
    event = retropicoes_rom_browser_update(&browser,
                                           RETROPICOES_UI_CONFIRM);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_OPEN);
    CHECK(event.source_index == 1u);
    CHECK(event.item_kind == RETROPICOES_ROM_ITEM_ROM);
    event = retropicoes_rom_browser_update(&browser, RETROPICOES_UI_MENU);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_SETTINGS);
    event = retropicoes_rom_browser_update(&browser, RETROPICOES_UI_CANCEL);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_BACK);

    source_data.count = 1u;
    CHECK(retropicoes_rom_browser_refresh(&browser));
    CHECK(browser.list.selected == 0u);
    return 0;
}

static int test_left_right_page_navigation(void)
{
    fake_source_t source_data = {
        5u, "/roms/TEST", {"A", "B", "FOLDER", "D", "E"},
        {RETROPICOES_ROM_ITEM_ROM, RETROPICOES_ROM_ITEM_ROM,
         RETROPICOES_ROM_ITEM_DIRECTORY, RETROPICOES_ROM_ITEM_ROM,
         RETROPICOES_ROM_ITEM_ROM}
    };
    retropicoes_rom_source_t source = make_source(&source_data);
    retropicoes_rom_browser_t browser;
    retropicoes_rom_browser_event_t event;

    CHECK(retropicoes_rom_browser_open(&browser, &source, 2u));
    event = retropicoes_rom_browser_update(&browser, RETROPICOES_UI_RIGHT);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_REDRAW);
    CHECK(browser.list.selected == 2u && browser.list.first_visible == 2u);
    CHECK(event.item_kind == RETROPICOES_ROM_ITEM_DIRECTORY);
    event = retropicoes_rom_browser_update(&browser, RETROPICOES_UI_RIGHT);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_REDRAW);
    CHECK(browser.list.selected == 4u && browser.list.first_visible == 4u);
    event = retropicoes_rom_browser_update(&browser, RETROPICOES_UI_LEFT);
    CHECK(event.type == RETROPICOES_ROM_BROWSER_EVENT_REDRAW);
    CHECK(browser.list.selected == 2u && browser.list.first_visible == 2u);
    return 0;
}

static int saw_title;
static int saw_path;
static int saw_demo;
static int saw_open;
static int saw_back;
static int saw_settings;

static void record_text(retropicoes_menu_surface_t *surface,
                        int x,
                        int y,
                        const char *text,
                        uint16_t color,
                        unsigned scale,
                        size_t max_chars,
                        void *context)
{
    (void)surface;
    (void)x;
    (void)y;
    (void)color;
    (void)scale;
    (void)max_chars;
    (void)context;
    if (strcmp(text, "-- TEST CORE --") == 0) saw_title = 1;
    if (strcmp(text, "/roms/TEST") == 0) saw_path = 1;
    if (strcmp(text, "DEMO / TEST GAME (SIMULATED)") == 0) saw_demo = 1;
    if (strcmp(text, "OPEN") == 0) saw_open = 1;
    if (strcmp(text, "BACK") == 0) saw_back = 1;
    if (strcmp(text, "SETTINGS") == 0) saw_settings = 1;
}

static int test_render_uses_one_public_layout(void)
{
    uint16_t pixels[RETROPICOES_MENU_WIDTH * RETROPICOES_MENU_HEIGHT];
    fake_source_t source_data = {0u, "/roms/TEST", {0}, {0}};
    retropicoes_rom_source_t source = make_source(&source_data);
    retropicoes_rom_browser_t browser;
    retropicoes_menu_renderer_t renderer =
        retropicoes_menu_nintendo_renderer();
    retropicoes_menu_surface_t surface = {
        pixels, RETROPICOES_MENU_WIDTH,
        RETROPICOES_MENU_WIDTH, RETROPICOES_MENU_HEIGHT
    };

    renderer.draw_text = record_text;
    saw_title = saw_path = saw_demo = 0;
    saw_open = saw_back = saw_settings = 0;
    CHECK(retropicoes_rom_browser_open(&browser, &source, 0u));
    CHECK(retropicoes_rom_browser_render(&browser, &renderer, &surface,
                                         NULL));
    CHECK(saw_title && saw_path && saw_demo);
    CHECK(saw_open && saw_back && saw_settings);
    CHECK(pixels[0] == renderer.theme.background);
    CHECK(pixels[30u * RETROPICOES_MENU_WIDTH] == renderer.theme.accent);
    CHECK(pixels[207u * RETROPICOES_MENU_WIDTH] == renderer.theme.accent);
    return 0;
}

int main(void)
{
    if (test_empty_source_injects_demo() != 0) return 1;
    if (test_navigation_and_public_actions() != 0) return 1;
    if (test_left_right_page_navigation() != 0) return 1;
    if (test_render_uses_one_public_layout() != 0) return 1;
    puts("common ROM browser tests: PASS");
    return 0;
}
