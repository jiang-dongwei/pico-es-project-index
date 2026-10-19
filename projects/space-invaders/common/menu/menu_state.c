#include "menu_state.h"

#include <string.h>

static retropicoes_menu_event_t event(retropicoes_menu_event_type_t type,
                                      size_t selected)
{
    retropicoes_menu_event_t result = {type, selected};
    return result;
}

static void keep_selection_visible(retropicoes_menu_list_state_t *state)
{
    if (state->item_count == 0u) {
        state->selected = 0u;
        state->first_visible = 0u;
        return;
    }
    if (state->selected >= state->item_count) {
        state->selected = state->item_count - 1u;
    }
    state->first_visible = (state->selected / state->page_rows) *
                           state->page_rows;
}

void retropicoes_menu_list_init(retropicoes_menu_list_state_t *state,
                                size_t item_count,
                                size_t page_rows)
{
    if (state == NULL) {
        return;
    }
    state->item_count = item_count;
    state->selected = 0u;
    state->first_visible = 0u;
    state->page_rows = page_rows != 0u ? page_rows
                                      : RETROPICOES_MENU_DEFAULT_PAGE_ROWS;
}

void retropicoes_menu_list_set_count(retropicoes_menu_list_state_t *state,
                                     size_t item_count)
{
    if (state == NULL) {
        return;
    }
    state->item_count = item_count;
    keep_selection_visible(state);
}

size_t retropicoes_menu_list_visible_count(
    const retropicoes_menu_list_state_t *state)
{
    size_t remaining;

    if (state == NULL || state->item_count == 0u ||
        state->first_visible >= state->item_count) {
        return 0u;
    }
    remaining = state->item_count - state->first_visible;
    return remaining < state->page_rows ? remaining : state->page_rows;
}

retropicoes_menu_event_t
retropicoes_menu_list_update(retropicoes_menu_list_state_t *state,
                             retropicoes_ui_actions_t pressed_actions)
{
    if (state == NULL) {
        return event(RETROPICOES_MENU_EVENT_NONE, 0u);
    }
    if ((pressed_actions & (RETROPICOES_UI_CANCEL |
                            RETROPICOES_UI_MENU)) != 0u) {
        return event(RETROPICOES_MENU_EVENT_CLOSE, state->selected);
    }
    if (state->item_count == 0u) {
        return event(RETROPICOES_MENU_EVENT_NONE, 0u);
    }
    if ((pressed_actions & RETROPICOES_UI_CONFIRM) != 0u) {
        return event(RETROPICOES_MENU_EVENT_ACTIVATE, state->selected);
    }
    if ((pressed_actions & RETROPICOES_UI_UP) != 0u) {
        state->selected = state->selected == 0u
                              ? state->item_count - 1u
                              : state->selected - 1u;
    } else if ((pressed_actions & RETROPICOES_UI_DOWN) != 0u) {
        state->selected = (state->selected + 1u) % state->item_count;
    } else if ((pressed_actions & RETROPICOES_UI_LEFT) != 0u) {
        state->selected = state->selected >= state->page_rows
                              ? state->selected - state->page_rows
                              : state->item_count - 1u;
    } else if ((pressed_actions & RETROPICOES_UI_RIGHT) != 0u) {
        state->selected = state->selected + state->page_rows < state->item_count
                              ? state->selected + state->page_rows
                              : 0u;
    } else {
        return event(RETROPICOES_MENU_EVENT_NONE, state->selected);
    }
    keep_selection_visible(state);
    return event(RETROPICOES_MENU_EVENT_REDRAW, state->selected);
}

int retropicoes_menu_item_needs_confirmation(
    retropicoes_menu_item_id_t item)
{
    return item == RETROPICOES_MENU_ITEM_RESET ||
           item == RETROPICOES_MENU_ITEM_RETURN_LOADER;
}

int retropicoes_system_menu_open(retropicoes_system_menu_state_t *state,
                                 const retropicoes_menu_ops_t *ops,
                                 size_t page_rows)
{
    if (state == NULL) {
        return 0;
    }
    memset(state, 0, sizeof(*state));
    state->item_count = retropicoes_menu_build_items(
        ops, state->items, RETROPICOES_MENU_MAX_SYSTEM_ITEMS);
    if (state->item_count > RETROPICOES_MENU_MAX_SYSTEM_ITEMS) {
        state->item_count = RETROPICOES_MENU_MAX_SYSTEM_ITEMS;
    }
    retropicoes_menu_list_init(&state->list, state->item_count, page_rows);
    state->open = state->item_count != 0u;
    return state->open;
}

static retropicoes_system_menu_event_t
system_event(retropicoes_menu_event_type_t type,
             retropicoes_menu_item_id_t item)
{
    retropicoes_system_menu_event_t result = {type, item};
    return result;
}

retropicoes_system_menu_event_t
retropicoes_system_menu_update(retropicoes_system_menu_state_t *state,
                               retropicoes_ui_actions_t pressed_actions)
{
    retropicoes_menu_event_t list_event;
    retropicoes_menu_item_id_t item;

    if (state == NULL || !state->open || state->item_count == 0u) {
        return system_event(RETROPICOES_MENU_EVENT_NONE,
                            RETROPICOES_MENU_ITEM_COUNT);
    }

    if (state->confirmation_active) {
        if ((pressed_actions & (RETROPICOES_UI_CANCEL |
                                RETROPICOES_UI_MENU)) != 0u) {
            state->confirmation_active = 0;
            return system_event(RETROPICOES_MENU_EVENT_REDRAW,
                                state->confirmation_item);
        }
        if ((pressed_actions & RETROPICOES_UI_CONFIRM) != 0u) {
            state->confirmation_active = 0;
            return system_event(RETROPICOES_MENU_EVENT_ACTIVATE,
                                state->confirmation_item);
        }
        return system_event(RETROPICOES_MENU_EVENT_NONE,
                            state->confirmation_item);
    }

    list_event = retropicoes_menu_list_update(&state->list, pressed_actions);
    item = state->items[state->list.selected];
    if (list_event.type == RETROPICOES_MENU_EVENT_CLOSE) {
        state->open = 0;
        return system_event(RETROPICOES_MENU_EVENT_CLOSE, item);
    }
    if (list_event.type != RETROPICOES_MENU_EVENT_ACTIVATE) {
        return system_event(list_event.type, item);
    }
    if (retropicoes_menu_item_needs_confirmation(item)) {
        state->confirmation_active = 1;
        state->confirmation_item = item;
        return system_event(RETROPICOES_MENU_EVENT_CONFIRM, item);
    }
    return system_event(RETROPICOES_MENU_EVENT_ACTIVATE, item);
}

