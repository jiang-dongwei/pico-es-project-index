#ifndef RETROPICOES_TI83_SETTINGS_UI_H
#define RETROPICOES_TI83_SETTINGS_UI_H

#include "menu_adapter.h"
#include "settings_adapter.h"
#include "settings_menu.h"

#ifdef __cplusplus
extern "C" {
#endif
void retropicoes_ti83_settings_ui_init(void);
const retropicoes_ti83_settings_t *retropicoes_ti83_settings_ui_current(void);
retropicoes_ti83_menu_request_t retropicoes_ti83_settings_ui_run(
    int in_game, const retropicoes_settings_platform_t *platform);
#ifdef __cplusplus
}
#endif
#endif