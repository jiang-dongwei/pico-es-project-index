/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_STUDIO2_MENU_ADAPTER_H
#define RETROPICOES_STUDIO2_MENU_ADAPTER_H

#include <stdint.h>

#include "menu_contract.h"
#include "menu_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum retropicoes_studio2_menu_request {
    RETROPICOES_STUDIO2_MENU_REQUEST_NONE = 0,
    RETROPICOES_STUDIO2_MENU_REQUEST_RESUME,
    RETROPICOES_STUDIO2_MENU_REQUEST_CONTROLS,
    RETROPICOES_STUDIO2_MENU_REQUEST_RESET,
    RETROPICOES_STUDIO2_MENU_REQUEST_ROM_LIST,
    RETROPICOES_STUDIO2_MENU_REQUEST_RETURN_LOADER
} retropicoes_studio2_menu_request_t;

retropicoes_ui_actions_t retropicoes_studio2_menu_actions(uint32_t pressed_compat_keys,
                                                          retropicoes_controller_profile_t profile);

#ifdef __cplusplus
}
#endif

#endif
