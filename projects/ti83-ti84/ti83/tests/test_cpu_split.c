/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "adapter.h"
#include "cpu_profile.h"
#include "fast_memory.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
extern dword ti83_reference_tilem_z80_run(TilemCalc *,int,int *);
static unsigned rng=0x9123;
static byte random_byte(void){rng=rng*1664525u+1013904223u;return (byte)(rng>>24);}
typedef struct { unsigned used; dword events[128]; } TimerTrace;
typedef struct { TimerTrace *trace; unsigned marker; } TimerContext;
static void timer_trace(TilemCalc *c,void *context)
{
    TimerContext *p=context;
    assert(p->trace->used+2<=128);
    p->trace->events[p->trace->used++]=p->marker;
    p->trace->events[p->trace->used++]=c->z80.clock;
}
int main(void)
{
    const byte prefixes[][4]={{0},{0xcb},{0xed},{0xdd},{0xfd},{0xdd,0xcb},{0xfd,0xcb},{0xdd,0xfd,0xdd,0xed}};
    const unsigned lengths[]={0,1,1,1,1,2,2,4};
    TilemCalc *base=ti83_create('3');assert(base);
    ti83_profile_start();
    for(unsigned i=0;i<base->hw.romsize+base->hw.ramsize;++i)base->mem[i]=random_byte();
    unsigned cases=0;
    for(unsigned prefix=0;prefix<8;++prefix)for(unsigned op=0;op<256;++op)for(unsigned trial=0;trial<4;++trial){
        base->z80.r.pc.d=0x100;base->z80.r.sp.d=0xc100;
        base->z80.r.af.d=random_byte()|((unsigned)random_byte()<<8);
        base->z80.r.bc.d=0x8000+random_byte();base->z80.r.de.d=0x8100+random_byte();base->z80.r.hl.d=0x8200+random_byte();
        base->z80.r.ix.d=0x8300;base->z80.r.iy.d=0x8400;
        base->z80.interrupts=0;base->z80.r.iff1=base->z80.r.iff2=0;
        byte *p=base->mem+0x100;memcpy(p,prefixes[prefix],lengths[prefix]);p+=lengths[prefix];
        if(prefix==5 || prefix==6)*p++=random_byte();
        *p++=(byte)op;for(unsigned i=0;i<8;++i)*p++=random_byte();
        TilemCalc *ref=tilem_calc_copy(base),*fast=tilem_calc_copy(base);assert(ref && fast);
        srand(cases);dword reason=ti83_reference_tilem_z80_run(ref,1,NULL);
        srand(cases);assert(reason==tilem_z80_run(fast,1,NULL));
        assert(!memcmp(&ref->z80.r,&fast->z80.r,sizeof(ref->z80.r)));
        assert(ref->z80.clock==fast->z80.clock && ref->z80.interrupts==fast->z80.interrupts);
        assert(ref->z80.halted==fast->z80.halted && ref->z80.exception==fast->z80.exception);
        assert(!memcmp(ref->mem,fast->mem,ref->hw.romsize+ref->hw.ramsize+ref->hw.lcdmemsize));
        assert(!memcmp(ref->hwregs,fast->hwregs,ref->hw.nhwregs*sizeof(dword)));
        tilem_calc_free(ref);tilem_calc_free(fast);++cases;
    }
    tilem_calc_free(base);
#if TI83_TIMING_PROFILE
    assert(ti83_cpu_timing.samples>0);
#endif
    ti83_profile_stop();
    const int types[]={TILEM_BREAK_EXECUTE,TILEM_BREAK_MEM_READ,TILEM_BREAK_MEM_WRITE,TILEM_BREAK_MEM_EXEC,TILEM_BREAK_PORT_READ,TILEM_BREAK_PORT_WRITE};
    for(unsigned i=0;i<6;++i){
        base=ti83_create('3');assert(base);
        const byte code[]={0x3a,0x00,0xc0,0x32,0x01,0xc0,0xdb,0x01,0xd3,0x01,0x00};
        memcpy(base->mem+0x100,code,sizeof(code));
        base->z80.r.pc.d=0x100;base->z80.r.iff1=base->z80.r.iff2=0;
        base->z80.stop_mask|=TILEM_STOP_BREAKPOINT;
        int bp=tilem_z80_add_breakpoint(base,types[i],0,0xffff,0xffff,NULL,NULL);assert(bp>0);
        TilemCalc *ref=tilem_calc_copy(base),*fast=tilem_calc_copy(base);assert(ref && fast);
        assert(ti83_reference_tilem_z80_run(ref,100,NULL)==tilem_z80_run(fast,100,NULL));
        assert(ref->z80.stop_breakpoint==fast->z80.stop_breakpoint && fast->z80.stop_breakpoint==bp);
        assert(!memcmp(&ref->z80.r,&fast->z80.r,sizeof(ref->z80.r)));
        assert(ref->z80.clock==fast->z80.clock);
        assert(!memcmp(ref->mem,fast->mem,ref->hw.romsize+ref->hw.ramsize+ref->hw.lcdmemsize));
        tilem_calc_free(ref);tilem_calc_free(fast);tilem_calc_free(base);
    }
    const dword starts[]={0,0xfffffff0u};
    const dword deadlines[]={1,7,10000,20000};
    for(unsigned s=0;s<2;++s)for(unsigned d=0;d<4;++d){
        TilemCalc *ref=ti83_create('3');assert(ref);
        memset(ref->mem,0,ref->hw.romsize);
        ref->z80.r.pc.d=0x100;ref->z80.r.iff1=ref->z80.r.iff2=0;ref->z80.clock=starts[s];
        TilemCalc *fast=tilem_calc_copy(ref);assert(fast);
        TimerTrace traces[2]={{0}};
        TimerContext contexts[2][2]={{{&traces[0],1},{&traces[0],2}},{{&traces[1],1},{&traces[1],2}}};
        for(unsigned j=0;j<2;++j){
            TilemCalc *c=j?fast:ref;
            assert(tilem_z80_add_timer(c,deadlines[d],17,0,timer_trace,&contexts[j][0])>0);
            assert(tilem_z80_add_timer(c,deadlines[d],19,1,timer_trace,&contexts[j][1])>0);
        }
        assert(ti83_reference_tilem_z80_run(ref,256,NULL)==tilem_z80_run(fast,256,NULL));
        assert(!memcmp(&traces[0],&traces[1],sizeof(TimerTrace)));
        if(d<2)assert(traces[0].used>0);else assert(traces[0].used==0);
        assert(ref->z80.clock==fast->z80.clock && ref->z80.interrupts==fast->z80.interrupts);
        assert(!memcmp(&ref->z80.r,&fast->z80.r,sizeof(ref->z80.r)));
        tilem_calc_free(ref);tilem_calc_free(fast);
    }
    for(unsigned i=0;i<6;++i){
        TilemCalc *ref=ti83_create('4');assert(ref);
        const byte code[]={0x2a,0x00,0xc0,0x22,0x02,0xc0,0x00};
        const byte immediate[]={0x3e,0x5a,0x00};
        if(i>=4)memcpy(ref->mem+0x100,immediate,sizeof(immediate));
        else memcpy(ref->mem+0x100,code,sizeof(code));
        ref->z80.r.pc.d=0x100;ref->z80.r.iff1=ref->z80.r.iff2=0;
        ref->mempagemap[0]=0;ref->mempagemap[3]=0x40;
        ref->ram[0]=0x78;ref->ram[1]=0x56;
        ref->z80.stop_mask|=TILEM_STOP_BREAKPOINT;
        unsigned address=i>=4?0x101:(i&1)?0xc002:0xc000;
        int type=i>=4?TILEM_BREAK_MEM_READ:(i&1)?TILEM_BREAK_MEM_WRITE:TILEM_BREAK_MEM_READ;
        if(i>=4?(i&1):(i&2)){address=ref->hw.mem_ltop(ref,address);type|=TILEM_BREAK_PHYSICAL;}
        int bp=tilem_z80_add_breakpoint(ref,type,address,address,0xffffffff,NULL,NULL);assert(bp>0);
        TilemCalc *fast=tilem_calc_copy(ref);assert(fast && ti83_fast_attach(fast));
        assert(ti83_reference_tilem_z80_run(ref,100,NULL)==tilem_z80_run(fast,100,NULL));
        assert(ref->z80.stop_breakpoint==bp && fast->z80.stop_breakpoint==bp);
        assert(!memcmp(&ref->z80.r,&fast->z80.r,sizeof(ref->z80.r)));
        assert(ref->z80.clock==fast->z80.clock && ref->z80.lastwrite==fast->z80.lastwrite);
        ti83_fast_detach(fast);
        assert(!memcmp(ref->mem,fast->mem,ref->hw.romsize+ref->hw.ramsize+ref->hw.lcdmemsize));
        tilem_calc_free(ref);tilem_calc_free(fast);
    }
    printf("Split CPU: %u opcode/prefix/flag cases, 6 breakpoint paths, 8 timer traces and 6 TI84 logical/physical word/operand breakpoint cases equivalent PASS\n",cases);
    return 0;
}
