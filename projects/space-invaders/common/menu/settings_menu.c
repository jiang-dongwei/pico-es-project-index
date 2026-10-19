#include "settings_menu.h"

#include <stdio.h>
#include <string.h>

enum {
    SETTINGS_TITLE_Y = 0 * 8,
    SETTINGS_UP_Y = 2 * 8,
    SETTINGS_LIST_Y = 3 * 8,
    SETTINGS_ROW_HEIGHT = 8,
    SETTINGS_DOWN_Y = 15 * 8,
    SETTINGS_ACTION_Y = 17 * 8,
    SETTINGS_PREVIEW_Y = 19 * 8,
    SETTINGS_HELP_Y = 24 * 8,
    SETTINGS_HINT_Y = 27 * 8,
    SETTINGS_ACTION_HELP_Y = 28 * 8,
    SETTINGS_BACK_Y = 29 * 8
};

static retropicoes_settings_event_t settings_event(
    retropicoes_settings_event_type_t type, size_t item, int delta)
{
    retropicoes_settings_event_t result = {type, item, delta};
    return result;
}

size_t retropicoes_settings_build_option_order(
    retropicoes_settings_option_id_t *items,
    size_t capacity,
    uint32_t capabilities,
    const retropicoes_settings_option_id_t *custom_items,
    size_t custom_count)
{
    static const struct {
        retropicoes_settings_option_id_t item;
        uint32_t capability;
    } common_order[] = {
        {RETROPICOES_SETTINGS_OPTION_QUIT, RETROPICOES_SETTINGS_CAP_QUIT},
        {RETROPICOES_SETTINGS_OPTION_RESET, RETROPICOES_SETTINGS_CAP_RESET},
        {RETROPICOES_SETTINGS_OPTION_RETURN_LOADER, RETROPICOES_SETTINGS_CAP_RETURN_LOADER},
        {RETROPICOES_SETTINGS_OPTION_SAVE_LOAD, RETROPICOES_SETTINGS_CAP_SAVE_LOAD},
        {RETROPICOES_SETTINGS_OPTION_SCREEN_MODE, RETROPICOES_SETTINGS_CAP_SCREEN_MODE},
        {RETROPICOES_SETTINGS_OPTION_SCANLINES, RETROPICOES_SETTINGS_CAP_SCANLINES},
        {RETROPICOES_SETTINGS_OPTION_FPS, RETROPICOES_SETTINGS_CAP_FPS},
        {RETROPICOES_SETTINGS_OPTION_AUDIO, RETROPICOES_SETTINGS_CAP_AUDIO},
        {RETROPICOES_SETTINGS_OPTION_MENU_FOREGROUND, RETROPICOES_SETTINGS_CAP_MENU_COLORS},
        {RETROPICOES_SETTINGS_OPTION_MENU_BACKGROUND, RETROPICOES_SETTINGS_CAP_MENU_COLORS},
        {RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST, RETROPICOES_SETTINGS_CAP_CONTROLLER_TEST}
    };
    size_t count = 0u;
    size_t index;
    if (items == NULL && capacity != 0u) return 0u;
    for (index = 0u; index < sizeof(common_order) / sizeof(common_order[0]); ++index) {
        if ((capabilities & common_order[index].capability) != 0u) {
            if (count >= capacity) return count;
            items[count++] = common_order[index].item;
        }
    }
    for (index = 0u; index < custom_count; ++index) {
        if (count >= capacity || custom_items == NULL) return count;
        items[count++] = custom_items[index];
    }
    return count;
}

const char *retropicoes_settings_common_label(
    retropicoes_settings_option_id_t item)
{
    static const char *const labels[] = {
        RETROPICOES_SETTINGS_LABEL_QUIT_GAME,
        RETROPICOES_SETTINGS_LABEL_RESET_GAME,
        RETROPICOES_SETTINGS_LABEL_RETURN_LOADER,
        RETROPICOES_SETTINGS_LABEL_SAVE_LOAD,
        RETROPICOES_SETTINGS_LABEL_SCREEN_MODE,
        RETROPICOES_SETTINGS_LABEL_SCANLINES,
        RETROPICOES_SETTINGS_LABEL_FPS,
        RETROPICOES_SETTINGS_LABEL_AUDIO,
        RETROPICOES_SETTINGS_LABEL_MENU_FOREGROUND,
        RETROPICOES_SETTINGS_LABEL_MENU_BACKGROUND,
        RETROPICOES_SETTINGS_LABEL_CONTROLLER_TEST
    };
    if ((unsigned)item > (unsigned)RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST)
        return NULL;
    return labels[(unsigned)item];
}

