/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "virtual_keyboard.h"
#include <stdio.h>
#include <string.h>
static const char *const labels[64] = {
    "@","A","B","C","D","E","F","G", "H","I","J","K","L","M","N","O",
    "P","Q","R","S","T","U","V","W", "X","Y","Z",NULL,NULL,NULL,NULL,NULL,
    "0","1","2","3","4","5","6","7", "8","9",":",";",",","-",".","/",
    "ENTER","CLEAR","BREAK","UP","DOWN","LEFT","RIGHT","SPACE",
    "SHIFT","RSHFT","CTRL","CAPS","F1","F2","F3",NULL
};
const char *trs80_vk_label(trs80_model_t model, unsigned key)
{
    if (key >= 64u || (model < TRS80_MODEL_4 && key >= 58u) ||
        (model == TRS80_MODEL_I && key == 57u)) return NULL;
    return labels[key];
}
void trs80_vk_open(trs80_virtual_keyboard_t *keyboard, trs80_model_t model)
{
    if (keyboard == NULL) return;
    memset(keyboard, 0, sizeof(*keyboard));
    keyboard->model = model;
    keyboard->selected = TRS80_MATRIX_ENTER;
    keyboard->open = keyboard->release_gate = 1;
}
void trs80_vk_close(trs80_virtual_keyboard_t *keyboard)
{
    if (keyboard == NULL) return;
    keyboard->open = keyboard->shift = 0;
    keyboard->release_gate = 1;
}
trs80_vk_result_t trs80_vk_update(trs80_virtual_keyboard_t *k,
                                 retropicoes_ui_actions_t actions)
{
    trs80_vk_result_t event = {TRS80_VK_NONE, 0u, 0};
    if (k == NULL || !k->open) return event;
    if (k->release_gate) {
        if (actions == 0u) k->release_gate = 0;
        return event;
    }
    if (actions & (RETROPICOES_UI_CANCEL | RETROPICOES_UI_MENU)) {
        trs80_vk_close(k); event.type = TRS80_VK_CLOSE; return event;
    }
    int step = 0;
    if (actions & RETROPICOES_UI_UP) step = -8;
    else if (actions & RETROPICOES_UI_DOWN) step = 8;
    else if (actions & RETROPICOES_UI_LEFT) step = -1;
    else if (actions & RETROPICOES_UI_RIGHT) step = 1;
    if (step) {
        unsigned next = k->selected;
        for (unsigned attempt = 0u; attempt < 64u; ++attempt) {
            next = (unsigned)((int)next + step + 64) % 64u;
            if (trs80_vk_label(k->model, next) != NULL) { k->selected = next; break; }
        }
        k->release_gate = 1;
    } else if (actions & RETROPICOES_UI_CONFIRM) {
        if (k->selected == TRS80_MATRIX_SHIFT || k->selected == 57u) k->shift = !k->shift;
        else {
            event.type = TRS80_VK_SEND; event.key = k->selected; event.shift = k->shift;
        }
        k->release_gate = 1;
    }
    return event;
}
void trs80_vk_render(const trs80_virtual_keyboard_t *k,
                     retropicoes_menu_surface_t *surface,
                     const retropicoes_controller_labels_t *controller)
{
    if (k == NULL || !k->open || surface == NULL || surface->pixels == NULL ||
        surface->width < 320u || surface->height < 240u || surface->stride < surface->width)
        return;
    for (size_t y = 0u; y < surface->height; ++y)
        for (size_t x = 0u; x < surface->width; ++x)
            surface->pixels[y * surface->stride + x] = 0u;
    retropicoes_menu_nintendo_draw_text(surface, 8, 8, "TRS-80 KEYBOARD", 0xffffu, 1u, 38u, NULL);
    for (unsigned key = 0u; key < 64u; ++key) {
        const char *label = trs80_vk_label(k->model, key);
        if (label == NULL) continue;
        int x = (int)(key % 8u) * 40, y = 32 + (int)(key / 8u) * 20;
        uint16_t fg = key == k->selected ? 0xffe0u : 0xffffu;
        retropicoes_menu_nintendo_draw_text(surface, x, y, label, fg, 1u, 5u, NULL);
    }
    char hint[64];
    snprintf(hint, sizeof(hint), "%s: SEND  %s: BACK  SHIFT:%s",
             controller != NULL ? controller->confirm : "A",
             controller != NULL ? controller->cancel : "B", k->shift ? "ON" : "OFF");
    retropicoes_menu_nintendo_draw_text(surface, 0, 204, "D-PAD: SELECT KEY", 0xffffu, 1u, 40u, NULL);
    retropicoes_menu_nintendo_draw_text(surface, 0, 220, hint, 0xffffu, 1u, 40u, NULL);
}
