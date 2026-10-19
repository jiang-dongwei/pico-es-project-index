#include "rom_browser.h"

#include <string.h>

static const char default_demo_label[] = "DEMO / TEST GAME (SIMULATED)";
static const char default_empty_message[] = "NO COMPATIBLE ROMS FOUND";

static retropicoes_rom_browser_event_t
browser_event(retropicoes_rom_browser_event_type_t type,
              size_t source_index,
              retropicoes_rom_item_kind_t item_kind)
{
    retropicoes_rom_browser_event_t value = {type, source_index, item_kind};
    return value;
}

int retropicoes_rom_source_valid(const retropicoes_rom_source_t *source)
{
    return source != NULL &&
           source->version == RETROPICOES_ROM_SOURCE_VERSION &&
           source->struct_size >= sizeof(*source) &&
           source->count != NULL && source->item != NULL;
}

int retropicoes_rom_browser_refresh(retropicoes_rom_browser_t *browser)
{
    size_t visible_count;

    if (browser == NULL || !browser->open ||
        !retropicoes_rom_source_valid(&browser->source)) {
        return 0;
    }
    browser->source_count = browser->source.count(browser->source.context);
    browser->showing_demo = browser->source_count == 0u;
    visible_count = browser->showing_demo ? 1u : browser->source_count;
    retropicoes_menu_list_set_count(&browser->list, visible_count);
    return 1;
}

int retropicoes_rom_browser_open(retropicoes_rom_browser_t *browser,
                                 const retropicoes_rom_source_t *source,
                                 size_t page_rows)
{
    if (browser == NULL || !retropicoes_rom_source_valid(source)) {
        return 0;
    }
    memset(browser, 0, sizeof(*browser));
    browser->source = *source;
    browser->open = 1;
    retropicoes_menu_list_init(&browser->list, 0u,
        page_rows != 0u ? page_rows :
                          RETROPICOES_ROM_BROWSER_DEFAULT_PAGE_ROWS);
    return retropicoes_rom_browser_refresh(browser);
}

int retropicoes_rom_browser_item(const retropicoes_rom_browser_t *browser,
                                 size_t visible_index,
                                 retropicoes_rom_item_view_t *item,
                                 size_t *source_index)
{
    if (browser == NULL || item == NULL || !browser->open ||
        visible_index >= browser->list.item_count) {
        return 0;
    }
    if (browser->showing_demo) {
        item->label = browser->source.demo_label != NULL
                          ? browser->source.demo_label
                          : default_demo_label;
        item->kind = RETROPICOES_ROM_ITEM_DEMO;
        if (source_index != NULL) {
            *source_index = RETROPICOES_ROM_BROWSER_DEMO_INDEX;
        }
        return 1;
    }
    if (!browser->source.item(browser->source.context, visible_index, item) ||
        item->label == NULL ||
        (item->kind != RETROPICOES_ROM_ITEM_ROM &&
         item->kind != RETROPICOES_ROM_ITEM_DIRECTORY)) {
        return 0;
    }
    if (source_index != NULL) {
        *source_index = visible_index;
    }
    return 1;
}

retropicoes_rom_browser_event_t
retropicoes_rom_browser_update(retropicoes_rom_browser_t *browser,
                               retropicoes_ui_actions_t actions)
{
    retropicoes_menu_event_t event;
    retropicoes_rom_item_view_t item;
    size_t source_index;

    if (browser == NULL || !browser->open) {
        return browser_event(RETROPICOES_ROM_BROWSER_EVENT_ERROR,
                             RETROPICOES_ROM_BROWSER_DEMO_INDEX,
                             RETROPICOES_ROM_ITEM_DEMO);
    }
    if ((actions & RETROPICOES_UI_MENU) != 0u) {
        return browser_event(RETROPICOES_ROM_BROWSER_EVENT_SETTINGS,
                             RETROPICOES_ROM_BROWSER_DEMO_INDEX,
                             RETROPICOES_ROM_ITEM_DEMO);
    }
    if ((actions & RETROPICOES_UI_CANCEL) != 0u) {
        return browser_event(RETROPICOES_ROM_BROWSER_EVENT_BACK,
                             RETROPICOES_ROM_BROWSER_DEMO_INDEX,
                             RETROPICOES_ROM_ITEM_DEMO);
    }
    if (browser->list.item_count != 0u &&
        (actions & (RETROPICOES_UI_LEFT | RETROPICOES_UI_RIGHT)) != 0u) {
        size_t selected = browser->list.selected;
        const size_t page = browser->list.page_rows;

        if ((actions & RETROPICOES_UI_LEFT) != 0u) {
            selected = selected >= page ? selected - page : 0u;
        } else {
            selected += page;
            if (selected >= browser->list.item_count)
                selected = browser->list.item_count - 1u;
        }
        if (selected != browser->list.selected) {
            browser->list.selected = selected;
            browser->list.first_visible = (selected / page) * page;
            if (!retropicoes_rom_browser_item(browser, selected, &item,
                                              &source_index)) {
                return browser_event(RETROPICOES_ROM_BROWSER_EVENT_ERROR,
                                     selected, RETROPICOES_ROM_ITEM_ROM);
            }
            return browser_event(RETROPICOES_ROM_BROWSER_EVENT_REDRAW,
                                 source_index, item.kind);
        }
        return browser_event(RETROPICOES_ROM_BROWSER_EVENT_NONE,
                             selected, RETROPICOES_ROM_ITEM_ROM);
    }

    event = retropicoes_menu_list_update(
        &browser->list,
        actions & ~(RETROPICOES_UI_MENU | RETROPICOES_UI_CANCEL |
                    RETROPICOES_UI_LEFT | RETROPICOES_UI_RIGHT));
    if (event.type == RETROPICOES_MENU_EVENT_REDRAW) {
        return browser_event(RETROPICOES_ROM_BROWSER_EVENT_REDRAW,
                             event.selected, RETROPICOES_ROM_ITEM_ROM);
    }
    if (event.type != RETROPICOES_MENU_EVENT_ACTIVATE) {
        return browser_event(RETROPICOES_ROM_BROWSER_EVENT_NONE,
                             browser->list.selected,
                             RETROPICOES_ROM_ITEM_ROM);
    }
    if (!retropicoes_rom_browser_item(browser, event.selected, &item,
                                      &source_index)) {
        return browser_event(RETROPICOES_ROM_BROWSER_EVENT_ERROR,
                             event.selected, RETROPICOES_ROM_ITEM_ROM);
    }
    return browser_event(RETROPICOES_ROM_BROWSER_EVENT_OPEN,
                         source_index, item.kind);
}

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

    if (x1 > (int)surface->width) x1 = (int)surface->width;
    if (y1 > (int)surface->height) y1 = (int)surface->height;
    for (py = y0; py < y1; ++py) {
        int px;
        uint16_t *line = surface->pixels + (size_t)py * surface->stride;
        for (px = x0; px < x1; ++px) line[px] = color;
    }
}

