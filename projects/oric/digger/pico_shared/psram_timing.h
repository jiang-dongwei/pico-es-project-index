/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FRENS_PSRAM_TIMING_H
#define FRENS_PSRAM_TIMING_H
#include <stdint.h>

typedef struct {
    uint32_t divisor;
    uint32_t rxdelay;
    uint32_t max_select;
    uint32_t min_deselect;
} frens_psram_timing_t;

/* base_divisor describes the board's PSRAM clock at 252 MHz, not clk_sys. */
static inline frens_psram_timing_t frens_psram_timing_for_clock(
    uint32_t sys_hz, uint32_t base_divisor)
{
    const uint32_t target_hz = 252000000u / base_divisor;
    uint32_t divisor = (sys_hz + target_hz - 1u) / target_hz;
    if (divisor < 1u) divisor = 1u;
    const uint32_t tcph = (18u * (sys_hz / 1000000u) + 999u) / 1000u;
    const uint32_t halfdiv = (divisor + 1u) / 2u;
    frens_psram_timing_t timing = {
        divisor, divisor > 7u ? 7u : divisor, 31u,
        tcph > halfdiv ? tcph - halfdiv : 0u
    };
    return timing;
}
#endif
