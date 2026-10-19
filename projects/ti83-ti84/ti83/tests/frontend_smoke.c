/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "frontend.h"
#include "compat.h"
#include "input_backend.h"
#include "program.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct compat_file { FILE *file; };
static const char *system_path;
static uint32_t buttons;
static unsigned menu;
static uint64_t ticks;
static uint16_t pixels[320*240];
static uint16_t other_pixels[320*240], loading_header[320*28];
static unsigned loading_test, buffer_index, loading_frames[2], have_header;
static ti83_load_progress_t previous_progress;
static unsigned seen_stages,have_loading_shot;
static char loading_path[512];
static void screenshot_from(const char *,const uint16_t *);
static compat_framebuffer_t fb={pixels,320,320,240};
compat_file_t *compat_file_open(const char *path,const char *mode)
{
    if(!strcmp(path,"/roms/TI83/ti84plus.84prom"))path=system_path;
    FILE *f=fopen(path,mode);if(!f)return NULL;
    compat_file_t *p=malloc(sizeof(*p));if(!p){fclose(f);return NULL;}p->file=f;return p;
}
long compat_file_size(compat_file_t *p)
{
    long at=ftell(p->file);fseek(p->file,0,SEEK_END);long n=ftell(p->file);fseek(p->file,at,SEEK_SET);return n;
}
size_t compat_file_read(void *b,size_t n,size_t count,compat_file_t *p){return fread(b,n,count,p->file);}
int compat_file_close(compat_file_t *p){int rc=fclose(p->file);free(p);return rc;}
void compat_poll_input(void){ticks+=loading_test?201000:100;}
int compat_key_pressed(compat_key_t k){(void)k;return 0;}
int retropicoes_input_menu_pressed(void){return (int)menu;}
void retropicoes_input_get_stats(retropicoes_input_stats_t *s)
{
    memset(s,0,sizeof(*s));s->usb_name[0]="Xbox 360 Wired";s->source_masks[0]=buttons;
}
compat_framebuffer_t *compat_framebuffer(void){return &fb;}
void compat_present(void)
{
    ticks+=50;
    if(!loading_test)return;
    for(unsigned i=0;i<320*240;++i)assert(fb.pixels[i]==0x2104 || fb.pixels[i]==0xffff || fb.pixels[i]==0x39e7 || fb.pixels[i]==0x07e0);
    ti83_load_progress_t p=ti83_program_progress();
    assert(p.stage>=previous_progress.stage);
    if(p.stage==previous_progress.stage)assert(p.completed>=previous_progress.completed);
    assert(!p.total || p.completed<=p.total);
    if(p.stage) {
        unsigned fill=p.stage*90u+(100u*p.completed/p.total)*90u/100u;
        for(unsigned x=1;x<271;++x)assert(fb.pixels[174*320+24+x]==(x<=fill?0x07e0:0x39e7));
    }
    seen_stages|=1u<<p.stage;previous_progress=p;
    if(!have_loading_shot && p.stage==1 && p.completed*100u/p.total>=40) {
        screenshot_from(loading_path,fb.pixels);have_loading_shot=1;
    }
    for(unsigned i=200*320;i<240*320;++i)assert(fb.pixels[i]==0x2104);
    if(!have_header){memcpy(loading_header,fb.pixels+80*320,sizeof(loading_header));have_header=1;}
    else assert(!memcmp(loading_header,fb.pixels+80*320,sizeof(loading_header)));
    ++loading_frames[buffer_index];buffer_index^=1;
    fb.pixels=buffer_index?other_pixels:pixels;
}
uint64_t compat_ticks_us(void){return ++ticks;}
uint32_t compat_ticks_ms(void){return (uint32_t)(ticks/1000);}
static void screenshot_from(const char *path,const uint16_t *image)
{
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
    for(unsigned i=0;i<320*240;++i){unsigned v=image[i];fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);}fclose(f);
}
static void screenshot(const char *path) {screenshot_from(path,pixels);}
int main(int argc,char **argv)
{
    if(argc!=4 && argc!=5)return 2;
    system_path=argv[1];char error[96];
    system_path="missing-system.84prom";
    assert(retropicoes_ti83_frontend_start(argv[2],error,sizeof(error))<0);
    assert(strstr(error,"MISSING"));system_path=argv[1];
    menu=1;assert(retropicoes_ti83_frontend_start(argv[2],error,sizeof(error))<0);
    assert(strstr(error,"CANCELLED"));menu=0;
    for(unsigned i=0;i<320*240;++i){pixels[i]=0x1234;other_pixels[i]=0x5678;}
    loading_test=1;
    snprintf(loading_path,sizeof(loading_path),"%s.loading.ppm",argv[3]);
    srand(2026);
    if(retropicoes_ti83_frontend_start(argv[2],error,sizeof(error))){puts(error);return 3;}
    assert(loading_frames[0]>1 && loading_frames[1]>1);
    assert(seen_stages==7u && have_loading_shot);
    ti83_load_progress_t finished=ti83_program_progress();
    assert(finished.stage==2 && finished.completed==finished.total);
    loading_test=0;fb.pixels=pixels;
    printf("LOADING DOUBLE BUFFER: PASS %u/%u complete frames, stable heading, no stale footer or poisoned pixels\n",loading_frames[0],loading_frames[1]);
    for(unsigned i=0;i<60;++i)assert(!retropicoes_ti83_frontend_run_frame());
    screenshot(argv[3]);
    uint16_t before[320*240];memcpy(before,pixels,sizeof(before));
    unsigned cold_loading=loading_frames[0]+loading_frames[1];
    const compat_key_t moves[]={COMPAT_KEY_RIGHT,COMPAT_KEY_DOWN,COMPAT_KEY_LEFT,COMPAT_KEY_UP};
    for(unsigned k=0;k<4;++k){
        buttons=1u<<moves[k];
        for(unsigned i=0;i<12;++i)assert(!retropicoes_ti83_frontend_run_frame());
        buttons=0;for(unsigned i=0;i<12;++i)assert(!retropicoes_ti83_frontend_run_frame());
    }
    unsigned changed=0;for(unsigned i=0;i<320*208;++i)if(before[i]!=pixels[i])++changed;
    assert(changed>0);
    char path[512];snprintf(path,sizeof(path),"%s.input.ppm",argv[3]);screenshot(path);
    assert(!retropicoes_ti83_frontend_pause());assert(!retropicoes_ti83_frontend_resume());
    menu=1;assert(retropicoes_ti83_frontend_run_frame()==2);menu=0;
    retropicoes_ti83_frontend_stop();assert(retropicoes_ti83_frontend_run_frame()<0);
    system_path="missing-system.84prom";
    assert(retropicoes_ti83_frontend_start(argv[2],error,sizeof(error))<0);
    assert(strstr(error,"MISSING"));system_path=argv[1];
    loading_test=1;loading_frames[0]=loading_frames[1]=0;
    previous_progress=(ti83_load_progress_t){0,0,0};srand(2026);
    assert(!retropicoes_ti83_frontend_start(argc==5?argv[4]:argv[2],error,sizeof(error)));
    unsigned warm_loading=loading_frames[0]+loading_frames[1];
    assert(warm_loading<cold_loading);
    loading_test=0;fb.pixels=pixels;
    for(unsigned i=0;i<60;++i)assert(!retropicoes_ti83_frontend_run_frame());
    snprintf(path,sizeof(path),"%s.warm.ppm",argv[3]);screenshot(path);
    assert(!memcmp(before+218*320,pixels+218*320,22*320*sizeof(*pixels)));
    uint16_t warm_before[320*240];memcpy(warm_before,pixels,sizeof(warm_before));
    for(unsigned k=0;k<4 && !memcmp(warm_before,pixels,208*320*sizeof(*pixels));++k){
        buttons=1u<<moves[k];
        for(unsigned i=0;i<12;++i)assert(!retropicoes_ti83_frontend_run_frame());
        buttons=0;for(unsigned i=0;i<12;++i)assert(!retropicoes_ti83_frontend_run_frame());
    }
    assert(memcmp(warm_before,pixels,208*320*sizeof(*pixels)));
    retropicoes_ti83_frontend_stop();
    printf("WARM BOOT: PASS cold/warm loading calls %u/%u, game restarts and responds, still requires system file\n",cold_loading,warm_loading);
    printf("PRODUCTION FRONTEND GAME: PASS missing system/cancel cleanup, imported/auto-started, directions, pause/resume/menu/quit; changed pixels=%u\n",changed);
    return 0;
}
