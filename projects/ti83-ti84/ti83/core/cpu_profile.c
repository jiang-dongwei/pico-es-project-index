/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cpu_profile.h"
#include <stdio.h>
#include <string.h>
ti83_cpu_timing_t ti83_cpu_timing;
#if TI83_TIMING_PROFILE
void ti83_profile_start(void)
{
    memset(&ti83_cpu_timing,0,sizeof(ti83_cpu_timing));ti83_cpu_timing.enabled=1;
}
void ti83_profile_stop(void) {ti83_cpu_timing.enabled=ti83_cpu_timing.active=0;}
void ti83_profile_report(const char *stage)
{
    ti83_cpu_timing_t *p=&ti83_cpu_timing;
    printf("TI83 CPU stage=%s sample=1/1024 samples=%u unit=us fetch=%lu main=%lu cb=%lu ed=%lu dd=%lu fd=%lu timers=%lu irq_tail=%lu\n",
        stage,p->samples,(unsigned long)p->fetch,(unsigned long)p->dispatch[0],
        (unsigned long)p->dispatch[1],(unsigned long)p->dispatch[2],
        (unsigned long)p->dispatch[3],(unsigned long)p->dispatch[4],
        (unsigned long)p->timers,(unsigned long)p->tail);
    printf("TI83 CPU COUNT stage=%s main=%u cb=%u ed=%u dd=%u fd=%u\n",stage,
        p->counts[0],p->counts[1],p->counts[2],p->counts[3],p->counts[4]);
    ti83_profile_stop();
}
#else
void ti83_profile_start(void) {}
void ti83_profile_stop(void) {}
void ti83_profile_report(const char *stage) {(void)stage;}
#endif