const char *retropicoes_settings_common_help(
    retropicoes_settings_option_id_t item)
{
    static const char *const help[] = {
        "EXIT GAME AND RETURN TO ROM LIST", "RESTART THE CURRENT GAME",
        "EXIT APP AND RETURN TO LOADER", "SAVE OR LOAD THE CURRENT GAME",
        "CHANGE THE VIDEO SCALING MODE", "DARKEN ALTERNATE VIDEO LINES",
        "SHOW FRAMES PER SECOND", "ENABLE GAME AUDIO", "ROM MENU TEXT COLOR",
        "ROM MENU BACKGROUND COLOR", "SHOW LIVE CONTROLLER INPUT"
    };
    if ((unsigned)item > (unsigned)RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST)
        return NULL;
    return help[(unsigned)item];
}

int retropicoes_settings_menu_open(retropicoes_settings_menu_state_t *state,
                                   size_t item_count,
                                   size_t page_rows)
{
    if (state == NULL) return 0;
    memset(state, 0, sizeof(*state));
    retropicoes_menu_list_init(
        &state->list, item_count,
        page_rows != 0u ? page_rows : RETROPICOES_SETTINGS_DEFAULT_PAGE_ROWS);
    state->on_action_row = item_count == 0u;
    state->open = 1;
    return 1;
}

retropicoes_settings_event_t retropicoes_settings_menu_update(
    retropicoes_settings_menu_state_t *state,
    retropicoes_ui_actions_t actions)
{
    size_t selected;
    if (state == NULL || !state->open) {
        return settings_event(RETROPICOES_SETTINGS_EVENT_NONE, 0u, 0);
    }
    selected = state->list.selected;
    if ((actions & (RETROPICOES_UI_CANCEL | RETROPICOES_UI_MENU)) != 0u) {
        state->open = 0;
        return settings_event(RETROPICOES_SETTINGS_EVENT_CANCEL, selected, 0);
    }
    if ((actions & RETROPICOES_UI_UP) != 0u) {
        if (state->on_action_row && state->list.item_count != 0u) {
            state->on_action_row = 0;
            state->list.selected = state->list.item_count - 1u;
            state->list.first_visible =
                state->list.item_count > state->list.page_rows
                    ? state->list.item_count - state->list.page_rows : 0u;
        } else if (!state->on_action_row) {
            if (state->list.selected == 0u) {
                state->on_action_row = 1;
            } else {
                --state->list.selected;
            }
        }
        selected = state->list.selected;
        goto selection_changed;
    }
    if ((actions & RETROPICOES_UI_DOWN) != 0u) {
        if (state->on_action_row && state->list.item_count != 0u) {
            state->on_action_row = 0;
            state->list.selected = 0u;
            state->list.first_visible = 0u;
        } else if (!state->on_action_row) {
            if (state->list.selected + 1u < state->list.item_count) {
                ++state->list.selected;
            } else {
                state->on_action_row = 1;
            }
        }
        selected = state->list.selected;
        goto selection_changed;
    }
    if ((actions & (RETROPICOES_UI_LEFT | RETROPICOES_UI_RIGHT)) != 0u) {
        const int delta = (actions & RETROPICOES_UI_LEFT) != 0u ? -1 : 1;
        if (state->on_action_row) {
            state->action = (size_t)(((int)state->action +
                                      (int)RETROPICOES_SETTINGS_ACTION_COUNT +
                                      delta) %
                                     (int)RETROPICOES_SETTINGS_ACTION_COUNT);
            return settings_event(RETROPICOES_SETTINGS_EVENT_REDRAW,
                                  selected, delta);
        }
        return settings_event(RETROPICOES_SETTINGS_EVENT_CHANGE,
                              selected, delta);
    }
    if ((actions & RETROPICOES_UI_CONFIRM) != 0u) {
        if (!state->on_action_row) {
            return settings_event(RETROPICOES_SETTINGS_EVENT_ACTIVATE,
                                  selected, 1);
        }
        if (state->action == RETROPICOES_SETTINGS_ACTION_SAVE) {
            state->open = 0;
            return settings_event(RETROPICOES_SETTINGS_EVENT_SAVE,
                                  selected, 0);
        }
        if (state->action == RETROPICOES_SETTINGS_ACTION_CANCEL) {
            state->open = 0;
            return settings_event(RETROPICOES_SETTINGS_EVENT_CANCEL,
                                  selected, 0);
        }
        return settings_event(RETROPICOES_SETTINGS_EVENT_DEFAULT,
                              selected, 0);
    }
    return settings_event(RETROPICOES_SETTINGS_EVENT_NONE, selected, 0);

selection_changed:
    if (!state->on_action_row) {
        if (state->list.selected < state->list.first_visible) {
            state->list.first_visible = state->list.selected;
        }
        if (state->list.selected >= state->list.first_visible +
                                        state->list.page_rows) {
            state->list.first_visible = state->list.selected -
                                        state->list.page_rows + 1u;
        }
    }
    return settings_event(RETROPICOES_SETTINGS_EVENT_REDRAW, selected, 0);
}

