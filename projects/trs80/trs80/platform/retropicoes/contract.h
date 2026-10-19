/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TRS80_RETROPICOES_CONTRACT_H
#define TRS80_RETROPICOES_CONTRACT_H

#include <stddef.h>
#include <stdint.h>

#include "menu_contract.h"
#include "frontend_shell.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TRS80_KEY_UP = 1u << 0,
    TRS80_KEY_DOWN = 1u << 1,
    TRS80_KEY_LEFT = 1u << 2,
    TRS80_KEY_RIGHT = 1u << 3,
    TRS80_KEY_CANCEL = 1u << 4,
    TRS80_KEY_CONFIRM = 1u << 5,
    TRS80_KEY_AUX = 1u << 6,
    TRS80_KEY_MENU = 1u << 7
};

typedef enum trs80_menu_request {
    TRS80_MENU_REQUEST_NONE = 0,
    TRS80_MENU_REQUEST_RESUME,
    TRS80_MENU_REQUEST_CONTROLS,
    TRS80_MENU_REQUEST_ROM_LIST,
    TRS80_MENU_REQUEST_RETURN_LOADER
} trs80_menu_request_t;

typedef struct trs80_menu_context {
    trs80_menu_request_t request;
} trs80_menu_context_t;

retropicoes_ui_actions_t trs80_menu_actions(uint32_t keys);
void trs80_menu_build_ops(retropicoes_menu_ops_t *ops,
                                  trs80_menu_context_t *context);
int trs80_frontend_init(
    retropicoes_frontend_shell_t *shell,
    retropicoes_rom_browser_t *browser,
    const retropicoes_frontend_lifecycle_t *lifecycle,
    const retropicoes_rom_source_t *source);
int trs80_make_rom_path(char *out, size_t capacity,
                                const char *leaf);
int trs80_make_save_path(char *out, size_t capacity,
                                 const char *leaf);

#ifdef __cplusplus
}
#endif
#endif
