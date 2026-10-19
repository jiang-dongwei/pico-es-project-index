/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program.h"
#include <assert.h>
#include <string.h>
static void checksum(uint8_t *b,size_t n)
{
    unsigned sum=0;for(size_t i=55;i<n-2;++i)sum+=b[i];
    b[n-2]=(uint8_t)sum;b[n-1]=(uint8_t)(sum>>8);
}
int main(void)
{
    uint8_t b[80]={0};char error[96];ti83_program_t p;
    memcpy(b,"**TI83F*\x1a\x0a\x00",11);
    b[53]=23;b[55]=13;b[57]=6;b[59]=5;
    memcpy(b+60,"TEST",4);b[70]=6;b[72]=4;
    b[74]=0xbb;b[75]=0x6d;b[76]=0xaf;b[77]=0xc9;
    checksum(b,sizeof(b));
    assert(ti83_program_extension("game.8XP"));
    assert(!ti83_program_extension("os.84prom"));
    assert(!ti83_program_extension(NULL));
    assert(ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)));
    assert(p.assembly && p.size==6 && !strcmp(p.name,"TEST"));
    for(size_t n=0;n<sizeof(b);++n)assert(!ti83_program_parse(b,n,&p,error,sizeof(error)));
    b[77]^=1;assert(!ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)));b[77]^=1;
    b[74]=0xef;b[75]=0x7b;checksum(b,sizeof(b));
    assert(!ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)));
    b[74]=0xbb;b[75]=0x6d;b[76]=0xc9;checksum(b,sizeof(b));
    assert(!ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)));
    b[74]=0xe1;b[75]=0x3f;b[76]=0xde;checksum(b,sizeof(b));
    assert(ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)) && !p.assembly);
    b[70]=5;checksum(b,sizeof(b));assert(!ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)));b[70]=6;
    b[59]=0x15;checksum(b,sizeof(b));assert(!ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)));b[59]=5;
    b[60]='1';checksum(b,sizeof(b));assert(!ti83_program_parse(b,sizeof(b),&p,error,sizeof(error)));
    TilemCalc *c=ti83_create('3');assert(c);
    assert(!ti83_program_launch(c,&p,NULL,NULL,error,sizeof(error)));tilem_calc_free(c);
    c=ti83_create('4');assert(c);memset(c->mem,0,c->hw.romsize);c->mem[0]=0x76;
    assert(!ti83_program_launch(c,&p,NULL,NULL,error,sizeof(error)));
    assert(c->linkport.linkemu==TILEM_LINK_EMULATOR_NONE);tilem_calc_free(c);
    puts("8XP validation: checksum, truncation, length, model, CE/shell rejection, transfer timeout cleanup PASS");
    return 0;
}
