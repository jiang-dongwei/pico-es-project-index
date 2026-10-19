/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_TI83_MENU_ADAPTER_H
#define RETROPICOES_TI83_MENU_ADAPTER_H

#include <stdint.h>

#include "menu_contract.h"
#include "menu_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum retropicoes_ti83_menu_request {
    RETROPICOES_TI83_MENU_REQUEST_NONE = 0,
    RETROPICOES_TI83_MENU_REQUEST_RESUME,
    RETROPICOES_TI83_MENU_REQUEST_RESET,
    RETROPICOES_TI83_MENU_REQUEST_ROM_LIST,
    RETROPICOES_TI83_MENU_REQUEST_RETURN_LOADER
} retropicoes_ti83_menu_request_t;

retropicoes_ui_actions_t
retropicoes_ti83_menu_actions(
    uint32_t pressed_compat_keys,
    retropicoes_controller_profile_t profile);

#ifdef __cplusplus
}
#endif

#endif