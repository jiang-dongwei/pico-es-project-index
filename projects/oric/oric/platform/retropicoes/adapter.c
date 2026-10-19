/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adapter.h"
#include "compat.h"
void oric_options_default(oric_options_t *s) {
    *s=(oric_options_t){1,0,1,0,0,0};
}
int oric_options_valid(const oric_options_t *s) {
    return s && s->model<2 && s->qaop<2 && s->audio<2 &&
        s->scanlines<2 && s->fps<2 && s->key<40;
}
uint32_t oric_options_checksum(const oric_options_t *s) {
    const uint8_t *p=(const uint8_t *)s; uint32_t h=2166136261u;
    for(unsigned i=0;i<sizeof(*s);++i) h=(h^p[i])*16777619u;
    return h;
}
retropicoes_ui_actions_t oric_ui_actions(uint32_t m,
    retropicoes_controller_profile_t profile,int menu) {
    retropicoes_ui_actions_t a=0;
    if(m&(1u<<COMPAT_KEY_UP)) a|=RETROPICOES_UI_UP;
    if(m&(1u<<COMPAT_KEY_DOWN)) a|=RETROPICOES_UI_DOWN;
    if(m&(1u<<COMPAT_KEY_LEFT)) a|=RETROPICOES_UI_LEFT;
    if(m&(1u<<COMPAT_KEY_RIGHT)) a|=RETROPICOES_UI_RIGHT;
    a|=retropicoes_menu_face_actions_from_compat(profile,
        !!(m&(1u<<COMPAT_KEY_ACTION)),!!(m&(1u<<COMPAT_KEY_JUMP)));
    if(menu) a|=RETROPICOES_UI_MENU;
    return a;
}
