/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ORIC_ADAPTER_H
#define ORIC_ADAPTER_H
#include "menu_input.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint8_t model, qaop, audio, scanlines, fps, key; } oric_options_t;
void oric_options_default(oric_options_t *);
int oric_options_valid(const oric_options_t *);
uint32_t oric_options_checksum(const oric_options_t *);
retropicoes_ui_actions_t oric_ui_actions(uint32_t compat_mask,
    retropicoes_controller_profile_t profile, int physical_menu);
#ifdef __cplusplus
}
#endif
#endif
