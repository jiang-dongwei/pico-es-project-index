/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program.h"
#include "fast_memory.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
static unsigned pulses;
static unsigned boot_steps;
#ifdef TI83_CPU_PROFILE
extern unsigned ti83_cpu_profile[5][256];
extern unsigned ti83_cpu_pc[65536];
extern unsigned ti83_cpu_main_hot,ti83_cpu_main_cold;
extern void ti83_memory_profile(void);
static void profile(const char *phase)
{
    unsigned main_total=ti83_cpu_main_hot+ti83_cpu_main_cold;
    printf("PROFILE %s main-path hot=%u cold=%u hot_pct=%u\n",phase,
        ti83_cpu_main_hot,ti83_cpu_main_cold,
        main_total?(unsigned)(100ull*ti83_cpu_main_hot/main_total):0);
    ti83_cpu_main_hot=ti83_cpu_main_cold=0;
    for(unsigned group=0;group<5;++group){
        unsigned total=0;
        for(unsigned op=0;op<256;++op)total+=ti83_cpu_profile[group][op];
        printf("PROFILE %s group=%u total=%u top=",phase,group,total);
        for(unsigned n=0;n<8;++n){
            unsigned best=0;
            for(unsigned op=1;op<256;++op)if(ti83_cpu_profile[group][op]>ti83_cpu_profile[group][best])best=op;
            printf(" %02X:%u",best,ti83_cpu_profile[group][best]);ti83_cpu_profile[group][best]=0;
        }
        puts("");
    }
    memset(ti83_cpu_profile,0,sizeof(ti83_cpu_profile));
    printf("PROFILE %s PC top=",phase);
    for(unsigned n=0;n<12;++n){
        unsigned best=0;
        for(unsigned pc=1;pc<65536;++pc)if(ti83_cpu_pc[pc]>ti83_cpu_pc[best])best=pc;
        printf(" %04X:%u",best,ti83_cpu_pc[best]);ti83_cpu_pc[best]=0;
    }
    puts("");memset(ti83_cpu_pc,0,sizeof(ti83_cpu_pc));
    ti83_memory_profile();
}
#endif
extern dword ti83_reference_tilem_z80_run_time(TilemCalc *,int,int *);
static int progress(void *p) { (void)p;++pulses;return 1; }
static int boot_progress(void *p) { (void)p;++boot_steps;return 1; }
int main(int argc,char **argv)
{
    if(argc!=4 && argc!=5) return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 3;
    TilemCalc *c=ti83_create('4');if(!c){fclose(f);return 4;}
    if(fread(c->mem,1,c->hw.romsize,f)!=c->hw.romsize)return 5;
    fclose(f);c->hw.stateloaded(c,0);
    f=fopen(argv[2],"rb");if(!f)return 6;
    fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    if(n<0 || n>24074)return 7;
    uint8_t *b=malloc((size_t)n);if(!b)return 8;
    if(fread(b,1,(size_t)n,f)!=(size_t)n)return 9;
    fclose(f);
    ti83_program_t p;char error[96];
    if(!ti83_program_parse(b,(size_t)n,&p,error,sizeof(error))) {puts(error);return 10;}
    assert(ti83_power_on_progress(c,boot_progress,NULL));
    printf("BOOT pc=%04X active=%u program=%s asm=%u bytes=%u simulated_ms=%u\n",c->z80.r.pc.w.l,c->lcd.active,p.name,p.assembly,p.size,boot_steps*10);
    if(!ti83_program_launch(c,&p,progress,NULL,error,sizeof(error))) {puts(error);return 11;}
#ifdef TI83_CPU_PROFILE
    profile("boot/import");
    if(argc==5 && !strcmp(argv[4],"--workload")) {
        assert(ti83_fast_attach(c));
        for(unsigned frame=0;frame<60;++frame)ti83_step(c,16667);
        profile("idle-before");
        for(unsigned frame=0;frame<60;++frame){
            if(frame%24<12)tilem_keypad_press_key(c,TILEM_KEY_RIGHT);
            else tilem_keypad_release_key(c,TILEM_KEY_RIGHT);
            ti83_step(c,16667);
        }
        profile("right-key");ti83_release_keys(c);
        for(unsigned frame=0;frame<60;++frame)ti83_step(c,16667);
        profile("idle-after");ti83_fast_detach(c);free(b);tilem_calc_free(c);return 0;
    }
#endif
    if(argc==5) {
        TilemCalc *fast=tilem_calc_copy(c);assert(fast);assert(ti83_fast_attach(fast));
        const unsigned codes[]={TILEM_KEY_RIGHT,TILEM_KEY_DOWN,TILEM_KEY_LEFT,TILEM_KEY_UP,TILEM_KEY_2ND,TILEM_KEY_ALPHA};
        for(unsigned frame=0;frame<300;++frame){
            ti83_release_keys(c);ti83_release_keys(fast);
            if(frame%24<12){unsigned code=codes[(frame/24)%6];tilem_keypad_press_key(c,(int)code);tilem_keypad_press_key(fast,(int)code);}
            srand(frame+1);ti83_reference_tilem_z80_run_time(c,16667,NULL);srand(frame+1);ti83_step(fast,16667);
            ti83_fast_flush(fast);
            assert(!memcmp(&c->z80.r,&fast->z80.r,sizeof(c->z80.r)));
            assert(c->z80.clock==fast->z80.clock && c->z80.interrupts==fast->z80.interrupts);
            assert(!memcmp(c->mem,fast->mem,c->hw.romsize+c->hw.ramsize+c->hw.lcdmemsize));
            assert(!memcmp(c->hwregs,fast->hwregs,c->hw.nhwregs*sizeof(dword)));
        }
        ti83_fast_detach(fast);tilem_calc_free(fast);
        puts("FAST/REFERENCE: PASS 300 frames, registers/clock/IRQ/full memory/LCD/hardware state, scripted directions/2nd/Alpha");
    }
#ifdef TI83_CPU_PROFILE
    profile("scripted");
#endif
    for(unsigned i=0;i<60;++i)ti83_step(c,16667);
#ifdef TI83_CPU_PROFILE
    profile("idle");
#endif
    uint16_t pixels[320*240];ti83_blit(c,pixels,320,320,240,0,0);
    f=fopen(argv[3],"wb");if(!f)return 12;
    fprintf(f,"P6\n320 240\n255\n");
    for(unsigned i=0;i<320*240;++i) {
        unsigned v=pixels[i];fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);
    }
    fclose(f);printf("GAME LAUNCH: pc=%04X active=%u progress=%u\n",c->z80.r.pc.w.l,c->lcd.active,pulses);
    free(b);tilem_calc_free(c);return 0;
}
