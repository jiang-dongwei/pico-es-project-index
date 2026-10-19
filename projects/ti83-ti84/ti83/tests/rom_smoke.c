/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adapter.h"
#include <stdlib.h>
static void run_ms(TilemCalc *calc,unsigned ms)
{
    while(ms) {unsigned part=ms>100?100:ms;ti83_step(calc,part*1000);ms-=part;}
}
static void key(TilemCalc *calc,unsigned code)
{
    tilem_keypad_press_key(calc,(int)code);run_ms(calc,100);
    tilem_keypad_release_key(calc,(int)code);run_ms(calc,200);
}
int main(int argc,char **argv)
{
    if(argc!=3) {fprintf(stderr,"Usage: rom_smoke full.rom output.ppm\n");return 2;}
    FILE *f=fopen(argv[1],"rb");if(!f)return 3;
    fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    char model=size>0?ti83_model(argv[1],(size_t)size):0;
    TilemCalc *c=ti83_create(model);if(!c){fclose(f);return 4;}
    if(fread(c->mem,1,c->hw.romsize,f)!=c->hw.romsize){fclose(f);tilem_calc_free(c);return 5;}
    fclose(f);if(c->hw.stateloaded)c->hw.stateloaded(c,0);
    run_ms(c,500);key(c,TILEM_KEY_ON);run_ms(c,3000);
    key(c,TILEM_KEY_ENTER);key(c,TILEM_KEY_CLEAR);
    key(c,TILEM_KEY_1);key(c,TILEM_KEY_ADD);key(c,TILEM_KEY_2);key(c,TILEM_KEY_ENTER);
    run_ms(c,500);
    uint16_t *fb=malloc(320*240*sizeof(*fb));if(!fb){tilem_calc_free(c);return 6;}
    ti83_blit(c,fb,320,320,240,0,0);
    f=fopen(argv[2],"wb");if(!f){free(fb);tilem_calc_free(c);return 7;}
    fprintf(f,"P6\n320 240\n255\n");
    for(unsigned i=0;i<320*240;++i){
        unsigned v=fb[i];fputc(((v>>11)&31)*255/31,f);fputc(((v>>5)&63)*255/63,f);fputc((v&31)*255/31,f);
    }
    fclose(f);free(fb);
    printf("ROM SMOKE: model=%c lcd_active=%u pc=%04X halted=%d clocks=%u lastlcdwrite=%u\n",
           model,(unsigned)c->lcd.active,(unsigned)c->z80.r.pc.w.l,c->z80.halted,
           (unsigned)c->z80.clock,(unsigned)c->z80.lastlcdwrite);
    int ok=c->lcd.active!=0;tilem_calc_free(c);return ok?0:8;
}
