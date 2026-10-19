/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RCA_STUDIO2_H
#define RCA_STUDIO2_H
#include "cdp1802.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { STUDIO2_MACHINE_HZ = 220000, STUDIO2_FRAME_CYCLES = 3668, STUDIO2_AUDIO_RATE = 22050 };
typedef struct {
    cdp1802_t cpu;
    uint8_t rom[4096], mapped[16], ram[512], video[128][8];
    uint16_t keys[2];
    uint8_t key_latch, display, dma_left, dma_column;
    uint16_t dma_line;
    uint64_t ticks;
    uint32_t frames, audio_fraction, tone_phase;
    uint32_t keypad_ef_cycles[2];
    int16_t audio[400];
    size_t audio_count;
} studio2_t;
void studio2_init(studio2_t *s);
void studio2_reset(studio2_t *s);
int studio2_load_bios(studio2_t *s, const uint8_t *data, size_t size);
int studio2_load_cart(studio2_t *s, const uint8_t *data, size_t size, int st2);
uint8_t studio2_read(void *s, uint16_t address);
void studio2_write(void *s, uint16_t address, uint8_t value);
void studio2_frame(studio2_t *s);
int studio2_compose(const studio2_t *s, uint16_t *pixels, unsigned stride, int scanlines);
uint16_t studio2_keypad(unsigned mask, unsigned bank);
#ifdef __cplusplus
}
#endif
#endif
