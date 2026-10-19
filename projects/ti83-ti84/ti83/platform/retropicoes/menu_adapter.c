/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "menu_adapter.h"

#include <string.h>

#include "compat.h"

static uint32_t compat_bit(compat_key_t key)
{
    return 1u << (unsigned)key;
}

retropicoes_ui_actions_t
retropicoes_ti83_menu_actions(
    uint32_t pressed_compat_keys,
    retropicoes_controller_profile_t profile)
{
    retropicoes_ui_actions_t actions = 0u;
    if ((pressed_compat_keys & compat_bit(COMPAT_KEY_UP)) != 0u)
        actions |= RETROPICOES_UI_UP;
    if ((pressed_compat_keys & compat_bit(COMPAT_KEY_DOWN)) != 0u)
        actions |= RETROPICOES_UI_DOWN;
    if ((pressed_compat_keys & compat_bit(COMPAT_KEY_LEFT)) != 0u)
        actions |= RETROPICOES_UI_LEFT;
    if ((pressed_compat_keys & compat_bit(COMPAT_KEY_RIGHT)) != 0u)
        actions |= RETROPICOES_UI_RIGHT;
    actions |= retropicoes_menu_face_actions_from_compat(
        profile,
        (pressed_compat_keys & compat_bit(COMPAT_KEY_ACTION)) != 0u,
        (pressed_compat_keys & compat_bit(COMPAT_KEY_JUMP)) != 0u);
    if ((pressed_compat_keys & compat_bit(COMPAT_KEY_BACK)) != 0u)
        actions |= RETROPICOES_UI_MENU;
    if ((pressed_compat_keys & compat_bit(COMPAT_KEY_START)) != 0u)
        actions |= RETROPICOES_UI_AUX;
    return actions;
}