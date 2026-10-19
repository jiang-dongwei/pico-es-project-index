/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fast_memory.h"
#include "x4/x4.h"
#include <assert.h>
#include <string.h>
static unsigned rng=0x1234;
static unsigned random_word(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void)
{
    TilemCalc *ref=ti83_create('4');assert(ref);
    for(unsigned i=0;i<ref->hw.romsize+ref->hw.ramsize;++i)ref->mem[i]=(byte)random_word();
    TilemCalc *fast=tilem_calc_copy(ref);assert(fast);assert(ti83_fast_attach(fast));
    assert(!ti83_fast_attach(ref));
    for(unsigned i=0;i<50000;++i){
        for(unsigned bank=0;bank<4;++bank){
            byte p=(byte)(random_word()%72);ref->mempagemap[bank]=fast->mempagemap[bank]=p;
        }
        unsigned regs[]={PORT27,PORT28,RAM_READ_DELAY,RAM_WRITE_DELAY,RAM_EXEC_DELAY,FLASH_READ_DELAY,FLASH_EXEC_DELAY,FLASH_WRITE_DELAY,PROTECTSTATE};
        for(unsigned j=0;j<sizeof(regs)/sizeof(regs[0]);++j){
            unsigned v=random_word()%8;ref->hwregs[regs[j]]=fast->hwregs[regs[j]]=v;
        }
        unsigned mask=random_word()&0x1ffff,lower=random_word()&0xffff,upper=lower+(random_word()&0xffff);
        ref->hwregs[NO_EXEC_RAM_MASK]=fast->hwregs[NO_EXEC_RAM_MASK]=mask;
        ref->hwregs[NO_EXEC_RAM_LOWER]=fast->hwregs[NO_EXEC_RAM_LOWER]=lower;
        ref->hwregs[NO_EXEC_RAM_UPPER]=fast->hwregs[NO_EXEC_RAM_UPPER]=upper;
        ref->flash.unlock=fast->flash.unlock=random_word()&1;
        unsigned a=random_word()&65535;byte value=(byte)random_word();
        ref->z80.clock=fast->z80.clock=0;ref->z80.exception=fast->z80.exception=0;
        assert(ref->hw.z80_rdmem(ref,a)==ti83_fast_read_operand(fast,a));
        assert(ref->hw.z80_rdmem_m1(ref,a)==ti83_fast_fetch_opcode(fast,a));
        ref->hw.z80_wrmem(ref,a,value);fast->hw.z80_wrmem(fast,a,value);
        assert(ref->z80.clock==fast->z80.clock);
        assert(ref->z80.exception==fast->z80.exception);
        assert(!memcmp(ref->hwregs,fast->hwregs,ref->hw.nhwregs*sizeof(dword)));
        ref->z80.clock=fast->z80.clock=0;
        dword expected=ref->hw.z80_rdmem(ref,a);
        expected|=(dword)ref->hw.z80_rdmem(ref,(a+1)&65535)<<8;
        dword actual;
        if(!ti83_fast_read_word(fast,a,&actual)){
            actual=fast->hw.z80_rdmem(fast,a);
            actual|=(dword)fast->hw.z80_rdmem(fast,(a+1)&65535)<<8;
        }
        assert(actual==expected && ref->z80.clock==fast->z80.clock);
        assert(!memcmp(ref->hwregs,fast->hwregs,ref->hw.nhwregs*sizeof(dword)));
        if((i&7)==0){
            word pair=(word)random_word();
            ref->hw.z80_wrmem(ref,a,(byte)pair);ref->hw.z80_wrmem(ref,(a+1)&65535,(byte)(pair>>8));
            if(!ti83_fast_write_word(fast,a,pair)){
                fast->hw.z80_wrmem(fast,a,(byte)pair);fast->hw.z80_wrmem(fast,(a+1)&65535,(byte)(pair>>8));
            }
            assert(ref->z80.clock==fast->z80.clock);
        }
    }
    const unsigned overlays[]={0,1,7,255};
    for(unsigned t=0;t<4;++t){
        ref->hwregs[PORT27]=fast->hwregs[PORT27]=overlays[t];
        ref->hwregs[PORT28]=fast->hwregs[PORT28]=overlays[3-t];
        for(unsigned bank=0;bank<4;++bank)ref->mempagemap[bank]=fast->mempagemap[bank]=(bank&1)?0x40:0x41;
        for(unsigned base=0;base<65536;base+=64)for(unsigned tail=61;tail<=63;++tail){
            unsigned a=base+tail;
            ref->z80.clock=fast->z80.clock=0;
            dword expected=ref->hw.z80_rdmem(ref,a);
            expected|=(dword)ref->hw.z80_rdmem(ref,(a+1)&65535)<<8;
            dword actual;
            if(!ti83_fast_read_word(fast,a,&actual)){
                actual=fast->hw.z80_rdmem(fast,a);
                actual|=(dword)fast->hw.z80_rdmem(fast,(a+1)&65535)<<8;
            }
            assert(actual==expected && ref->z80.clock==fast->z80.clock);
            assert(!memcmp(ref->hwregs,fast->hwregs,ref->hw.nhwregs*sizeof(dword)));
            if(ti83_fast_write_word(fast,a,(word)a)){
                ref->hw.z80_wrmem(ref,a,(byte)a);ref->hw.z80_wrmem(ref,(a+1)&65535,(byte)(a>>8));
                assert(ref->z80.clock==fast->z80.clock);
            }
        }
    }
    ref->mempagemap[0]=fast->mempagemap[0]=0;
    for(unsigned protected=0;protected<2;++protected){
        ref->hwregs[PORT22]=fast->hwregs[PORT22]=protected?0:1;
        ref->hwregs[PORT23]=fast->hwregs[PORT23]=63;
        for(unsigned a=0;a<0x4000;a+=31){
            ref->z80.clock=fast->z80.clock=0;ref->z80.exception=fast->z80.exception=0;
            assert(ref->hw.z80_rdmem(ref,a)==ti83_fast_read_operand(fast,a));
            assert(ref->hw.z80_rdmem_m1(ref,a)==ti83_fast_fetch_opcode(fast,a));
            assert(ref->z80.clock==fast->z80.clock && ref->z80.exception==fast->z80.exception);
            assert(!memcmp(ref->hwregs,fast->hwregs,ref->hw.nhwregs*sizeof(dword)));
        }
    }
    ref->flash.unlock=fast->flash.unlock=1;
    ref->flash.overridegroup=fast->flash.overridegroup=255;
    ref->flash.busy=fast->flash.busy=0;
    ref->flash.state=fast->flash.state=0;
    ref->flash.emuflags &= ~TILEM_FLASH_REQUIRE_DELAY;
    fast->flash.emuflags &= ~TILEM_FLASH_REQUIRE_DELAY;
    const unsigned addresses[]={0xaaa,0x555,0xaaa,1,0xaaa,0x555,0xaaa,0xaaa,0x555,1};
    const byte values[]={0xaa,0x55,0xa0,0,0xaa,0x55,0x80,0xaa,0x55,0x30};
    assert(ref->hw.z80_rdmem(ref,1)==fast->hw.z80_rdmem(fast,1));
    for(unsigned i=0;i<sizeof(values);++i){
        ref->hw.z80_wrmem(ref,addresses[i],values[i]);fast->hw.z80_wrmem(fast,addresses[i],values[i]);
        if(i==3 || i==9){
            assert(ref->mem[1]==(i==3?0:255));
            for(unsigned a=0;a<16384;a+=31){
                assert(ref->hw.z80_rdmem(ref,a)==ti83_fast_read_operand(fast,a));
                assert(ref->hw.z80_rdmem_m1(ref,a)==ti83_fast_fetch_opcode(fast,a));
                assert(ref->z80.clock==fast->z80.clock && ref->z80.exception==fast->z80.exception);
            }
        }
        assert(ref->flash.state==fast->flash.state && ref->flash.busy==fast->flash.busy);
    }
    ti83_fast_flush(fast);assert(!memcmp(ref->mem,fast->mem,ref->hw.romsize+ref->hw.ramsize));
    ti83_fast_detach(fast);assert(ti83_fast_attach(ref));ti83_fast_detach(ref);
    tilem_calc_free(ref);tilem_calc_free(fast);
    puts("TI84 cached bus: 50000 randomized byte/word bank/protection/clock operations and Flash program/erase invalidation equivalent PASS");
    return 0;
}