static int centered_text_x(const char *text, unsigned scale, size_t max_chars)
{
    size_t length = text != NULL ? strlen(text) : 0u;
    size_t width;

    if (length > max_chars) length = max_chars;
    width = length * 8u * (size_t)scale;
    return width < RETROPICOES_MENU_WIDTH
               ? (int)(RETROPICOES_MENU_WIDTH - width) / 2
               : 0;
}

int retropicoes_rom_browser_render(
    const retropicoes_rom_browser_t *browser,
    const retropicoes_menu_renderer_t *renderer,
    retropicoes_menu_surface_t *surface,
    const retropicoes_controller_labels_t *labels)
{
    const retropicoes_controller_labels_t *shown = labels;
    const char *title;
    const char *path = "/";
    const char *empty_message;
    size_t visible;
    size_t row;

    if (browser == NULL || !browser->open || renderer == NULL ||
        renderer->draw_text == NULL || surface == NULL ||
        surface->pixels == NULL ||
        surface->width != RETROPICOES_MENU_WIDTH ||
        surface->height != RETROPICOES_MENU_HEIGHT ||
        surface->stride < surface->width) {
        return 0;
    }
    if (shown == NULL) {
        shown = retropicoes_menu_controller_labels(
            RETROPICOES_CONTROLLER_XBOX);
    }
    if (browser->source.path != NULL) {
        const char *reported = browser->source.path(browser->source.context);
        if (reported != NULL && *reported != '\0') path = reported;
    }
    empty_message = browser->source.empty_message != NULL
                        ? browser->source.empty_message
                        : default_empty_message;
    title = browser->source.title != NULL
                ? browser->source.title : "SELECT ROM";

    fill_rect(surface, 0, 0, (int)surface->width, (int)surface->height,
              renderer->theme.background);
    renderer->draw_text(surface, centered_text_x(title, 2u, 20u), 4,
                        title,
                        renderer->theme.accent, 2u, 22u,
                        renderer->text_context);
    renderer->draw_text(surface, 6, 20, path, renderer->theme.foreground,
                        1u, 38u, renderer->text_context);
    fill_rect(surface, 0, 30, (int)surface->width, 2,
              renderer->theme.accent);

    if (browser->showing_demo) {
        renderer->draw_text(surface, 58, 36, empty_message,
                            renderer->theme.muted, 1u, 30u,
                            renderer->text_context);
    }
    visible = retropicoes_menu_list_visible_count(&browser->list);
    for (row = 0u; row < visible; ++row) {
        size_t index = browser->list.first_visible + row;
        retropicoes_rom_item_view_t item;
        int y = 52 + (int)row * 8;
        int selected = index == browser->list.selected;

        if (!retropicoes_rom_browser_item(browser, index, &item, NULL)) {
            return 0;
        }
        if (selected) {
            fill_rect(surface, 0, y - 1, (int)surface->width, 8,
                      renderer->theme.selection_background);
        }
        renderer->draw_text(surface, 5, y, selected ? ">" : " ",
                            selected ? renderer->theme.selection_foreground
                                     : renderer->theme.muted,
                            1u, 1u, renderer->text_context);
        renderer->draw_text(surface, 15, y, item.label,
                            selected ? renderer->theme.selection_foreground
                                     : renderer->theme.foreground,
                            1u, 36u, renderer->text_context);
    }

    fill_rect(surface, 0, 207, (int)surface->width, 2,
              renderer->theme.accent);
    renderer->draw_text(surface, 7, 213, shown->confirm,
                        renderer->theme.accent, 1u, 8u,
                        renderer->text_context);
    renderer->draw_text(surface, 22, 213, "OPEN", renderer->theme.muted,
                        1u, 8u, renderer->text_context);
    renderer->draw_text(surface, 100, 213, shown->cancel,
                        renderer->theme.accent, 1u, 8u,
                        renderer->text_context);
    renderer->draw_text(surface, 115, 213, "BACK", renderer->theme.muted,
                        1u, 8u, renderer->text_context);
    renderer->draw_text(surface, 190, 213, shown->menu,
                        renderer->theme.accent, 1u, 8u,
                        renderer->text_context);
    renderer->draw_text(surface, 230, 213, "SETTINGS",
                        renderer->theme.muted, 1u, 10u,
                        renderer->text_context);
    return 1;
}

