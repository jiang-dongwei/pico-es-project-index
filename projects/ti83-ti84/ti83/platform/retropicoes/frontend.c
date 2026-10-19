/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "frontend.h"
#include "adapter.h"
#include "program.h"
#include "fast_memory.h"
#include "cpu_profile.h"
#include "compat.h"
#include "input_backend.h"
#include "menu_render.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
static TilemCalc *calc;
static TilemCalc *boot_cache;
static int paused, demo, game_session;
static unsigned time_fraction;
static uint32_t fps_start, fps_frames, fps10;
static ti83_input_t input;
static retropicoes_ti83_frontend_stats_t stats;
static retropicoes_ti83_frontend_config_t config;
static const char *load_stage;
static uint32_t load_draw;
static uint32_t load_begin;
static char load_name[40];
static void text(compat_framebuffer_t *fb,int x,int y,const char *message,uint16_t color)
{
    retropicoes_menu_surface_t surface={fb->pixels,fb->stride,fb->width,fb->height};
    retropicoes_menu_renderer_t renderer=retropicoes_menu_nintendo_renderer();
    renderer.draw_text(&surface,x,y,message,color,1,40,renderer.text_context);
}
static int fail(char *error,unsigned cap,const char *message)
{
    if(error && cap) snprintf(error,cap,"%s",message);
    retropicoes_ti83_frontend_stop(); return -1;
}
static void draw_loading(void)
{
    compat_framebuffer_t *fb=compat_framebuffer();
    if(!fb)return;
    for(unsigned y=0;y<fb->height;++y)
        for(unsigned x=0;x<fb->width;++x)fb->pixels[y*fb->stride+x]=0x2104;
    text(fb,24,88,game_session?"STARTING GAME...":"STARTING SYSTEM...",0xffff);
    ti83_load_progress_t p=ti83_program_progress();
    unsigned percent=p.total?100u*p.completed/p.total:0;
    if(percent>100)percent=100;
    char phase[40];
    if(game_session && p.stage)
        snprintf(phase,sizeof(phase),"%u/3 %s",p.stage+1,p.stage==1?"LOADING GAME":"STARTING");
    else snprintf(phase,sizeof(phase),"%s",game_session?"1/3 PREPARING":load_stage);
    text(fb,24,112,phase,0xffff);
    text(fb,24,144,load_name,0xffff);
    unsigned filled=p.stage?p.stage*90u+percent*90u/100u:0;
    unsigned travel=(compat_ticks_ms()-load_begin)/60u%140u;
    unsigned position=travel<=70u?travel:140u-travel;
    for(unsigned y=0;y<12;++y)for(unsigned x=0;x<272;++x) {
        uint16_t color=(y==0 || y==11 || x==0 || x==271)?0xffff:0x39e7;
        if(y>1 && y<10 && x>0 && x<271 &&
           (p.stage?x<=filled:(x>position && x<=position+20u)))color=0x07e0;
        fb->pixels[(170u+y)*fb->stride+24u+x]=color;
    }
    compat_present();
}
static int loading(void *context)
{
    (void)context;
    compat_poll_input();
    uint32_t now=compat_ticks_ms();
    if(now-load_draw>=200) {
        draw_loading();
        load_draw=now;
    }
    return !retropicoes_input_menu_pressed() && !compat_key_pressed(COMPAT_KEY_BACK);
}
int retropicoes_ti83_frontend_start(const char *path,char *error,unsigned cap)
{
    if(calc) {if(error && cap)snprintf(error,cap,"SESSION ALREADY ACTIVE");return -1;}
    demo=path==NULL;
    int game=ti83_program_extension(path);
    game_session=game;
    const char *name=path?path:"DEMO";
    for(const char *at=name;*at;++at)if(*at=='/' || *at=='\\')name=at+1;
    snprintf(load_name,sizeof(load_name),"%s",name);
    char *extension=strrchr(load_name,'.');if(extension)*extension=0;
    for(char *at=load_name;*at;++at)*at=(char)toupper((unsigned char)*at);
    ti83_program_progress_reset();
    load_begin=compat_ticks_ms();
    uint64_t total_begin=compat_ticks_us();
    uint8_t *program_bytes=NULL;
    ti83_program_t program;
    if(game) {
        compat_file_t *f=compat_file_open(path,"rb");
        if(!f) return fail(error,cap,"CANNOT OPEN GAME");
        long length=compat_file_size(f);
        if(length>=74 && length<=24074) program_bytes=tilem_try_malloc((size_t)length);
        int ok=program_bytes && compat_file_read(program_bytes,1,(size_t)length,f)==(size_t)length;
        compat_file_close(f);
        if(!ok) {tilem_free(program_bytes);return fail(error,cap,"GAME READ FAILED / TOO LARGE");}
        if(!ti83_program_parse(program_bytes,(size_t)length,&program,error,cap)) {
            tilem_free(program_bytes);retropicoes_ti83_frontend_stop();return -1;
        }
    }
    compat_file_t *file=NULL;
    long size=0; char model='3';
    if(!demo) {
        const char *rom=game?"/roms/TI83/ti84plus.84prom":path;
        file=compat_file_open(rom,"rb");
        if(!file) {tilem_free(program_bytes);return fail(error,cap,"MISSING TI84PLUS.84PROM SYSTEM");}
        size=compat_file_size(file);
        model=size>0 ? ti83_model(rom,(size_t)size) : 0;
        if(!model) {tilem_free(program_bytes);compat_file_close(file); return fail(error,cap,"NEED 256K/512K/1M Z80 ROM DUMP");}
    }
    calc=ti83_create(model);
    if(!calc) {tilem_free(program_bytes);if(file)compat_file_close(file); return fail(error,cap,"CALCULATOR ALLOCATION FAILED");}
    if(file) {
        load_stage="READING SYSTEM";draw_loading();
        uint64_t read_begin=compat_ticks_us();
        int ok=compat_file_read(calc->mem,1,(size_t)size,file)==(size_t)size;
        compat_file_close(file);
        if(!ok) {tilem_free(program_bytes);return fail(error,cap,"ROM READ FAILED");}
        if(model=='p' || model=='4') {
            calc->hw.stateloaded(calc,0);
        }
        dword boot=calc->hw.mem_ltop(calc,calc->z80.r.pc.d);
        if(boot>=calc->hw.romsize || calc->mem[boot]==0xff) {
            tilem_free(program_bytes);return fail(error,cap,"BLANK BOOT PAGE; NEED FULL ROM DUMP");
        }
        int warm=0;
        if(game && boot_cache) {
            if(!memcmp(calc->mem,boot_cache->mem,calc->hw.romsize)) {
                TilemCalc *restored=tilem_calc_copy(boot_cache);
                if(restored) {tilem_calc_free(calc);calc=restored;warm=1;}
            }
            if(!warm) {tilem_calc_free(boot_cache);boot_cache=NULL;}
        }
        if(model=='4' && !ti83_fast_attach(calc)) {tilem_free(program_bytes);return fail(error,cap,"FAST RAM INITIALIZATION FAILED");}
        load_stage="BOOTING SYSTEM";load_draw=compat_ticks_ms();
        uint64_t boot_begin=compat_ticks_us();
        ti83_profile_start();
        if(!warm && !ti83_power_on_progress(calc,loading,NULL)) {tilem_free(program_bytes);return fail(error,cap,"GAME LOAD CANCELLED");}
        if(game && !warm) {
            ti83_fast_detach(calc);
            boot_cache=tilem_calc_copy(calc);
            if(!ti83_fast_attach(calc)) {tilem_free(program_bytes);return fail(error,cap,"FAST RAM INITIALIZATION FAILED");}
        }
        uint64_t boot_end=compat_ticks_us();
        ti83_profile_report(warm?"warm-restore":"boot");
        if(game) {
            load_stage="IMPORTING GAME";
            ok=ti83_program_launch(calc,&program,loading,NULL,error,cap);
            if(ok)draw_loading();
            tilem_free(program_bytes);
            if(!ok) {retropicoes_ti83_frontend_stop();return -1;}
        }
        printf("TI83 LOAD warm=%u system_ms=%lu boot_ms=%lu import_ms=%lu total_ms=%lu\n",warm,
            (unsigned long)((boot_begin-read_begin)/1000),(unsigned long)((boot_end-boot_begin)/1000),
            (unsigned long)((compat_ticks_us()-boot_end)/1000),(unsigned long)((compat_ticks_us()-total_begin)/1000));
    } else {
        memset(calc->mem,0,calc->hw.romsize);
        calc->mem[0]=0x76;
        calc->lcd.active=1;
        for(unsigned y=0;y<64;++y) for(unsigned b=0;b<12;++b)
            calc->lcdmem[y*(unsigned)calc->lcd.rowstride+b]=(y/8+b)%2 ? 0xaa : 0x55;
    }
    ti83_input_reset(&input,config.controller_mode);
    input.game=game!=0;
    paused=0;time_fraction=0;fps_start=compat_ticks_ms();fps_frames=fps10=0;
    memset(&stats,0,sizeof(stats));
    if(calc && !demo) {
#if TI83_TIMING_PROFILE
        printf("TI83 MEM calc=%p regs=%p timers=%p lcd=%p rom=%p cpu_khz=%d\n",
            (void*)calc,(void*)calc->hwregs,(void*)calc->z80.timers,(void*)calc->lcdmem,(void*)calc->mem,calc->z80.clockspeed);
        ti83_profile_start();
#endif
    }

    return 0;
}
int retropicoes_ti83_frontend_run_frame(void)
{
    if(!calc || paused) return -1;
    uint64_t begin=compat_ticks_us();
    compat_poll_input();
    if(retropicoes_input_menu_pressed() || compat_key_pressed(COMPAT_KEY_BACK)) return 2;
    retropicoes_input_stats_t device; retropicoes_input_get_stats(&device);
    uint32_t masks=device.source_masks[0]|device.source_masks[1]|device.source_masks[2]|device.source_masks[3];
    unsigned owner=device.source_masks[0] ? 0 : device.source_masks[1] ? 1 : 2;
    retropicoes_controller_profile_t profile=owner<2 ?
        retropicoes_menu_controller_profile_from_name(device.usb_name[owner]) : RETROPICOES_CONTROLLER_NES;
    retropicoes_ui_actions_t face=retropicoes_menu_face_actions_from_compat(profile,
        (masks&(1u<<COMPAT_KEY_ACTION))!=0,(masks&(1u<<COMPAT_KEY_JUMP))!=0);
    unsigned buttons=masks&15u;
    if(face&RETROPICOES_UI_CONFIRM) buttons|=16u;
    if(face&RETROPICOES_UI_CANCEL) buttons|=32u;
    if(masks&(1u<<COMPAT_KEY_START)) buttons|=64u;
    ti83_input_update(calc,&input,buttons);
    uint64_t input_end=compat_ticks_us();
    time_fraction+=40u;
    if(!demo) ti83_step(calc,16666u+time_fraction/60u);
    time_fraction%=60u;
    uint64_t step_end=compat_ticks_us();
    compat_framebuffer_t *fb=compat_framebuffer();
    if(!fb || !ti83_blit(calc,fb->pixels,fb->stride,fb->width,fb->height,input.keyboard,config.scanlines_enabled)) return -2;
    if(input.keyboard) {
        for(unsigned key=0;key<49;++key) {
            int x=7+(int)(key%7)*44,y=137+(int)(key/7)*12;
            if(key==input.cursor) for(int dy=0;dy<10;++dy) for(int dx=0;dx<43;++dx)
                fb->pixels[(y+dy)*fb->stride+x+dx]=0xffff;
            text(fb,x,y,ti83_keylabels[key],key==input.cursor?0:0xffff);
        }
        text(fb,7,223,input.second?"2ND LOCK":"B:2ND",0xffe0);
        if(input.alpha) text(fb,95,223,"ALPHA LOCK",0xffe0);
    } else text(fb,4,218,game_session?"A:2ND B:ALPHA START:KEYPAD":"A:ENTER B:CLEAR START:KEYPAD",0xffff);
    if(demo) text(fb,4,4,"DEMO / TEST GAME (SIMULATED)",0xffff);
    if(config.fps_overlay_enabled) {
        char line[24];snprintf(line,sizeof(line),"%u.%u FPS",(unsigned)fps10/10,(unsigned)fps10%10);
        text(fb,244,230,line,0xffff);
    }
    uint64_t draw_end=compat_ticks_us();compat_present();uint64_t end=compat_ticks_us();
    ++stats.frames;++fps_frames;uint32_t now=compat_ticks_ms();
    if(now-fps_start>=500) {fps10=fps_frames*10000/(now-fps_start);fps_start=now;fps_frames=0;}
    stats.last_input_us=(uint32_t)(input_end-begin);stats.last_step_us=(uint32_t)(step_end-input_end);
    stats.last_draw_us=(uint32_t)(draw_end-step_end);stats.last_present_us=(uint32_t)(end-draw_end);
    stats.last_frame_us=(uint32_t)(end-begin);
    if(stats.frames==120)ti83_profile_report("game-120-frames");
    return 0;
}
int retropicoes_ti83_frontend_pause(void)
{
    if(!calc) return -1;
    ti83_release_keys(calc);memset(input.down,0,sizeof(input.down));input.previous=0;
    input.second=input.alpha=0;paused=1;return 0;
}
int retropicoes_ti83_frontend_resume(void)
{
    if(!calc) return -1;
    paused=0;fps_start=compat_ticks_ms();fps_frames=0;return 0;
}
void retropicoes_ti83_frontend_stop(void)
{
    ti83_profile_stop();
    if(calc) {ti83_fast_detach(calc);tilem_calc_free(calc);}
    calc=NULL;paused=0;
}
void retropicoes_ti83_frontend_get_stats(retropicoes_ti83_frontend_stats_t *s) {if(s)*s=stats;}
void retropicoes_ti83_frontend_set_config(const retropicoes_ti83_frontend_config_t *c)
{
    if(!c) return;
    if(calc && config.controller_mode!=c->controller_mode) {
        ti83_release_keys(calc);ti83_input_reset(&input,c->controller_mode);
        input.game=game_session!=0;
    }
    config=*c;
}
