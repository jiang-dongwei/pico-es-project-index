/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adapter.h"
#include <ctype.h>
#include <string.h>
static int suffix(const char *name, const char *ext)
{
    if (!name) return 0;
    size_t n = strlen(name), e = strlen(ext);
    if (n < e) return 0;
    for (size_t i = 0; i < e; ++i)
        if (tolower((unsigned char)name[n-e+i]) != ext[i]) return 0;
    return 1;
}
int ti83_extension(const char *name)
{
    return suffix(name,".rom") || suffix(name,".83rom") ||
           suffix(name,".83prom") || suffix(name,".84prom");
}
char ti83_model(const char *name, size_t size)
{
    if (!ti83_extension(name)) return 0;
    if (suffix(name,".83rom")) return size == 0x40000 ? '3' : 0;
    if (suffix(name,".83prom")) return size == 0x80000 ? 'p' : 0;
    if (suffix(name,".84prom")) return size == 0x100000 ? '4' : 0;
    return size == 0x40000 ? '3' : size == 0x80000 ? 'p' :
           size == 0x100000 ? '4' : 0;
}
TilemCalc *ti83_create(char model)
{
    if (model != '3' && model != 'p' && model != '4') return NULL;
    return tilem_calc_new(model);
}
void ti83_step(TilemCalc *calc, unsigned microseconds)
{
    if (calc && microseconds <= 100000u)
        (void)tilem_z80_run_time(calc, (int)microseconds, NULL);
}
void ti83_release_keys(TilemCalc *calc)
{
    if (calc) for (int key = 1; key <= 0x38; ++key)
        tilem_keypad_release_key(calc,key);
}
const unsigned ti83_keycodes[49] = {
    TILEM_KEY_YEQU,TILEM_KEY_WINDOW,TILEM_KEY_ZOOM,TILEM_KEY_TRACE,TILEM_KEY_GRAPH,TILEM_KEY_2ND,TILEM_KEY_ALPHA,
    TILEM_KEY_MODE,TILEM_KEY_DEL,TILEM_KEY_LEFT,TILEM_KEY_RIGHT,TILEM_KEY_UP,TILEM_KEY_DOWN,TILEM_KEY_CLEAR,
    TILEM_KEY_MATH,TILEM_KEY_MATRIX,TILEM_KEY_PRGM,TILEM_KEY_VARS,TILEM_KEY_STAT,TILEM_KEY_GRAPHVAR,TILEM_KEY_ON,
    TILEM_KEY_RECIP,TILEM_KEY_SIN,TILEM_KEY_COS,TILEM_KEY_TAN,TILEM_KEY_POWER,TILEM_KEY_SQUARE,TILEM_KEY_STORE,
    TILEM_KEY_LN,TILEM_KEY_LOG,TILEM_KEY_7,TILEM_KEY_8,TILEM_KEY_9,TILEM_KEY_DIV,TILEM_KEY_LPAREN,
    TILEM_KEY_4,TILEM_KEY_5,TILEM_KEY_6,TILEM_KEY_MUL,TILEM_KEY_1,TILEM_KEY_2,TILEM_KEY_3,
    TILEM_KEY_SUB,TILEM_KEY_0,TILEM_KEY_DECPNT,TILEM_KEY_CHS,TILEM_KEY_ADD,TILEM_KEY_RPAREN,TILEM_KEY_ENTER
};
const char *const ti83_keylabels[49] = {
    "Y=","WIN","ZOOM","TRACE","GRAPH","2ND","ALPHA",
    "MODE","DEL","LEFT","RIGHT","UP","DOWN","CLEAR",
    "MATH","MAT","PRGM","VARS","STAT","X,T","ON",
    "1/X","SIN","COS","TAN","^","X2","STO",
    "LN","LOG","7","8","9","/","(",
    "4","5","6","*","1","2","3",
    "-","0",".","(-)","+",")","ENTER"
};
void ti83_input_reset(ti83_input_t *input, int keyboard)
{
    if (!input) return;
    memset(input,0,sizeof(*input)); input->keyboard = keyboard != 0; input->cursor = 48;
}
void ti83_input_update(TilemCalc *calc, ti83_input_t *input, unsigned buttons)
{
    if (!calc || !input) return;
    unsigned pressed = buttons & ~input->previous;
    uint8_t next[57] = {0};
    if (pressed & 64u) {
        input->keyboard ^= 1u; input->second = input->alpha = 0;
    }
    if (input->keyboard) {
        unsigned row = input->cursor/7, col = input->cursor%7;
        if (pressed&1u) col=(col+6)%7;
        if (pressed&2u) col=(col+1)%7;
        if (pressed&4u) row=(row+6)%7;
        if (pressed&8u) row=(row+1)%7;
        input->cursor=row*7+col;
        unsigned key=ti83_keycodes[input->cursor];
        if ((pressed&16u) && key==TILEM_KEY_2ND) input->second ^= 1u;
        if ((pressed&16u) && key==TILEM_KEY_ALPHA) input->alpha ^= 1u;
        if ((buttons&16u) && key!=TILEM_KEY_2ND && key!=TILEM_KEY_ALPHA) next[key]=1;
        next[TILEM_KEY_2ND]=input->second || (buttons&32u);
        next[TILEM_KEY_ALPHA]=input->alpha;
    } else {
        if (buttons&1u) next[TILEM_KEY_LEFT]=1;
        if (buttons&2u) next[TILEM_KEY_RIGHT]=1;
        if (buttons&4u) next[TILEM_KEY_UP]=1;
        if (buttons&8u) next[TILEM_KEY_DOWN]=1;
        if (buttons&16u) next[input->game?TILEM_KEY_2ND:TILEM_KEY_ENTER]=1;
        if (buttons&32u) next[input->game?TILEM_KEY_ALPHA:TILEM_KEY_CLEAR]=1;
    }
    for (unsigned key=1;key<=0x38;++key) if (next[key]!=input->down[key]) {
        if(next[key]) tilem_keypad_press_key(calc,(int)key);
        else tilem_keypad_release_key(calc,(int)key);
    }
    memcpy(input->down,next,sizeof(next)); input->previous=buttons;
}
int ti83_blit(TilemCalc *calc, uint16_t *pixels, unsigned stride,
              unsigned width, unsigned height, int keyboard, int scanlines)
{
    if (!calc || !pixels || width != 320 || height != 240 || stride < width) return 0;
    byte packed[12*64];
    calc->hw.get_lcd(calc,packed);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) pixels[y*stride+x] = 0x2104;
    unsigned scale = keyboard ? 2u : 3u;
    unsigned ox = (320-96*scale)/2, oy = keyboard ? 4u : 16u;
    int active = calc->lcd.active && (!calc->z80.halted || calc->poweronhalt);
    for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 96; ++x) {
        uint16_t color = active && (packed[y*12+x/8] & (0x80u >> (x%8))) ? 0x18c3 : 0xbe77;
        for (unsigned dy = 0; dy < scale; ++dy) for (unsigned dx = 0; dx < scale; ++dx)
            pixels[(oy+y*scale+dy)*stride+ox+x*scale+dx] =
                scanlines && dy == scale-1 ? (uint16_t)((color & 0xf7deu)>>1) : color;
    }
    return 1;
}
