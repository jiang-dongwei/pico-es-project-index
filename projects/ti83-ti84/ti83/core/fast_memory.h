/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TI83_FAST_MEMORY_H
#define TI83_FAST_MEMORY_H
#include "adapter.h"
#include "x4/x4.h"
typedef struct {
    TilemCalc *owner;
    int valid;
    uint8_t rom[16384];
} ti83_fetch_cache_t;
extern ti83_fetch_cache_t ti83_fetch_cache;
#ifdef TI83_CPU_PROFILE
void ti83_fast_count_rom0(void);
#endif
static inline __attribute__((always_inline)) byte ti83_fast_fetch_opcode(TilemCalc *c,dword a)
{
    if(c==ti83_fetch_cache.owner && a<0x4000 && c->mempagemap[0]==0 && ti83_fetch_cache.valid && !c->flash.state && !c->flash.busy){
#ifdef TI83_CPU_PROFILE
        ti83_fast_count_rom0();
#endif
        c->z80.clock+=c->hwregs[FLASH_EXEC_DELAY];
        if(c->hwregs[PORT22]==0)tilem_z80_exception(c,TILEM_EXC_FLASH_EXEC);
        c->hwregs[PROTECTSTATE]=0;
        byte value=ti83_fetch_cache.rom[a];
        if(value==0xff && a==0x0038)tilem_z80_exception(c,TILEM_EXC_FLASH_EXEC);
        return value;
    }
    return (*c->hw.z80_rdmem_m1)(c,a);
}
int ti83_fast_attach(TilemCalc *calc);
static inline __attribute__((always_inline)) byte ti83_fast_read_operand(TilemCalc *c,dword a)
{
    if(c==ti83_fetch_cache.owner && a<0x4000 && c->mempagemap[0]==0 && ti83_fetch_cache.valid && !c->flash.state && !c->flash.busy){
#ifdef TI83_CPU_PROFILE
        ti83_fast_count_rom0();
#endif
        c->z80.clock+=c->hwregs[FLASH_READ_DELAY];c->hwregs[PROTECTSTATE]=0;
        return ti83_fetch_cache.rom[a];
    }
    return (*c->hw.z80_rdmem)(c,a);
}
void ti83_fast_flush(TilemCalc *calc);
void ti83_fast_detach(TilemCalc *calc);
extern uint8_t ti83_fast_ram[32768];
static inline unsigned ti83_word_physical(TilemCalc *c,dword a)
{
    unsigned page=c->mempagemap[a>>14];
    if(a&0x8000) {
        if(a>0xffff-64*c->hwregs[PORT27])page=0x40;
        else if(a<0x8000+64*c->hwregs[PORT28])page=0x41;
    }
    return (page<<14)|(a&0x3fff);
}
static inline __attribute__((always_inline)) int ti83_inline_read_word(TilemCalc *c,dword a,dword *value)
{
    if(c!=ti83_fetch_cache.owner || c->z80.breakpoint_mr || c->z80.breakpoint_mpr)return 0;
    a&=0xffff;
    if(a<0x3fff && c->mempagemap[0]==0 && ti83_fetch_cache.valid && !c->flash.state && !c->flash.busy){
#ifdef TI83_CPU_PROFILE
        ti83_fast_count_rom0();ti83_fast_count_rom0();
#endif
        *value=ti83_fetch_cache.rom[a]|((dword)ti83_fetch_cache.rom[a+1]<<8);
        c->z80.clock+=2*c->hwregs[FLASH_READ_DELAY];c->hwregs[PROTECTSTATE]=0;
        return 1;
    }
    unsigned p=ti83_word_physical(c,a);
    if(p<0x100000 || p>=0x107fff || ((a&63)==63 && ti83_word_physical(c,(a+1)&0xffff)!=p+1))return 0;
    *value=ti83_fast_ram[p-0x100000]|((dword)ti83_fast_ram[p-0x100000+1]<<8);
    c->z80.clock+=2*c->hwregs[RAM_READ_DELAY];c->hwregs[PROTECTSTATE]=0;
    return 1;
}
static inline __attribute__((always_inline)) int ti83_inline_write_word(TilemCalc *c,dword a,word value)
{
    if(c!=ti83_fetch_cache.owner || c->z80.breakpoint_mw || c->z80.breakpoint_mpw)return 0;
    a&=0xffff;
    unsigned p=ti83_word_physical(c,a);
    if(p<0x100000 || p>=0x107fff || ((a&63)==63 && ti83_word_physical(c,(a+1)&0xffff)!=p+1))return 0;
    ti83_fast_ram[p-0x100000]=(byte)value;ti83_fast_ram[p-0x100000+1]=(byte)(value>>8);
    c->z80.clock+=2*c->hwregs[RAM_WRITE_DELAY];c->z80.lastwrite=c->z80.clock;
    return 1;
}

int ti83_fast_read_word(TilemCalc *calc,dword address,dword *value);
int ti83_fast_write_word(TilemCalc *calc,dword address,word value);
#endif
