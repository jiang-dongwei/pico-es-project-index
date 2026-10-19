/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TI83_CPU_TIMING_PROFILE_H
#define TI83_CPU_TIMING_PROFILE_H
#include <stdint.h>
#ifndef TI83_TIMING_PROFILE
#define TI83_TIMING_PROFILE 0
#endif
#if TI83_TIMING_PROFILE
#ifdef SDLPoP_PLATFORM_PICO
#include "hardware/timer.h"
#define PROFILE_INLINE static inline __attribute__((always_inline))
PROFILE_INLINE uint32_t ti83_profile_tick(void) { return time_us_32(); }
#else
#include <time.h>
#define PROFILE_INLINE static inline
PROFILE_INLINE uint32_t ti83_profile_tick(void) { return (uint32_t)((uint64_t)clock()*1000000u/CLOCKS_PER_SEC); }
#endif
#else
#define PROFILE_INLINE static inline
#endif
typedef struct {
    unsigned enabled,active,serial,samples,category;
    uint32_t last;
    uint64_t fetch,dispatch[5],timers,tail;
    unsigned counts[5];
} ti83_cpu_timing_t;
extern ti83_cpu_timing_t ti83_cpu_timing;
void ti83_profile_start(void);
void ti83_profile_report(const char *stage);
void ti83_profile_stop(void);
#if TI83_TIMING_PROFILE
PROFILE_INLINE void ti83_profile_begin(void)
{
    ti83_cpu_timing_t *p=&ti83_cpu_timing;
    p->active=p->enabled && (++p->serial&1023u)==0;
    if(p->active){++p->samples;p->last=ti83_profile_tick();}
}
PROFILE_INLINE void ti83_profile_fetch(unsigned op)
{
    ti83_cpu_timing_t *p=&ti83_cpu_timing;
    if(!p->active)return;
    uint32_t now=ti83_profile_tick();p->fetch+=(uint32_t)(now-p->last);p->last=now;
    p->category=op==0xcb?1:op==0xed?2:op==0xdd?3:op==0xfd?4:0;
    ++p->counts[p->category];
}
PROFILE_INLINE void ti83_profile_dispatch(void)
{
    ti83_cpu_timing_t *p=&ti83_cpu_timing;
    if(!p->active)return;
    uint32_t now=ti83_profile_tick();p->dispatch[p->category]+=(uint32_t)(now-p->last);p->last=now;
}
PROFILE_INLINE void ti83_profile_timer(void)
{
    ti83_cpu_timing_t *p=&ti83_cpu_timing;
    if(!p->active)return;
    uint32_t now=ti83_profile_tick();p->timers+=(uint32_t)(now-p->last);p->last=now;
}
PROFILE_INLINE void ti83_profile_end(void)
{
    ti83_cpu_timing_t *p=&ti83_cpu_timing;
    if(p->active)p->tail+=(uint32_t)(ti83_profile_tick()-p->last);
    p->active=0;
}
#else
PROFILE_INLINE void ti83_profile_begin(void) {}
PROFILE_INLINE void ti83_profile_fetch(unsigned op) {(void)op;}
PROFILE_INLINE void ti83_profile_dispatch(void) {}
PROFILE_INLINE void ti83_profile_timer(void) {}
PROFILE_INLINE void ti83_profile_end(void) {}
#endif
#endif
