/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stddef.h>
#include <stdio.h>
#include "tilem.h"
static void z80_execute(TilemCalc *)
    __attribute__((section(".time_critical.ti83_execute"),optimize("O2")));
static dword z80_main_split(TilemCalc *,byte,int *) __attribute__((noinline));
static inline dword z80_execute_hot(TilemCalc *,byte) __attribute__((always_inline));
static dword z80_execute_opcode(TilemCalc *,byte)
    __attribute__((section(".time_critical.ti83_dispatch")));
static int z80_hot_prefix(TilemCalc *,byte,int,dword *)
    __attribute__((section(".time_critical.ti83_prefix"),noinline));
#define CPU_HOT __attribute__((section(".time_critical.ti83_helpers")))
static inline byte z80_readb_m1(TilemCalc *,dword) CPU_HOT __attribute__((always_inline));
static inline byte z80_readb(TilemCalc *,dword) CPU_HOT __attribute__((always_inline));
static inline byte z80_readpc(TilemCalc *,dword) CPU_HOT __attribute__((always_inline));
static inline dword z80_readw(TilemCalc *,dword) CPU_HOT __attribute__((optimize("O2")));
static inline void z80_writeb(TilemCalc *,dword,byte) CPU_HOT __attribute__((always_inline));
static inline void z80_writew(TilemCalc *,dword,word) CPU_HOT __attribute__((optimize("O2")));
static inline byte z80_input(TilemCalc *,dword) CPU_HOT;
static inline void z80_output(TilemCalc *,dword,byte) CPU_HOT;
static inline void check_timers(TilemCalc *) CPU_HOT __attribute__((always_inline));
static void check_timers_due(TilemCalc *) CPU_HOT __attribute__((noinline));
static inline void check_breakpoints(TilemCalc *,int,dword) CPU_HOT;
static inline void check_mem_breakpoints(TilemCalc *,int,int,dword) CPU_HOT __attribute__((always_inline));
