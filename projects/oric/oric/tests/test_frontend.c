/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adapter.h"
#include "compat.h"
#include "frontend_shell.h"
#include "settings_host.h"
#include <assert.h>
#include <string.h>
static unsigned starts,pauses,resumes,stops,resets;
static retropicoes_menu_result_t start(void *p,size_t i,retropicoes_rom_item_kind_t k){(void)p;(void)i;assert(k==RETROPICOES_ROM_ITEM_DEMO);++starts;return RETROPICOES_MENU_RESULT_OK;}
static retropicoes_menu_result_t pause_core(void*p){(void)p;++pauses;return RETROPICOES_MENU_RESULT_OK;}
static retropicoes_menu_result_t resume_core(void*p){(void)p;++resumes;return RETROPICOES_MENU_RESULT_OK;}
static retropicoes_menu_result_t stop_core(void*p){(void)p;++stops;return RETROPICOES_MENU_RESULT_OK;}
static retropicoes_menu_result_t reset_core(void*p){(void)p;++resets;return RETROPICOES_MENU_RESULT_OK;}
static size_t count(void*p){(void)p;return 0;}
static int item(void*p,size_t i,retropicoes_rom_item_view_t*v){(void)p;(void)i;(void)v;return 0;}
int main(void){
    oric_options_t o,c;oric_options_default(&o);assert(oric_options_valid(&o));c=o;c.audio^=1;assert(oric_options_checksum(&o)!=oric_options_checksum(&c));c.key=40;assert(!oric_options_valid(&c));
    for(unsigned p=0;p<RETROPICOES_CONTROLLER_PROFILE_COUNT;++p){
        assert(!(oric_ui_actions((1u<<COMPAT_KEY_START)|(1u<<COMPAT_KEY_BACK),(retropicoes_controller_profile_t)p,0)&RETROPICOES_UI_MENU));
        assert(oric_ui_actions(0,(retropicoes_controller_profile_t)p,1)==RETROPICOES_UI_MENU);
    }
    assert(oric_ui_actions(1u<<COMPAT_KEY_ACTION,RETROPICOES_CONTROLLER_NES,0)==RETROPICOES_UI_CONFIRM);
    assert(oric_ui_actions(1u<<COMPAT_KEY_JUMP,RETROPICOES_CONTROLLER_NES,0)==RETROPICOES_UI_CANCEL);
    retropicoes_rom_source_t source={1,sizeof(source),0,"ORIC","NO TAP","SIMULATED TEST",count,item,0};
    retropicoes_rom_browser_t b;assert(retropicoes_rom_browser_open(&b,&source,18));assert(b.showing_demo);
    retropicoes_rom_browser_event_t e=retropicoes_rom_browser_update(&b,RETROPICOES_UI_CONFIRM);assert(e.type==RETROPICOES_ROM_BROWSER_EVENT_OPEN && e.item_kind==RETROPICOES_ROM_ITEM_DEMO);
    retropicoes_frontend_lifecycle_t life={1,sizeof(life),0,start,pause_core,resume_core,reset_core,stop_core,stop_core};
    retropicoes_frontend_shell_t s;assert(retropicoes_frontend_shell_init(&s,&life));
    assert(retropicoes_frontend_shell_launch(&s,e.source_index,e.item_kind)==0);
    assert(retropicoes_frontend_shell_open_settings(&s)==0 && pauses==1);
    assert(retropicoes_frontend_shell_settings_action(&s,RETROPICOES_FRONTEND_SETTINGS_CANCEL)==0 && resumes==1);
    assert(retropicoes_frontend_shell_open_settings(&s)==0);
    assert(retropicoes_frontend_shell_settings_action(&s,RETROPICOES_FRONTEND_SETTINGS_RESET)==0 && resets==1 && resumes==2);
    assert(retropicoes_frontend_shell_open_settings(&s)==0);
    assert(retropicoes_frontend_shell_settings_action(&s,RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER)==0 && stops==1 && s.screen==RETROPICOES_FRONTEND_ROM_BROWSER);
    retropicoes_settings_home_gate_t g;retropicoes_settings_home_gate_init(&g,0);
    assert(!retropicoes_settings_home_gate_filter(&g,RETROPICOES_UI_MENU,1,0,150));
    assert(!retropicoes_settings_home_gate_filter(&g,0,0,10,150));
    assert(!retropicoes_settings_home_gate_filter(&g,0,0,160,150));
    assert(retropicoes_settings_home_gate_filter(&g,RETROPICOES_UI_MENU,1,170,150)==RETROPICOES_UI_MENU);
    return 0;
}
