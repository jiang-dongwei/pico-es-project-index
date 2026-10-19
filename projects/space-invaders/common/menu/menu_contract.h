#ifndef RETROPICOES_COMMON_MENU_CONTRACT_H
#define RETROPICOES_COMMON_MENU_CONTRACT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_MENU_CONTRACT_VERSION 1u

/* Logical UI actions. Platform code maps physical controls to these bits. */
typedef uint32_t retropicoes_ui_actions_t;

enum retropicoes_ui_action {
    RETROPICOES_UI_UP      = 1u << 0,
    RETROPICOES_UI_DOWN    = 1u << 1,
    RETROPICOES_UI_LEFT    = 1u << 2,
    RETROPICOES_UI_RIGHT   = 1u << 3,
    RETROPICOES_UI_CONFIRM = 1u << 4,
    RETROPICOES_UI_CANCEL  = 1u << 5,
    RETROPICOES_UI_MENU    = 1u << 6,
    RETROPICOES_UI_AUX     = 1u << 7
};

typedef enum retropicoes_menu_item_id {
    RETROPICOES_MENU_ITEM_RESUME = 0,
    RETROPICOES_MENU_ITEM_CONTROLS,
    RETROPICOES_MENU_ITEM_STATE,
    RETROPICOES_MENU_ITEM_SETTINGS,
    RETROPICOES_MENU_ITEM_RESET,
    RETROPICOES_MENU_ITEM_ROM_LIST,
    RETROPICOES_MENU_ITEM_RETURN_LOADER,
    RETROPICOES_MENU_ITEM_COUNT
} retropicoes_menu_item_id_t;

enum retropicoes_menu_capability {
    RETROPICOES_MENU_CAP_RESUME        = 1u << 0,
    RETROPICOES_MENU_CAP_CONTROLS      = 1u << 1,
    RETROPICOES_MENU_CAP_STATE         = 1u << 2,
    RETROPICOES_MENU_CAP_SETTINGS      = 1u << 3,
    RETROPICOES_MENU_CAP_RESET         = 1u << 4,
    RETROPICOES_MENU_CAP_ROM_LIST      = 1u << 5,
    RETROPICOES_MENU_CAP_RETURN_LOADER = 1u << 6
};

typedef enum retropicoes_menu_result {
    RETROPICOES_MENU_RESULT_OK = 0,
    RETROPICOES_MENU_RESULT_CANCELLED,
    RETROPICOES_MENU_RESULT_UNAVAILABLE,
    RETROPICOES_MENU_RESULT_FAILED,
    RETROPICOES_MENU_RESULT_INVALID
} retropicoes_menu_result_t;

typedef retropicoes_menu_result_t (*retropicoes_menu_callback_t)(void *context);

/*
 * A core explicitly advertises a capability and supplies its callback.
 * The common menu exposes an item only when both are present.
 */
typedef struct retropicoes_menu_ops {
    uint32_t contract_version;
    size_t struct_size;
    uint32_t capabilities;
    void *context;
    retropicoes_menu_callback_t resume;
    retropicoes_menu_callback_t show_controls;
    retropicoes_menu_callback_t open_state_menu;
    retropicoes_menu_callback_t open_settings;
    retropicoes_menu_callback_t reset;
    retropicoes_menu_callback_t open_rom_list;
    retropicoes_menu_callback_t return_loader;
} retropicoes_menu_ops_t;

int retropicoes_menu_contract_valid(const retropicoes_menu_ops_t *ops);

size_t retropicoes_menu_build_items(const retropicoes_menu_ops_t *ops,
                                    retropicoes_menu_item_id_t *items,
                                    size_t item_capacity);

retropicoes_menu_result_t
retropicoes_menu_invoke(const retropicoes_menu_ops_t *ops,
                        retropicoes_menu_item_id_t item);

const char *retropicoes_menu_item_name(retropicoes_menu_item_id_t item);

#ifdef __cplusplus
}
#endif

#endif

