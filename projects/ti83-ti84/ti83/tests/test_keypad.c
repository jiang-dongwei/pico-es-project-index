/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adapter.h"
#include <assert.h>
#include <string.h>
extern byte ti83_reference_tilem_keypad_read_keys(TilemCalc *);
static unsigned rng=0x8817;
static byte random_byte(void){rng=rng*1664525u+1013904223u;return (byte)(rng>>24);}
static void compare_groups(TilemCalc *c)
{
    for(unsigned group=0;group<256;++group){
        c->keypad.group=(byte)group;
        TilemKeypad before=c->keypad;
        assert(tilem_keypad_read_keys(c)==ti83_reference_tilem_keypad_read_keys(c));
        assert(!memcmp(&before,&c->keypad,sizeof(before)));
    }
}
int main(void)
{
    TilemCalc *c=ti83_create('4');assert(c);
    compare_groups(c);
    tilem_keypad_press_key(c,TILEM_KEY_ON);compare_groups(c);
    tilem_keypad_release_key(c,TILEM_KEY_ON);
    for(unsigned key=1;key<=64;++key){
        tilem_keypad_press_key(c,(int)key);compare_groups(c);
        tilem_keypad_release_key(c,(int)key);compare_groups(c);
    }
    c->keypad.keysdown[0]=1;c->keypad.keysdown[1]=3;
    c->keypad.keysdown[2]=6;c->keypad.keysdown[3]=12;
    compare_groups(c);
    for(unsigned trial=0;trial<1024;++trial){
        for(unsigned row=0;row<8;++row)c->keypad.keysdown[row]=random_byte();
        compare_groups(c);
    }
    tilem_keypad_reset(c);compare_groups(c);
    tilem_calc_free(c);
    puts("Keypad reference: empty/single/multiple/ghost/reset and all group masks PASS");
    return 0;
}
