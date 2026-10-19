/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TRS80_VIRTUAL_KEYBOARD_H
#define TRS80_VIRTUAL_KEYBOARD_H
#include "trs80.h"
#include "menu_input.h"
#include "menu_render.h"
typedef struct {
    trs80_model_t model;
    unsigned selected;
    int open, release_gate, shift;
} trs80_virtual_keyboard_t;
typedef enum { TRS80_VK_NONE, TRS80_VK_SEND, TRS80_VK_CLOSE } trs80_vk_event_t;
typedef struct { trs80_vk_event_t type; unsigned key; int shift; } trs80_vk_result_t;
void trs80_vk_open(trs80_virtual_keyboard_t *keyboard, trs80_model_t model);
void trs80_vk_close(trs80_virtual_keyboard_t *keyboard);
trs80_vk_result_t trs80_vk_update(trs80_virtual_keyboard_t *keyboard,
                                 retropicoes_ui_actions_t actions);
const char *trs80_vk_label(trs80_model_t model, unsigned key);
void trs80_vk_render(const trs80_virtual_keyboard_t *keyboard,
                     retropicoes_menu_surface_t *surface,
                     const retropicoes_controller_labels_t *labels);
#endif
