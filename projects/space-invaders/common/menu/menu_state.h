#ifndef RETROPICOES_COMMON_MENU_STATE_H
#define RETROPICOES_COMMON_MENU_STATE_H

#include <stddef.h>

#include "menu_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_MENU_MAX_SYSTEM_ITEMS RETROPICOES_MENU_ITEM_COUNT
#define RETROPICOES_MENU_DEFAULT_PAGE_ROWS 10u

typedef enum retropicoes_menu_event_type {
    RETROPICOES_MENU_EVENT_NONE = 0,
    RETROPICOES_MENU_EVENT_REDRAW,
    RETROPICOES_MENU_EVENT_ACTIVATE,
    RETROPICOES_MENU_EVENT_CLOSE,
    RETROPICOES_MENU_EVENT_CONFIRM,
    RETROPICOES_MENU_EVENT_ERROR
} retropicoes_menu_event_type_t;

typedef struct retropicoes_menu_list_state {
    size_t item_count;
    size_t selected;
    size_t first_visible;
    size_t page_rows;
} retropicoes_menu_list_state_t;

typedef struct retropicoes_menu_event {
    retropicoes_menu_event_type_t type;
    size_t selected;
} retropicoes_menu_event_t;

void retropicoes_menu_list_init(retropicoes_menu_list_state_t *state,
                                size_t item_count,
                                size_t page_rows);
void retropicoes_menu_list_set_count(retropicoes_menu_list_state_t *state,
                                     size_t item_count);
size_t retropicoes_menu_list_visible_count(
    const retropicoes_menu_list_state_t *state);
retropicoes_menu_event_t
retropicoes_menu_list_update(retropicoes_menu_list_state_t *state,
                             retropicoes_ui_actions_t pressed_actions);

typedef struct retropicoes_system_menu_state {
    retropicoes_menu_list_state_t list;
    retropicoes_menu_item_id_t items[RETROPICOES_MENU_MAX_SYSTEM_ITEMS];
    size_t item_count;
    retropicoes_menu_item_id_t confirmation_item;
    int confirmation_active;
    int open;
} retropicoes_system_menu_state_t;

typedef struct retropicoes_system_menu_event {
    retropicoes_menu_event_type_t type;
    retropicoes_menu_item_id_t item;
} retropicoes_system_menu_event_t;

int retropicoes_system_menu_open(retropicoes_system_menu_state_t *state,
                                 const retropicoes_menu_ops_t *ops,
                                 size_t page_rows);
retropicoes_system_menu_event_t
retropicoes_system_menu_update(retropicoes_system_menu_state_t *state,
                               retropicoes_ui_actions_t pressed_actions);
int retropicoes_menu_item_needs_confirmation(
    retropicoes_menu_item_id_t item);

#ifdef __cplusplus
}
#endif

#endif

