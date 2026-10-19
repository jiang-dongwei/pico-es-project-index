/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "oric.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static oric_t machine;
static uint8_t rom[ORIC_ROM_SIZE], video[ORIC_WIDTH * ORIC_HEIGHT + 2];
static uint8_t streamed[ORIC_WIDTH * ORIC_HEIGHT];
static void collect_line(void *context,unsigned y,const uint8_t *pixels)
{
    (void)context; assert(y<ORIC_HEIGHT);
    memcpy(streamed+y*ORIC_WIDTH,pixels,ORIC_WIDTH);
}
static void compare_streaming(void)
{
    oric_t *copy=malloc(sizeof(*copy));assert(copy);
    *copy=machine;copy->video=NULL;oric_set_line_sink(copy,collect_line,NULL);
    oric_run_frame(&machine);oric_run_frame(copy);
    assert(!memcmp(streamed,video+1,sizeof(streamed)));
    assert(!memcmp(copy->memory,machine.memory,sizeof(copy->memory)));
    assert(copy->cpu.PC==machine.cpu.PC && copy->ticks==machine.ticks);
    assert(copy->audio_count==machine.audio_count);
    assert(!memcmp(copy->audio,machine.audio,machine.audio_count*sizeof(int16_t)));
    free(copy);
}
static void compare_packed(void)
{
    oric_t *copy=malloc(sizeof(*copy));assert(copy);
    uint8_t *packed=malloc(ORIC_WIDTH*ORIC_HEIGHT*3/8+2);assert(packed);
    memset(packed,0,ORIC_WIDTH*ORIC_HEIGHT*3/8+2);
    packed[0]=0xa5;packed[ORIC_WIDTH*ORIC_HEIGHT*3/8+1]=0x5a;
    *copy=machine;
    oric_set_packed_video(copy,packed+1);
    for(unsigned frame=0;frame<120;++frame) {
        oric_run_frame(&machine);oric_run_frame(copy);
        assert(!memcmp(&machine.cpu,&copy->cpu,sizeof(machine.cpu)));
        assert(!memcmp(&machine.via,&copy->via,sizeof(machine.via)));
        assert(!memcmp(&machine.ay,&copy->ay,sizeof(machine.ay)));
        assert(!memcmp(machine.memory,copy->memory,sizeof(machine.memory)));
        assert(machine.pins==copy->pins && machine.ticks==copy->ticks);
        assert(machine.ula_mode==copy->ula_mode);
        assert(machine.audio_count==copy->audio_count);
        assert(!memcmp(machine.audio,copy->audio,machine.audio_count*sizeof(int16_t)));
        for(unsigned y=0;y<ORIC_HEIGHT;++y)
            for(unsigned x=0;x<ORIC_WIDTH;++x) {
                const unsigned offset=y*(ORIC_WIDTH*3/8)+(x/8)*3;
                const uint32_t bits=(uint32_t)packed[1+offset]|
                    ((uint32_t)packed[1+offset+1]<<8)|
                    ((uint32_t)packed[1+offset+2]<<16);
                assert(((bits>>((x&7u)*3u))&7u)==video[1+y*ORIC_WIDTH+x]);
            }
    }
    uint32_t seed=0x12345678u;
    for(unsigned address=0x9800u;address<0xc000u;++address) {
        seed=seed*1664525u+1013904223u;
        machine.memory[address]=copy->memory[address]=(uint8_t)(seed>>24u);
    }
    for(unsigned pass=0;pass<4;++pass) {
        machine.ula_mode=copy->ula_mode=(pass&1u)?6u:2u;
        machine.frames=copy->frames=(pass&2u)?16u:0u;
        for(unsigned y=0;y<ORIC_HEIGHT;++y) {
            oric_render_line(&machine,y);oric_render_line(copy,y);
            assert(machine.ula_mode==copy->ula_mode);
            for(unsigned x=0;x<ORIC_WIDTH;++x) {
                const unsigned offset=y*(ORIC_WIDTH*3/8)+(x/8)*3;
                const uint32_t bits=(uint32_t)packed[1+offset]|
                    ((uint32_t)packed[1+offset+1]<<8)|
                    ((uint32_t)packed[1+offset+2]<<16);
                assert(((bits>>((x&7u)*3u))&7u)==video[1+y*ORIC_WIDTH+x]);
            }
        }
    }
    assert(packed[0]==0xa5 && packed[ORIC_WIDTH*ORIC_HEIGHT*3/8+1]==0x5a);
    free(packed);free(copy);
}
static void compare_via_fast_path(void)
{
    oric_t *reference=malloc(sizeof(*reference));assert(reference);
    uint8_t *reference_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(reference_video);
    *reference=machine;
    memcpy(reference_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    reference->video=reference_video;
    reference->via_fast_enabled=false;
    for(unsigned frame=0;frame<80;++frame) {
        if(frame==20 || frame==30 || frame==40 || frame==50 ||
           frame==60 || frame==70) {
            uint8_t extra=frame==20 ? 0x80 : frame==40 ? 0x03 :
                          frame==60 ? 0x20 : 0;
            machine.via.acr=(machine.via.acr&0x5c)|extra;
            reference->via.acr=(reference->via.acr&0x5c)|extra;
        }
        const bool left=(frame%11)<4, right=(frame%13)<5;
        oric_key(&machine,ORIC_KEY_LEFT,left);
        oric_key(reference,ORIC_KEY_LEFT,left);
        oric_key(&machine,ORIC_KEY_RIGHT,right);
        oric_key(reference,ORIC_KEY_RIGHT,right);
        oric_run_frame(&machine);
        oric_run_frame(reference);
        assert(!memcmp(&machine.cpu,&reference->cpu,sizeof(machine.cpu)));
        assert(!memcmp(&machine.via,&reference->via,sizeof(machine.via)));
        assert(!memcmp(&machine.ay,&reference->ay,sizeof(machine.ay)));
        assert(!memcmp(machine.memory,reference->memory,sizeof(machine.memory)));
        assert(machine.pins==reference->pins && machine.ticks==reference->ticks);
        assert(machine.tape_offset==reference->tape_offset);
        assert(machine.audio_count==reference->audio_count);
        assert(!memcmp(machine.audio,reference->audio,machine.audio_count*sizeof(int16_t)));
        assert(!memcmp(video+1,reference_video,ORIC_WIDTH*ORIC_HEIGHT));
    }
    oric_release_keys(&machine);
    free(reference_video);free(reference);
}
static void compare_via_steady_path(void)
{
    oric_t *original=malloc(sizeof(*original));assert(original);
    uint8_t *original_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(original_video);
    *original=machine;
    memcpy(original_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    oric_t *reference=malloc(sizeof(*reference));assert(reference);
    uint8_t *reference_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(reference_video);
    *reference=machine;
    memcpy(reference_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    reference->video=reference_video;
    reference->via_steady_enabled=false;
    for(unsigned frame=0;frame<80;++frame) {
        if(frame%10u==0u) {
            machine.via.t1.counter=reference->via.t1.counter=3u;
            machine.via.t2.counter=reference->via.t2.counter=3u;
        }
        const bool key=(frame%13u)<6u;
        oric_key(&machine,ORIC_KEY_RIGHT,key);
        oric_key(reference,ORIC_KEY_RIGHT,key);
        oric_run_frame(&machine);
        oric_run_frame(reference);
        assert(!memcmp(&machine.cpu,&reference->cpu,sizeof(machine.cpu)));
        assert(!memcmp(&machine.via,&reference->via,sizeof(machine.via)));
        assert(!memcmp(&machine.ay,&reference->ay,sizeof(machine.ay)));
        assert(!memcmp(machine.memory,reference->memory,sizeof(machine.memory)));
        assert(machine.pins==reference->pins && machine.ticks==reference->ticks);
        assert(machine.audio_count==reference->audio_count);
        assert(!memcmp(machine.audio,reference->audio,machine.audio_count*sizeof(int16_t)));
        assert(!memcmp(video+1,reference_video,ORIC_WIDTH*ORIC_HEIGHT));
    }
    machine=*original;
    memcpy(video+1,original_video,ORIC_WIDTH*ORIC_HEIGHT);
    free(original_video);free(original);
    free(reference_video);free(reference);
}
static void compare_via_lazy_path(void)
{
    oric_t *original=malloc(sizeof(*original));assert(original);
    uint8_t *original_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(original_video);
    *original=machine;
    memcpy(original_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    oric_t *reference=malloc(sizeof(*reference));assert(reference);
    uint8_t *reference_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(reference_video);
    *reference=machine;
    memcpy(reference_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    reference->video=reference_video;
    reference->via_lazy_enabled=false;
    for(unsigned frame=0;frame<80;++frame) {
        if(frame%10u==0u) {
            machine.via.t1.counter=reference->via.t1.counter=3u;
            machine.via.t2.counter=reference->via.t2.counter=3u;
        }
        const bool key=(frame%13u)<6u;
        oric_key(&machine,ORIC_KEY_RIGHT,key);
        oric_key(reference,ORIC_KEY_RIGHT,key);
        oric_run_frame(&machine);
        oric_run_frame(reference);
        assert(!memcmp(&machine.cpu,&reference->cpu,sizeof(machine.cpu)));
        assert(!memcmp(&machine.via,&reference->via,sizeof(machine.via)));
        assert(!memcmp(&machine.ay,&reference->ay,sizeof(machine.ay)));
        assert(!memcmp(machine.memory,reference->memory,sizeof(machine.memory)));
        assert(machine.pins==reference->pins && machine.ticks==reference->ticks);
        assert(machine.tape_level==reference->tape_level &&
               machine.tape_countdown==reference->tape_countdown &&
               machine.tape_offset==reference->tape_offset);
        assert(machine.audio_count==reference->audio_count);
        assert(!memcmp(machine.audio,reference->audio,machine.audio_count*sizeof(int16_t)));
        assert(!memcmp(video+1,reference_video,ORIC_WIDTH*ORIC_HEIGHT));
    }
    machine=*original;
    memcpy(video+1,original_video,ORIC_WIDTH*ORIC_HEIGHT);
    free(original_video);free(original);
    free(reference_video);free(reference);
}
static void compare_via_quiet_shortcut(void)
{
    oric_t *original=malloc(sizeof(*original));assert(original);
    uint8_t *original_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(original_video);
    *original=machine;
    memcpy(original_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    oric_t *reference=malloc(sizeof(*reference));assert(reference);
    uint8_t *reference_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(reference_video);
    *reference=machine;
    memcpy(reference_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    reference->video=reference_video;
    reference->via_quiet_shortcut_enabled=false;
    for(unsigned frame=0;frame<140;++frame) {
        if(frame%19u==0u) {
            machine.via.t1.counter=reference->via.t1.counter=2u;
            machine.via.t2.counter=reference->via.t2.counter=3u;
        }
        if(frame%17u==0u)
            machine.tape_level=reference->tape_level^=1u;
        const bool key=(frame%11u)<5u;
        oric_key(&machine,ORIC_KEY_LEFT,key);
        oric_key(reference,ORIC_KEY_LEFT,key);
        oric_run_frame(&machine);
        oric_run_frame(reference);
        assert(!memcmp(&machine.cpu,&reference->cpu,sizeof(machine.cpu)));
        assert(!memcmp(&machine.via,&reference->via,sizeof(machine.via)));
        assert(!memcmp(&machine.ay,&reference->ay,sizeof(machine.ay)));
        assert(!memcmp(machine.memory,reference->memory,sizeof(machine.memory)));
        assert(machine.pins==reference->pins && machine.ticks==reference->ticks);
        assert(machine.tape_level==reference->tape_level &&
               machine.tape_offset==reference->tape_offset);
        assert(machine.audio_count==reference->audio_count);
        assert(!memcmp(machine.audio,reference->audio,machine.audio_count*sizeof(int16_t)));
        assert(!memcmp(video+1,reference_video,ORIC_WIDTH*ORIC_HEIGHT));
    }
    machine=*original;
    memcpy(video+1,original_video,ORIC_WIDTH*ORIC_HEIGHT);
    free(original_video);free(original);
    free(reference_video);free(reference);
}
static void compare_ay_read_fast_path(void)
{
    oric_t *reference=malloc(sizeof(*reference));assert(reference);
    uint8_t *reference_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(reference_video);
    *reference=machine;
    memcpy(reference_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    reference->video=reference_video;
    reference->ay_read_fast_enabled=false;
    reference->ay_tick_fast_enabled=false;
    for(unsigned frame=0;frame<80;++frame) {
        const bool key=(frame%17u)<5u;
        oric_key(&machine,ORIC_KEY_SPACE,key);
        oric_key(reference,ORIC_KEY_SPACE,key);
        oric_run_frame(&machine);
        oric_run_frame(reference);
        assert(!memcmp(&machine.cpu,&reference->cpu,sizeof(machine.cpu)));
        assert(!memcmp(&machine.via,&reference->via,sizeof(machine.via)));
        assert(!memcmp(&machine.ay,&reference->ay,sizeof(machine.ay)));
        assert(!memcmp(machine.memory,reference->memory,sizeof(machine.memory)));
        assert(machine.pins==reference->pins && machine.ticks==reference->ticks);
        assert(machine.audio_count==reference->audio_count);
        assert(!memcmp(machine.audio,reference->audio,machine.audio_count*sizeof(int16_t)));
        assert(!memcmp(video+1,reference_video,ORIC_WIDTH*ORIC_HEIGHT));
    }
    oric_release_keys(&machine);
    free(reference_video);free(reference);
}
static void compare_video_cache(void)
{
    oric_t *reference=malloc(sizeof(*reference));assert(reference);
    uint8_t *reference_video=malloc(ORIC_WIDTH*ORIC_HEIGHT);assert(reference_video);
    *reference=machine;
    memcpy(reference_video,video+1,ORIC_WIDTH*ORIC_HEIGHT);
    reference->video=reference_video;
    reference->video_cache_enabled=false;
    for(unsigned frame=0;frame<80;++frame) {
        oric_run_frame(&machine);
        oric_run_frame(reference);
        assert(machine.ula_mode==reference->ula_mode);
        assert(!memcmp(video+1,reference_video,ORIC_WIDTH*ORIC_HEIGHT));
        assert(!memcmp(&machine.cpu,&reference->cpu,sizeof(machine.cpu)));
        assert(!memcmp(&machine.via,&reference->via,sizeof(machine.via)));
        assert(!memcmp(&machine.ay,&reference->ay,sizeof(machine.ay)));
        assert(!memcmp(machine.memory,reference->memory,sizeof(machine.memory)));
        assert(machine.audio_count==reference->audio_count);
        assert(!memcmp(machine.audio,reference->audio,machine.audio_count*sizeof(int16_t)));
    }
    free(reference_video);free(reference);
}

static void init(void)
{
    memset(rom, 0xea, sizeof(rom));
    rom[0] = 0x4c; rom[1] = 0; rom[2] = 0xc0;
    rom[0x3ffc] = 0; rom[0x3ffd] = 0xc0;
    video[0] = 0xa5; video[sizeof(video)-1] = 0x5a;
    assert(oric_init(&machine, ORIC_MODEL_ATMOS, rom, sizeof(rom),
                    video+1, sizeof(video)-2) == ORIC_OK);
}
static void memory_and_cpu(void)
{
    init();
    oric_run_cycles(&machine, 100);
    assert(machine.cpu.PC >= 0xc000 && machine.cpu.PC <= 0xc003);
    /* LDA #42; STA $C100 (ROM); STA $0200; decimal 49+51 ->00/C. */
    const uint8_t code[] = {0xa9,42,0x8d,0,0xc1,0x8d,0,2,0xf8,
                            0x18,0xa9,0x49,0x69,0x51,0x8d,1,2,0x4c,17,4};
    memcpy(machine.memory+0x400,code,sizeof(code));
    oric_jump(&machine,0x400); oric_run_cycles(&machine,100);
    assert(machine.memory[0xc100] == 0xea);
    assert(machine.memory[0x200] == 42 && machine.memory[0x201] == 0);
    assert(machine.cpu.P & M6502_CF);
    assert(oric_init(&machine, ORIC_MODEL_ATMOS, rom, sizeof(rom)-1,
                    video+1, sizeof(video)-2) == ORIC_INVALID_ROM);
}
static void ula(void)
{
    init();
    memset(machine.memory+0xbb80,32,1120);
    memset(machine.memory+0xb500,0,8); /* space */
    machine.memory[0xbb80]=1; machine.memory[0xbb81]='A';
    machine.memory[0xb400+'A'*8]=0x21;
    oric_render_line(&machine,0);
    assert(video[1+6] == 1 && video[1+7] == 0 && video[1+11] == 1);
    machine.memory[0xbb81]='A'|0x80;
    oric_render_line(&machine,0);
    assert(video[1+6] == 6 && video[1+7] == 7);
    machine.ula_mode=6;
    machine.memory[0xa000]=2; machine.memory[0xa001]=0x60;
    oric_render_line(&machine,0);
    assert(video[1+6] == 2 && video[1+7] == 0);
    /* Bottom three rows use text even in HIRES, with its 9800 charset. */
    machine.memory[0xbb80+25*40]='B';
    machine.memory[0x9800+'B'*8]=63;
    oric_render_line(&machine,200);
    assert(video[1+200*240] == 7);
    oric_render_line(&machine,224);
    assert(video[0] == 0xa5 && video[sizeof(video)-1] == 0x5a);
}
static void via_timer_and_audio(void)
{
    init();
    /* One-shot short cassette timer must wrap, allowing BASIC to restore
       its keyboard timer before another underflow. A reload here starves it. */
    const uint8_t code[]={0x78,0xa9,0,0x8d,0x0b,3,0xa9,16,0x8d,4,3,
        0xa9,0,0x8d,5,3,0x4c,16,4};
    memcpy(machine.memory+0x400,code,sizeof(code));oric_jump(&machine,0x400);
    oric_run_cycles(&machine,200);
    assert(machine.via.t1.counter>0xff00 && (machine.via.intr.ifr&0x40));
    init(); unsigned samples=0;
    for(unsigned i=0;i<500;++i){oric_run_frame(&machine);samples+=(unsigned)machine.audio_count;}
    unsigned expected=(unsigned)((uint64_t)machine.ticks*ORIC_AUDIO_RATE/ORIC_CLOCK);
    assert(samples<=expected && expected-samples<=1 && machine.audio_overflow==0);
}
static uint8_t *load(const char *root,const char *name,size_t *size)
{
    char path[1024]; snprintf(path,sizeof(path),"%s/%s",root,name);
    FILE *f=fopen(path,"rb"); assert(f);
    assert(fseek(f,0,SEEK_END)==0); long n=ftell(f); assert(n>0);
    rewind(f); uint8_t *p=malloc((size_t)n); assert(p);
    assert(fread(p,1,(size_t)n,f)==(size_t)n); fclose(f); *size=(size_t)n;
    return p;
}
static void tapes_and_machine(const char *root)
{
    size_t size;
    uint8_t *tape=load(root,"PicoES_Oric_Controls.tap",&size);
    oric_tape_block_t b;
    assert(oric_tape_block(tape,size,0,&b)==ORIC_OK);
    assert(b.start==0x600 && b.type==0x80 && b.next_offset==size);
    for (size_t i=0;i<size;++i)
        assert(oric_tape_block(tape,i,0,&b)!=ORIC_OK);
    init(); assert(oric_attach_tape(&machine,tape,size)==ORIC_OK);
    assert(oric_load_binary(&machine,tape,size)==ORIC_OK);
    for (unsigned i=0;i<5;++i) oric_run_frame(&machine);
    assert(machine.memory[0x20]==18);
    compare_via_fast_path();
    compare_via_steady_path();
    compare_via_lazy_path();
    compare_via_quiet_shortcut();
    compare_ay_read_fast_path();
    compare_video_cache();
    compare_streaming();
    compare_packed();
    assert(machine.memory[0x21]>0);
    assert(machine.ay.enable==0x7e);
    oric_key(&machine,ORIC_KEY_RIGHT,true);
    for (unsigned i=0;i<5;++i) oric_run_frame(&machine);
    assert(machine.memory[0x20]>18);
    oric_key(&machine,ORIC_KEY_RIGHT,false);
    oric_key(&machine,ORIC_KEY_LEFT,true);
    for (unsigned i=0;i<10;++i) oric_run_frame(&machine);
    assert(machine.memory[0x20]<18);
    oric_release_keys(&machine);
    uint8_t x=machine.memory[0x20];
    for (unsigned i=0;i<3;++i) oric_run_frame(&machine);
    assert(machine.memory[0x20]==x);
    oric_key(&machine,ORIC_KEY_SPACE,true);
    oric_run_frame(&machine); oric_run_frame(&machine);
    int nonzero=0;
    for(size_t i=0;i<machine.audio_count;++i) nonzero |= machine.audio[i]!=0;
    assert(nonzero && machine.ay.reg[8]==13);
    assert(machine.audio_count>=439 && machine.audio_count<=442);
    assert(machine.audio_overflow==0);
    oric_release_keys(&machine); oric_run_frame(&machine);
    assert(machine.ay.reg[8]==0);
    free(tape);
    tape=load(root,"PicoES_Oric_Hires.tap",&size);
    init(); assert(oric_load_binary(&machine,tape,size)==ORIC_OK);
    for(unsigned i=0;i<40;++i) oric_run_frame(&machine);
    compare_video_cache();
    assert(machine.ula_mode==6 && machine.ay.enable==0x77);
    /* The HIRES fixture is a legible chart, not a raw byte ramp. Its color
       bands, blank interior and text overlay catch guest-image regressions. */
    assert(machine.memory[0xa000]==30 && machine.memory[0xa001]==1);
    assert(video[1+12]==1 && video[1+10*ORIC_WIDTH+30]==0);
    assert(video[1+72*ORIC_WIDTH+12]==2);
    assert(machine.memory[0xbb80+25*40+12]=='O');
    assert(machine.memory[0xbb80+26*40+11]=='P');
    assert(machine.memory[0xbb81]==0x40);
    for(unsigned y=176;y<200;++y)
        for(unsigned x=0;x<ORIC_WIDTH;++x)
            assert(video[1+y*ORIC_WIDTH+x]==0);
    assert(video[1+200*ORIC_WIDTH+12*6+1]==7);
    compare_streaming();
    assert(machine.memory[0x21]>0);
    assert(video[0]==0xa5 && video[sizeof(video)-1]==0x5a);
    free(tape);
    tape=load(root,"PicoES_Oric_Audio.tap",&size);
    assert(oric_tape_block(tape,size,0,&b)==ORIC_OK);
    assert(b.type==0x80 && b.autorun && b.next_offset==size);
    init(); assert(oric_load_binary(&machine,tape,size)==ORIC_OK);
    for(unsigned i=0;i<30;++i) oric_run_frame(&machine);
    assert(machine.memory[0xbb80+11*40+19]=='C');
    assert(machine.ay.reg[0]==239 && machine.ay.reg[8]==13);
    int audible=0;
    for(size_t i=0;i<machine.audio_count;++i) audible|=machine.audio[i]!=0;
    assert(audible);
    for(unsigned i=0;i<25;++i) oric_run_frame(&machine);
    assert(machine.memory[0xbb80+11*40+19]=='D');
    assert(machine.ay.reg[0]==213 && machine.ay.reg[8]==13);
    for(unsigned i=0;i<175;++i) oric_run_frame(&machine);
    assert(machine.memory[0xbb80+11*40+19]=='S');
    assert(machine.ay.reg[8]==0 && machine.audio_overflow==0);
    for(size_t i=0;i<machine.audio_count;++i) assert(machine.audio[i]==0);
    for(unsigned i=0;i<25;++i) oric_run_frame(&machine);
    assert(machine.memory[0xbb80+11*40+19]=='C');
    assert(machine.ay.reg[0]==239 && machine.ay.reg[8]==13);
    free(tape);
}
int main(int argc,char **argv)
{
    assert(argc==2);
    memory_and_cpu(); ula(); via_timer_and_audio(); tapes_and_machine(argv[1]);
    printf("ORIC host: CPU, ROM protection, ULA, malformed TAP, two guest programs, keyboard and AY checks passed\n");
    return 0;
}
