/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adapter.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static void models(void)
{
    assert(ti83_model("calc.83rom",0x40000)=='3');
    assert(ti83_model("calc.83prom",0x80000)=='p');
    assert(ti83_model("calc.84prom",0x100000)=='4');
    assert(!ti83_model("wrong.83rom",0x100000));
    assert(!ti83_model("ce.rom",0x400000));
    assert(!ti83_model("program.8xp",0x80000));
    assert(!ti83_model(NULL,0x40000));
    assert(ti83_extension("CALC.84PROM"));
    assert(!ti83_create('z'));
}
static void machine(char model)
{
    TilemCalc *c=ti83_create(model); assert(c);
    assert(c->hw.lcdwidth==96 && c->hw.lcdheight==64);
    memset(c->mem,0,c->hw.romsize);
    /* Original synthetic Z80 program: LD A,2A; ADD A,18; HALT. */
    const byte program[]={0x3e,0x2a,0xc6,0x18,0x76};
    memcpy(c->mem,program,sizeof(program)); c->z80.r.pc.d=0;
    ti83_step(c,100); assert(c->z80.r.af.b.h==0x42 && c->z80.halted);
    ti83_input_t input; ti83_input_reset(&input,0);
    ti83_input_update(c,&input,1|16);
    tilem_keypad_set_group(c,0xfe); assert(!(tilem_keypad_read_keys(c)&2));
    ti83_input_update(c,&input,0); assert(tilem_keypad_read_keys(c)==0xff);
    input.game=1;ti83_input_update(c,&input,16|32);
    assert(input.down[TILEM_KEY_2ND] && input.down[TILEM_KEY_ALPHA]);
    assert(!input.down[TILEM_KEY_ENTER] && !input.down[TILEM_KEY_CLEAR]);
    ti83_input_update(c,&input,0);
    ti83_input_update(c,&input,64); assert(input.keyboard);
    ti83_input_update(c,&input,64); assert(input.keyboard);
    ti83_input_update(c,&input,0); input.cursor=20;
    c->keypad.onkeyint=1; c->z80.interrupts=0;
    ti83_input_update(c,&input,16); assert(c->keypad.onkeydown);
    c->z80.interrupts=0; ti83_input_update(c,&input,16);
    assert(c->z80.interrupts==0); /* Held ON must not retrigger every frame. */
    ti83_release_keys(c); assert(!c->keypad.onkeydown);
    ti83_input_reset(&input,1); input.cursor=0;
    ti83_input_update(c,&input,1); assert(input.cursor==6);
    ti83_input_update(c,&input,0); ti83_input_update(c,&input,4); assert(input.cursor==48);
    uint16_t *fb=malloc((320*240+2)*sizeof(*fb)); assert(fb);
    fb[0]=fb[320*240+1]=0xdead;
    assert(!ti83_blit(c,fb+1,319,320,240,0,0));
    c->lcd.active=1; c->lcd.y=0; c->lcd.rowshift=0;
    memset(c->lcdmem,0,c->hw.lcdmemsize); c->lcdmem[0]=0x80;
    assert(ti83_blit(c,fb+1,320,320,240,0,0));
    assert(fb[1+16*320+16]==0x18c3);
    assert(fb[1+16*320+19]==0xbe77);
    assert(ti83_blit(c,fb+1,320,320,240,1,1));
    assert(fb[1+4*320+64]==0x18c3);
    assert(fb[1+5*320+64]==((0x18c3&0xf7de)>>1));
    assert(fb[0]==0xdead && fb[320*240+1]==0xdead);
    free(fb); tilem_calc_free(c);
}
int main(void)
{
    models(); machine('3'); machine('p'); machine('4');
    void *p=tilem_malloc(24); memset(p,0x56,24);
    p=tilem_realloc(p,64); for(unsigned i=0;i<24;++i) assert(((byte*)p)[i]==0x56);
    tilem_free(p); assert(!tilem_try_malloc(5u*1024*1024));
    puts("TI83: model, Z80, keypad edges, ON, keyboard wrap, LCD geometry, allocation PASS");
    return 0;
}
