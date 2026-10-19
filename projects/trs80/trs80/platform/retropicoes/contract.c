/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "contract.h"

#include <stdio.h>
#include <string.h>

static int make_path(char *out, size_t capacity, const char *root,
                     const char *leaf)
{
    int length;
    if (out == NULL || capacity == 0u || root == NULL || leaf == NULL ||
        leaf[0] == '\0' || leaf[0] == '/' || leaf[0] == '\\')
        return 0;
    if (strchr(leaf, '/') != NULL || strchr(leaf, '\\') != NULL ||
        strchr(leaf, ':') != NULL || strcmp(leaf, ".") == 0 ||
        strcmp(leaf, "..") == 0) return 0;
    length = snprintf(out, capacity, "%s/%s", root, leaf);
    return length > 0 && (size_t)length < capacity;
}

retropicoes_ui_actions_t trs80_menu_actions(uint32_t keys)
{
    retropicoes_ui_actions_t actions = 0u;
    if ((keys & TRS80_KEY_UP) != 0u) actions |= RETROPICOES_UI_UP;
    if ((keys & TRS80_KEY_DOWN) != 0u) actions |= RETROPICOES_UI_DOWN;
    if ((keys & TRS80_KEY_LEFT) != 0u) actions |= RETROPICOES_UI_LEFT;
    if ((keys & TRS80_KEY_RIGHT) != 0u) actions |= RETROPICOES_UI_RIGHT;
    if ((keys & TRS80_KEY_CONFIRM) != 0u)
        actions |= RETROPICOES_UI_CONFIRM;
    if ((keys & TRS80_KEY_CANCEL) != 0u)
        actions |= RETROPICOES_UI_CANCEL;
    if ((keys & TRS80_KEY_MENU) != 0u) actions |= RETROPICOES_UI_MENU;
    if ((keys & TRS80_KEY_AUX) != 0u) actions |= RETROPICOES_UI_AUX;
    return actions;
}

static retropicoes_menu_result_t set_request(
    void *opaque, trs80_menu_request_t request)
{
    trs80_menu_context_t *context =
        (trs80_menu_context_t *)opaque;
    if (context == NULL) return RETROPICOES_MENU_RESULT_INVALID;
    context->request = request;
    return RETROPICOES_MENU_RESULT_OK;
}

static retropicoes_menu_result_t resume(void *context)
{
    return set_request(context, TRS80_MENU_REQUEST_RESUME);
}

static retropicoes_menu_result_t controls(void *context)
{
    return set_request(context, TRS80_MENU_REQUEST_CONTROLS);
}

static retropicoes_menu_result_t rom_list(void *context)
{
    return set_request(context, TRS80_MENU_REQUEST_ROM_LIST);
}

static retropicoes_menu_result_t return_loader(void *context)
{
    return set_request(context, TRS80_MENU_REQUEST_RETURN_LOADER);
}

void trs80_menu_build_ops(retropicoes_menu_ops_t *ops,
                                  trs80_menu_context_t *context)
{
    if (ops == NULL) return;
    memset(ops, 0, sizeof(*ops));
    ops->contract_version = RETROPICOES_MENU_CONTRACT_VERSION;
    ops->struct_size = sizeof(*ops);
    ops->capabilities = RETROPICOES_MENU_CAP_RESUME |
                        RETROPICOES_MENU_CAP_CONTROLS |
                        RETROPICOES_MENU_CAP_ROM_LIST |
                        RETROPICOES_MENU_CAP_RETURN_LOADER;
    ops->context = context;
    ops->resume = resume;
    ops->show_controls = controls;
    ops->open_rom_list = rom_list;
    ops->return_loader = return_loader;
}

int trs80_frontend_init(
    retropicoes_frontend_shell_t *shell,
    retropicoes_rom_browser_t *browser,
    const retropicoes_frontend_lifecycle_t *lifecycle,
    const retropicoes_rom_source_t *source)
{
    if (!retropicoes_frontend_lifecycle_valid(lifecycle) ||
        !retropicoes_rom_source_valid(source)) return 0;
    if (!retropicoes_frontend_shell_init(shell, lifecycle)) return 0;
    return retropicoes_rom_browser_open(browser, source,
        RETROPICOES_ROM_BROWSER_DEFAULT_PAGE_ROWS);
}

int trs80_make_rom_path(char *out, size_t capacity,
                                const char *leaf)
{
    return make_path(out, capacity, "/roms/TRS80", leaf);
}

int trs80_make_save_path(char *out, size_t capacity,
                                 const char *leaf)
{
    return make_path(out, capacity, "/SAVES/TRS80", leaf);
}
