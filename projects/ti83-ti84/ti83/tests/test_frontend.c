/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "frontend.h"
#include "compat.h"
#include "input_backend.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct compat_file { size_t size, offset; int truncated; };
static struct compat_file file;
static unsigned opens, closes, presents, menu;
static uint64_t ticks;
static uint16_t pixels[320*240];
static compat_framebuffer_t fb={pixels,320,320,240};
compat_file_t *compat_file_open(const char *path,const char *mode)
{
    assert(!strcmp(mode,"rb"));
    memset(&file,0,sizeof(file));
    if(!strcmp(path,"missing.rom")) return NULL;
    file.size=strstr(path,".83prom")?0x80000:strstr(path,".84prom")?0x100000:0x40000;
    if(strstr(path,"short")) file.size=12;
    if(strstr(path,"truncated")) file.truncated=1;
    ++opens;return &file;
}
long compat_file_size(compat_file_t *f) {return (long)f->size;}
size_t compat_file_read(void *p,size_t size,size_t count,compat_file_t *f)
{
    assert(size==1 && count==f->size);
    memset(p,0,count);((uint8_t*)p)[0]=0x76;
    f->offset=count;return f->truncated?count-1:count;
}
int compat_file_close(compat_file_t *f) {assert(f==&file);++closes;return 0;}
void compat_poll_input(void) {ticks+=100;}
int compat_key_pressed(compat_key_t key) {(void)key;return 0;}
int retropicoes_input_menu_pressed(void) {return (int)menu;}
void retropicoes_input_get_stats(retropicoes_input_stats_t *s)
{
    memset(s,0,sizeof(*s));s->usb_name[0]="Xbox 360 Wired";
}
compat_framebuffer_t *compat_framebuffer(void) {return &fb;}
void compat_present(void) {++presents;ticks+=50;}
uint64_t compat_ticks_us(void) {return ++ticks;}
uint32_t compat_ticks_ms(void) {return (uint32_t)(ticks/1000);}
int main(void)
{
    char error[96];
    assert(retropicoes_ti83_frontend_start("missing.rom",error,sizeof(error))<0);
    assert(retropicoes_ti83_frontend_start("short.rom",error,sizeof(error))<0);
    assert(retropicoes_ti83_frontend_start("truncated.rom",error,sizeof(error))<0);
    assert(retropicoes_ti83_frontend_start("bad.8xp",error,sizeof(error))<0);
    assert(opens==closes);
    const char *models[]={"test.83rom","test.83prom","test.84prom",NULL};
    for(unsigned i=0;i<4;++i) {
        assert(!retropicoes_ti83_frontend_start(models[i],error,sizeof(error)));
        assert(retropicoes_ti83_frontend_start(models[i],error,sizeof(error))<0);
        assert(!retropicoes_ti83_frontend_run_frame());
        assert(!retropicoes_ti83_frontend_pause());
        assert(retropicoes_ti83_frontend_run_frame()<0);
        assert(!retropicoes_ti83_frontend_resume());
        menu=1;unsigned old=presents;
        assert(retropicoes_ti83_frontend_run_frame()==2 && presents==old);
        menu=0;assert(retropicoes_ti83_frontend_start(models[i],error,sizeof(error))<0);
        assert(!retropicoes_ti83_frontend_run_frame());
        retropicoes_ti83_frontend_stats_t stats;retropicoes_ti83_frontend_get_stats(&stats);
        assert(stats.frames==2 && stats.audio_samples_dropped==0);
        retropicoes_ti83_frontend_stop();retropicoes_ti83_frontend_stop();
        assert(retropicoes_ti83_frontend_run_frame()<0);
    }
    assert(opens==closes);
    puts("TI83 frontend: exact production load failures, all models, demo, pause/resume, Menu, stop PASS");
    return 0;
}
