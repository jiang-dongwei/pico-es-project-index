#include "frontend_shell.h"

#include <string.h>

int retropicoes_frontend_lifecycle_valid(
    const retropicoes_frontend_lifecycle_t *lifecycle)
{
    return lifecycle != NULL &&
           lifecycle->version == RETROPICOES_FRONTEND_LIFECYCLE_VERSION &&
           lifecycle->struct_size >= sizeof(*lifecycle) &&
           lifecycle->start != NULL && lifecycle->pause != NULL &&
           lifecycle->resume != NULL && lifecycle->reset != NULL &&
           lifecycle->stop != NULL && lifecycle->return_loader != NULL;
}

int retropicoes_frontend_shell_init(
    retropicoes_frontend_shell_t *shell,
    const retropicoes_frontend_lifecycle_t *lifecycle)
{
    if (shell == NULL || !retropicoes_frontend_lifecycle_valid(lifecycle)) {
        return 0;
    }
    memset(shell, 0, sizeof(*shell));
    shell->lifecycle = *lifecycle;
    shell->screen = RETROPICOES_FRONTEND_ROM_BROWSER;
    shell->settings_origin = RETROPICOES_FRONTEND_ROM_BROWSER;
    shell->last_result = RETROPICOES_MENU_RESULT_OK;
    return 1;
}

static retropicoes_menu_result_t remember(
    retropicoes_frontend_shell_t *shell,
    retropicoes_menu_result_t result)
{
    shell->last_result = result;
    return result;
}

retropicoes_menu_result_t retropicoes_frontend_shell_launch(
    retropicoes_frontend_shell_t *shell,
    size_t source_index,
    retropicoes_rom_item_kind_t item_kind)
{
    retropicoes_menu_result_t result;

    if (shell == NULL || shell->screen != RETROPICOES_FRONTEND_ROM_BROWSER) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }
    result = shell->lifecycle.start(shell->lifecycle.context, source_index,
                                    item_kind);
    if (result == RETROPICOES_MENU_RESULT_OK) {
        shell->game_active = 1;
        shell->screen = RETROPICOES_FRONTEND_RUNNING;
    }
    return remember(shell, result);
}

retropicoes_menu_result_t retropicoes_frontend_shell_open_settings(
    retropicoes_frontend_shell_t *shell)
{
    retropicoes_menu_result_t result = RETROPICOES_MENU_RESULT_OK;

    if (shell == NULL ||
        (shell->screen != RETROPICOES_FRONTEND_ROM_BROWSER &&
         shell->screen != RETROPICOES_FRONTEND_RUNNING)) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }
    shell->settings_origin = shell->screen;
    if (shell->screen == RETROPICOES_FRONTEND_RUNNING) {
        result = shell->lifecycle.pause(shell->lifecycle.context);
        if (result != RETROPICOES_MENU_RESULT_OK) {
            return remember(shell, result);
        }
    }
    shell->screen = RETROPICOES_FRONTEND_SETTINGS;
    return remember(shell, RETROPICOES_MENU_RESULT_OK);
}

static retropicoes_menu_result_t resume_game(
    retropicoes_frontend_shell_t *shell)
{
    retropicoes_menu_result_t result =
        shell->lifecycle.resume(shell->lifecycle.context);
    if (result == RETROPICOES_MENU_RESULT_OK) {
        shell->screen = RETROPICOES_FRONTEND_RUNNING;
    }
    return remember(shell, result);
}

static retropicoes_menu_result_t return_to_browser(
    retropicoes_frontend_shell_t *shell)
{
    retropicoes_menu_result_t result = RETROPICOES_MENU_RESULT_OK;

    if (shell->game_active) {
        result = shell->lifecycle.stop(shell->lifecycle.context);
        if (result != RETROPICOES_MENU_RESULT_OK) {
            return remember(shell, result);
        }
        shell->game_active = 0;
    }
    shell->screen = RETROPICOES_FRONTEND_ROM_BROWSER;
    return remember(shell, RETROPICOES_MENU_RESULT_OK);
}

retropicoes_menu_result_t retropicoes_frontend_shell_settings_action(
    retropicoes_frontend_shell_t *shell,
    retropicoes_frontend_settings_action_t action)
{
    retropicoes_menu_result_t result;

    if (shell == NULL || shell->screen != RETROPICOES_FRONTEND_SETTINGS) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }
    if (action == RETROPICOES_FRONTEND_SETTINGS_CANCEL ||
        action == RETROPICOES_FRONTEND_SETTINGS_RESUME) {
        if (shell->settings_origin == RETROPICOES_FRONTEND_RUNNING) {
            return resume_game(shell);
        }
        shell->screen = RETROPICOES_FRONTEND_ROM_BROWSER;
        return remember(shell, RETROPICOES_MENU_RESULT_OK);
    }
    if (action == RETROPICOES_FRONTEND_SETTINGS_RESET) {
        if (shell->settings_origin != RETROPICOES_FRONTEND_RUNNING ||
            !shell->game_active) {
            return remember(shell, RETROPICOES_MENU_RESULT_INVALID);
        }
        result = shell->lifecycle.reset(shell->lifecycle.context);
        if (result != RETROPICOES_MENU_RESULT_OK) {
            return remember(shell, result);
        }
        return resume_game(shell);
    }
    if (action == RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER) {
        return return_to_browser(shell);
    }
    if (action == RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER) {
        result = return_to_browser(shell);
        if (result != RETROPICOES_MENU_RESULT_OK) return result;
        result = shell->lifecycle.return_loader(shell->lifecycle.context);
        if (result == RETROPICOES_MENU_RESULT_OK) {
            shell->screen = RETROPICOES_FRONTEND_STOPPED;
        }
        return remember(shell, result);
    }
    return remember(shell, RETROPICOES_MENU_RESULT_INVALID);
}

retropicoes_menu_result_t retropicoes_frontend_shell_game_finished(
    retropicoes_frontend_shell_t *shell)
{
    if (shell == NULL || shell->screen != RETROPICOES_FRONTEND_RUNNING ||
        !shell->game_active) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }
    return return_to_browser(shell);
}
