#ifndef RETROPICOES_COMMON_FRONTEND_SHELL_H
#define RETROPICOES_COMMON_FRONTEND_SHELL_H

#include <stddef.h>
#include <stdint.h>

#include "menu_contract.h"
#include "rom_browser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETROPICOES_FRONTEND_CONTRACT_VERSION 2u
#define RETROPICOES_FRONTEND_LIFECYCLE_VERSION 1u

typedef enum retropicoes_frontend_screen {
    RETROPICOES_FRONTEND_ROM_BROWSER = 0,
    RETROPICOES_FRONTEND_RUNNING,
    RETROPICOES_FRONTEND_SETTINGS,
    RETROPICOES_FRONTEND_STOPPED
} retropicoes_frontend_screen_t;

typedef enum retropicoes_frontend_settings_action {
    RETROPICOES_FRONTEND_SETTINGS_CANCEL = 0,
    RETROPICOES_FRONTEND_SETTINGS_RESUME,
    RETROPICOES_FRONTEND_SETTINGS_RESET,
    RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER,
    RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER
} retropicoes_frontend_settings_action_t;

typedef retropicoes_menu_result_t (*retropicoes_frontend_start_fn)(
    void *context,
    size_t source_index,
    retropicoes_rom_item_kind_t item_kind);

typedef struct retropicoes_frontend_lifecycle {
    uint32_t version;
    size_t struct_size;
    void *context;
    retropicoes_frontend_start_fn start;
    retropicoes_menu_callback_t pause;
    retropicoes_menu_callback_t resume;
    retropicoes_menu_callback_t reset;
    retropicoes_menu_callback_t stop;
    retropicoes_menu_callback_t return_loader;
} retropicoes_frontend_lifecycle_t;

typedef struct retropicoes_frontend_shell {
    retropicoes_frontend_lifecycle_t lifecycle;
    retropicoes_frontend_screen_t screen;
    retropicoes_frontend_screen_t settings_origin;
    retropicoes_menu_result_t last_result;
    int game_active;
} retropicoes_frontend_shell_t;

int retropicoes_frontend_lifecycle_valid(
    const retropicoes_frontend_lifecycle_t *lifecycle);
int retropicoes_frontend_shell_init(
    retropicoes_frontend_shell_t *shell,
    const retropicoes_frontend_lifecycle_t *lifecycle);
retropicoes_menu_result_t retropicoes_frontend_shell_launch(
    retropicoes_frontend_shell_t *shell,
    size_t source_index,
    retropicoes_rom_item_kind_t item_kind);
retropicoes_menu_result_t retropicoes_frontend_shell_open_settings(
    retropicoes_frontend_shell_t *shell);
retropicoes_menu_result_t retropicoes_frontend_shell_settings_action(
    retropicoes_frontend_shell_t *shell,
    retropicoes_frontend_settings_action_t action);
retropicoes_menu_result_t retropicoes_frontend_shell_game_finished(
    retropicoes_frontend_shell_t *shell);

#ifdef __cplusplus
}
#endif

#endif
