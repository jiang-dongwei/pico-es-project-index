/* SPDX-License-Identifier: LGPL-2.1-or-later
 * TI84 memory semantics derived from TilEm x4_memory.c:
 * Copyright (C) 2001 Solignac Julien; 2004-2011 Benjamin Moody.
 */
#include "fast_memory.h"
#include "x4/x4.h"
#include <string.h>
#ifdef SDLPoP_PLATFORM_PICO
#define HOT __attribute__((section(".time_critical.ti83_memory")))
#else
#define HOT
#endif
uint8_t ti83_fast_ram[32768];
static uint8_t rom_lines[64][128];
static unsigned rom_tags[64];
ti83_fetch_cache_t ti83_fetch_cache;
#define rom0 ti83_fetch_cache.rom
#define rom0_valid ti83_fetch_cache.valid
#define owner ti83_fetch_cache.owner
#ifdef TI83_CPU_PROFILE
static unsigned rom_reads[64],rom_misses[64];
void ti83_fast_count_rom0(void) {++rom_reads[0];}
void ti83_memory_profile(void)
{
    for(unsigned page=0;page<64;++page)if(rom_reads[page])
        printf("PROFILE ROM page=%02X reads=%u misses=%u\n",page,rom_reads[page],rom_misses[page]);
    memset(rom_reads,0,sizeof(rom_reads));memset(rom_misses,0,sizeof(rom_misses));
}
#endif
static byte (*original_read)(TilemCalc *,dword);
static byte (*original_fetch)(TilemCalc *,dword);
static void (*original_write)(TilemCalc *,dword,byte);
static inline unsigned physical(TilemCalc *c,dword a)
{
    unsigned page=c->mempagemap[a>>14];
    if(a&0x8000) {
        if(a>0xffff-64*c->hwregs[PORT27])page=0x40;
        else if(a<0x8000+64*c->hwregs[PORT28])page=0x41;
    }
    return (page<<14)|(a&0x3fff);
}
static HOT byte read_rom(TilemCalc *c,unsigned p)
{
#ifdef TI83_CPU_PROFILE
    ++rom_reads[p>>14];
#endif
    if(p<sizeof(rom0)) {
        if(!rom0_valid) {memcpy(rom0,c->mem,sizeof(rom0));rom0_valid=1;}
        return rom0[p];
    }
    unsigned line=p>>7,slot=(line^(line>>7))&63u;
    if(rom_tags[slot]!=line) {
#ifdef TI83_CPU_PROFILE
        ++rom_misses[p>>14];
#endif
        memcpy(rom_lines[slot],c->mem+(p&~127u),128);
        rom_tags[slot]=line;
    }
    return rom_lines[slot][p&127u];
}
static void invalidate_rom(void)
{
    rom0_valid=0;memset(rom_tags,0xff,sizeof(rom_tags));
}
static HOT byte read_value(TilemCalc *c,unsigned p)
{
    byte value;
    if(p<0x100000) {
        value=c->flash.state || c->flash.busy ? tilem_flash_read_byte(c,p) : read_rom(c,p);
    } else value=p<0x108000?ti83_fast_ram[p-0x100000]:c->mem[p];
    if(p<0xb0000 || p>=0x100000 || (p>=0xc0000 && p<0xf0000)) c->hwregs[PROTECTSTATE]=0;
    else {
        static const byte sequence[]={0,0,0xed,0x56,0xf3,0xd3};
        unsigned state=c->hwregs[PROTECTSTATE];
        c->hwregs[PROTECTSTATE]=state==6?7:state<6 && value==sequence[state]?state+1:0;
    }
    return value;
}
static HOT byte read_ram(TilemCalc *c,dword a)
{
    if(a<0x4000 && c->mempagemap[0]==0 && rom0_valid && !c->flash.state && !c->flash.busy) {
#ifdef TI83_CPU_PROFILE
        ++rom_reads[0];
#endif
        c->z80.clock+=c->hwregs[FLASH_READ_DELAY];c->hwregs[PROTECTSTATE]=0;
        return rom0[a];
    }
    unsigned p=physical(c,a);
    if(p>=0x100000 && p<0x108000){
        c->z80.clock+=c->hwregs[RAM_READ_DELAY];c->hwregs[PROTECTSTATE]=0;
        return ti83_fast_ram[p-0x100000];
    }
    if((p>>14)==0x3e && !c->flash.unlock)return 0xff;
    c->z80.clock+=c->hwregs[p<0x100000?FLASH_READ_DELAY:RAM_READ_DELAY];
    return read_value(c,p);
}
static HOT byte fetch_ram(TilemCalc *c,dword a)
{
    if(a<0x4000 && c->mempagemap[0]==0 && rom0_valid && !c->flash.state && !c->flash.busy) {
#ifdef TI83_CPU_PROFILE
        ++rom_reads[0];
#endif
        c->z80.clock+=c->hwregs[FLASH_EXEC_DELAY];
        if(c->hwregs[PORT22]==0)tilem_z80_exception(c,TILEM_EXC_FLASH_EXEC);
        c->hwregs[PROTECTSTATE]=0;
        byte value=rom0[a];
        if(value==0xff && a==0x0038)tilem_z80_exception(c,TILEM_EXC_FLASH_EXEC);
        return value;
    }
    unsigned p=physical(c,a);
    if((p>>14)==0x3e && !c->flash.unlock)return 0xff;
    if(p<0x100000) {
        c->z80.clock+=c->hwregs[FLASH_EXEC_DELAY];
        unsigned page=p>>14;
        if(page>=c->hwregs[PORT22] && page<=c->hwregs[PORT23])tilem_z80_exception(c,TILEM_EXC_FLASH_EXEC);
    } else {
        c->z80.clock+=c->hwregs[RAM_EXEC_DELAY];
        unsigned m=p&c->hwregs[NO_EXEC_RAM_MASK];
        if(m<c->hwregs[NO_EXEC_RAM_LOWER] || m>c->hwregs[NO_EXEC_RAM_UPPER])tilem_z80_exception(c,TILEM_EXC_RAM_EXEC);
    }
    byte value=read_value(c,p);
    if(value==0xff && a==0x0038)tilem_z80_exception(c,TILEM_EXC_FLASH_EXEC);
    return value;
}
static HOT void write_ram(TilemCalc *c,dword a,byte value)
{
    unsigned p=physical(c,a);
    if(p<0x100000) {invalidate_rom();original_write(c,a,value);return;}
    if(p>=0x108000){original_write(c,a,value);return;}
    c->z80.clock+=c->hwregs[RAM_WRITE_DELAY];ti83_fast_ram[p-0x100000]=value;
}
#ifndef SDLPoP_PLATFORM_PICO
int ti83_fast_read_word(TilemCalc *c,dword a,dword *value)
{
    return ti83_inline_read_word(c,a,value);
}
int HOT ti83_fast_write_word(TilemCalc *c,dword a,word value)
{
    return ti83_inline_write_word(c,a,value);
}
#endif
int ti83_fast_attach(TilemCalc *c)
{
    if(!c || c->hw.model_id!='4' || owner)return 0;
    memcpy(ti83_fast_ram,c->ram,sizeof(ti83_fast_ram));invalidate_rom();owner=c;
    original_read=c->hw.z80_rdmem;original_fetch=c->hw.z80_rdmem_m1;original_write=c->hw.z80_wrmem;
    c->hw.z80_rdmem=read_ram;c->hw.z80_rdmem_m1=fetch_ram;c->hw.z80_wrmem=write_ram;
    return 1;
}
void ti83_fast_flush(TilemCalc *c)
{
    if(c && c==owner)memcpy(c->ram,ti83_fast_ram,sizeof(ti83_fast_ram));
}
void ti83_fast_detach(TilemCalc *c)
{
    if(!c || c!=owner)return;
    ti83_fast_flush(c);
    c->hw.z80_rdmem=original_read;c->hw.z80_rdmem_m1=original_fetch;c->hw.z80_wrmem=original_write;
    owner=NULL;
}
