/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef STUDIO2_CDP1802_H
#define STUDIO2_CDP1802_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    void *context;
    uint8_t (*read)(void *, uint16_t);
    void (*write)(void *, uint16_t, uint8_t);
    uint8_t (*input)(void *, unsigned);
    void (*output)(void *, unsigned, uint8_t);
} cdp1802_bus_t;
typedef struct {
    uint16_t r[16];
    uint8_t d, df, p, x, t, q, ie, idle, phase, op, high;
    uint64_t instructions, cycles;
} cdp1802_t;
void cdp1802_reset(cdp1802_t *c);
void cdp1802_tick(cdp1802_t *c, const cdp1802_bus_t *b, uint8_t ef, int irq);
uint8_t cdp1802_dma_out(cdp1802_t *c, const cdp1802_bus_t *b);
void cdp1802_dma_in(cdp1802_t *c, const cdp1802_bus_t *b, uint8_t value);
#ifdef __cplusplus
}
#endif
#endif
