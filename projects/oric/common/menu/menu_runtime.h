#ifndef RETROPICOES_COMMON_MENU_RUNTIME_H
#define RETROPICOES_COMMON_MENU_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "menu_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_MENU_LIFECYCLE_VERSION 1u

typedef struct retropicoes_menu_lifecycle {
    uint32_t version;
    size_t struct_size;
    void *context;
    retropicoes_menu_callback_t enter_menu;
    retropicoes_menu_callback_t leave_menu;
} retropicoes_menu_lifecycle_t;

typedef struct retropicoes_menu_runtime_event {
    retropicoes_menu_event_type_t type;
    retropicoes_menu_item_id_t item;
    retropicoes_menu_result_t result;
} retropicoes_menu_runtime_event_t;

typedef struct retropicoes_menu_runtime {
    retropicoes_system_menu_state_t menu;
    retropicoes_menu_ops_t ops;
    retropicoes_menu_lifecycle_t lifecycle;
    retropicoes_menu_item_id_t last_item;
    retropicoes_menu_result_t last_result;
    int has_lifecycle;
    int open;
} retropicoes_menu_runtime_t;

int retropicoes_menu_lifecycle_valid(
    const retropicoes_menu_lifecycle_t *lifecycle);

retropicoes_menu_result_t
retropicoes_menu_runtime_open(retropicoes_menu_runtime_t *runtime,
                              const retropicoes_menu_ops_t *ops,
                              const retropicoes_menu_lifecycle_t *lifecycle,
                              size_t page_rows);

retropicoes_menu_runtime_event_t
retropicoes_menu_runtime_update(retropicoes_menu_runtime_t *runtime,
                                retropicoes_ui_actions_t pressed_actions);

int retropicoes_menu_runtime_is_open(
    const retropicoes_menu_runtime_t *runtime);

const char *retropicoes_menu_result_name(retropicoes_menu_result_t result);

#ifdef __cplusplus
}
#endif

#endif