static void fill_rect(retropicoes_menu_surface_t *surface,
                      int x, int y, int width, int height, uint16_t color)
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

static void centered_text(const retropicoes_menu_renderer_t *renderer,
                          retropicoes_menu_surface_t *surface, int y,
                          const char *value, uint16_t color, size_t max_chars)
{
    size_t length = value != NULL ? strlen(value) : 0u;
    int x;
    if (length > max_chars) length = max_chars;
    x = (320 - (int)length * 8) / 2;
    renderer->draw_text(surface, x < 0 ? 0 : x, y, value, color, 1u,
                        max_chars, renderer->text_context);
}

int retropicoes_settings_menu_render(
    const retropicoes_menu_renderer_t *renderer,
    retropicoes_menu_surface_t *surface,
    const retropicoes_settings_menu_state_t *state,
    const retropicoes_settings_view_t *view)
{
    static const char *const action_names[] = {"SAVE", "CANCEL", "DEFAULT"};
    static const char *const action_help[] = {
        "Confirm changes", "Discard changes", "Restore defaults"
    };
    const retropicoes_controller_labels_t *labels;
    size_t visible;
    size_t row;
    char hint[64];
    char line[64];
    if (renderer == NULL || renderer->draw_text == NULL || surface == NULL ||
        surface->pixels == NULL || surface->width != RETROPICOES_MENU_WIDTH ||
        surface->height != RETROPICOES_MENU_HEIGHT ||
        surface->stride < surface->width || state == NULL || view == NULL ||
        view->item_count != state->list.item_count ||
        (view->item_count != 0u && view->items == NULL)) return 0;

    labels = view->labels != NULL ? view->labels :
        retropicoes_menu_controller_labels(RETROPICOES_CONTROLLER_UNKNOWN);
    fill_rect(surface, 0, 0, 320, 240, 0xffffu);
    centered_text(renderer, surface, SETTINGS_TITLE_Y, "-- Settings --",
                  0x0000u, 22u);

    visible = retropicoes_menu_list_visible_count(&state->list);
    for (row = 0u; row < visible; ++row) {
        const size_t index = state->list.first_visible + row;
        const int y = SETTINGS_LIST_Y + (int)row * SETTINGS_ROW_HEIGHT;
        const int active = !state->on_action_row &&
                           state->list.selected == index;
        const uint16_t color = active ? 0xffffu : 0x0000u;
        const char *value = view->items[index].value;
        if (active) fill_rect(surface, 0, y, 320, 8, 0x0000u);
        (void)snprintf(line, sizeof(line), "%s%s%s",
                       view->items[index].label,
                       value != NULL && value[0] != '\0' ? ": " : "",
                       value != NULL ? value : "");
        renderer->draw_text(surface, 0, y, line, color, 1u, 40u,
                            renderer->text_context);
    }
    if (state->list.first_visible != 0u) {
        renderer->draw_text(surface, 160, SETTINGS_UP_Y, "^", 0x0000u,
                            1u, 1u,
                            renderer->text_context);
    }
    if (state->list.first_visible + visible < state->list.item_count) {
        renderer->draw_text(surface, 160, SETTINGS_DOWN_Y, "v", 0x0000u,
                            1u, 1u,
                            renderer->text_context);
    }
    {
        const char *shown_names[RETROPICOES_SETTINGS_ACTION_COUNT];
        int action_x[RETROPICOES_SETTINGS_ACTION_COUNT];
        int total_width;
        shown_names[0] = view->settings_changed ? "SAVE*" : action_names[0];
        shown_names[1] = action_names[1];
        shown_names[2] = action_names[2];
        total_width = ((int)strlen(shown_names[0]) +
                       (int)strlen(shown_names[1]) +
                       (int)strlen(shown_names[2]) + 4) * 8;
        action_x[0] = (320 - total_width) / 2;
        action_x[1] = action_x[0] +
                      ((int)strlen(shown_names[0]) + 2) * 8;
        action_x[2] = action_x[1] +
                      ((int)strlen(shown_names[1]) + 2) * 8;
        for (row = 0u; row < RETROPICOES_SETTINGS_ACTION_COUNT; ++row) {
        const int x = action_x[row];
        const int active = state->on_action_row && state->action == row;
        const int width = (int)strlen(shown_names[row]) * 8;
        if (active) fill_rect(surface, x, SETTINGS_ACTION_Y, width, 8,
                              0x0000u);
        renderer->draw_text(surface, x, SETTINGS_ACTION_Y, shown_names[row],
                            active ? 0xffffu : 0x0000u, 1u, 10u,
                            renderer->text_context);
        }
    }
    if (view->preview_colors != NULL && view->preview_color_count != 0u) {
        const int start_x = 96;
        for (row = 0u; row < view->preview_color_count; ++row) {
            const int x = start_x + (int)(row % 16u) * 8;
            const int y = SETTINGS_PREVIEW_Y + (int)(row / 16u) * 8;
            if ((row % 16u) == 0u) {
                (void)snprintf(line, sizeof(line), "%02u", (unsigned)row);
                renderer->draw_text(surface, 80, y, line, 0x0000u, 1u, 2u,
                                    renderer->text_context);
            }
            fill_rect(surface, x, y, 8, 8,
                      view->preview_colors[row]);
        }
        (void)snprintf(line, sizeof(line), "FG=%02u",
                       (unsigned)view->foreground_color);
        fill_rect(surface, 232, SETTINGS_PREVIEW_Y, 40, 8,
                  view->preview_colors[
                      view->background_color % view->preview_color_count]);
        renderer->draw_text(surface, 232, SETTINGS_PREVIEW_Y, line,
                            view->preview_colors[
                                view->foreground_color %
                                view->preview_color_count],
                            1u, 5u, renderer->text_context);
        (void)snprintf(line, sizeof(line), "BG=%02u",
                       (unsigned)view->background_color);
        fill_rect(surface, 232, SETTINGS_PREVIEW_Y + 8, 40, 8,
                  view->preview_colors[
                      view->background_color % view->preview_color_count]);
        renderer->draw_text(surface, 232, SETTINGS_PREVIEW_Y + 8, line,
                            view->preview_colors[
                                view->foreground_color %
                                view->preview_color_count],
                            1u, 5u, renderer->text_context);
    }
    if (!state->on_action_row && view->item_count != 0u) {
        renderer->draw_text(surface, 0, SETTINGS_HELP_Y,
                            view->items[state->list.selected].help,
                            0x0000u, 1u, 40u, renderer->text_context);
    }
    if (state->on_action_row) {
        (void)snprintf(hint, sizeof(hint),
                       "LEFT/RIGHT: Select, %s: Confirm", labels->confirm);
    } else if (view->item_count != 0u &&
               view->items[state->list.selected].value != NULL &&
               view->items[state->list.selected].value[0] != '\0') {
        (void)snprintf(hint, sizeof(hint),
                       "UP/DOWN: Move, LEFT/RIGHT: Change");
    } else {
        (void)snprintf(hint, sizeof(hint),
                       "UP/DOWN: Move, %s: select", labels->confirm);
    }
    centered_text(renderer, surface, SETTINGS_HINT_Y, hint, 0x0000u, 53u);
    if (state->on_action_row) {
        (void)snprintf(hint, sizeof(hint), "%s: %s", labels->confirm,
                       action_help[state->action]);
        centered_text(renderer, surface, SETTINGS_ACTION_HELP_Y, hint,
                      0x0000u, 40u);
    }
    (void)snprintf(hint, sizeof(hint), "Press %s to go back.",
                   labels->cancel);
    centered_text(renderer, surface, SETTINGS_BACK_Y, hint, 0x0000u, 53u);
    return 1;
}

