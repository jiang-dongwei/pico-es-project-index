/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "oric.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static oric_t s;
static uint8_t video[ORIC_WIDTH*ORIC_HEIGHT], rom[ORIC_ROM_SIZE];
static void screen(void)
{
    for(unsigned row=0;row<28;++row) {
        for(unsigned x=0;x<40;++x) {
            unsigned c=s.memory[0xbb80+row*40+x]&127;
            putchar(c>=32 && c<127 ? (int)c : ' ');
        }
        putchar('\n');
    }
}
static void frames(unsigned n) { while(n--) oric_run_frame(&s); }
static void type(const char *str)
{
    while(*str) {
        bool shift; int key=oric_ascii_key(*str++,&shift);
        if(key<0) continue;
        oric_key(&s,(unsigned)key,true);
        if(shift) oric_key(&s,ORIC_KEY_SHIFT,true);
        frames(5); oric_release_keys(&s); frames(5);
    }
}
int main(int argc,char **argv)
{
    if(argc<3) { fprintf(stderr,"rom_probe BASIC.rom program.tap [output.ppm]\n"); return 2; }
    FILE *f=fopen(argv[1],"rb");
    if(!f || fread(rom,1,sizeof(rom),f)!=sizeof(rom)) return 2;
    fclose(f);
    const oric_model_t model=strstr(argv[1],"basic10") ? ORIC_MODEL_ORIC1 : ORIC_MODEL_ATMOS;
    if(oric_init(&s,model,rom,sizeof(rom),video,sizeof(video))!=ORIC_OK) return 2;
    frames(150); screen();
    f=fopen(argv[2],"rb"); if(!f) return 2;
    fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
    if(n<=0 || n>ORIC_MAX_TAPE) return 2;
    uint8_t *data=malloc((size_t)n); if(!data) return 2;
    if(fread(data,1,(size_t)n,f)!=(size_t)n) return 2;
    fclose(f);
    if(oric_attach_tape(&s,data,(size_t)n)!=ORIC_OK) return 2;
    type("CLOAD\"\"\r");
    unsigned count=0;
    /* Original test programs use SEI/CLD/LDX and increment guest $21. */
    while(count++<6000u) {
        oric_run_frame(&s);
        if(s.memory[0x600]==0x78 && s.memory[0x601]==0xd8 &&
           s.memory[0x602]==0xa2 && s.memory[0x21]>5 &&
           s.cpu.PC>=0x600 && s.cpu.PC<0x1000) break;
    }
    if(count<=6000u) frames(20); /* finish original guest initialization */
    printf("probe: model=%u frames=%u PC=%04x tape=%zu/%zu motor=%02x guest=%u\n",
           model,count,s.cpu.PC,s.tape_offset,s.tape_size,s.via.pb.outr,s.memory[0x21]);
    screen();
    printf("VIA acr=%02x pcr=%02x ifr=%02x ier=%02x t1=%04x/%04x t2=%04x CPU S=%02x P=%02x\n",s.via.acr,s.via.pcr,s.via.intr.ifr,s.via.intr.ier,s.via.t1.counter,s.via.t1.latch,s.via.t2.counter,s.cpu.S,s.cpu.P);
    printf("RAM0600:"); for(unsigned i=0;i<24;++i) printf(" %02x",s.memory[0x600+i]);
    printf("\nTAPE pointers:"); for(unsigned i=0x290;i<0x2c0;++i) printf(" %02x",s.memory[i]);
    putchar('\n');
    oric_tape_block_t block;
    oric_tape_block(data,(size_t)n,0,&block);
    unsigned mismatch=0;
    for(unsigned i=0;i<=(unsigned)(block.end-block.start);++i) {
        if(s.memory[block.start+i]!=data[block.data_offset+i]) {
            if(mismatch++<12) printf("mismatch %04x got %02x expected %02x\n",block.start+i,s.memory[block.start+i],data[block.data_offset+i]);
        }
    }
    printf("loaded mismatches=%u stack:",mismatch);
    for(unsigned i=0x1f0;i<0x200;++i) printf(" %02x",s.memory[i]);
    putchar('\n');
    if(argc>3) {
        const uint8_t palette[8][3]={{0,0,0},{255,0,0},{0,255,0},{255,255,0},
            {0,0,255},{255,0,255},{0,255,255},{255,255,255}};
        f=fopen(argv[3],"wb"); if(!f) return 2;
        fprintf(f,"P6\n240 224\n255\n");
        for(unsigned i=0;i<sizeof(video);++i) fwrite(palette[video[i]&7],1,3,f);
        fclose(f);
    }
    free(data);
    return count<=6000u && mismatch==0 && s.memory[0x20]==18 &&
        (s.ay.enable==0x7e || s.ay.enable==0x77) ? 0 : 1;
}
