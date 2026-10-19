/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "studio2.h"
#include <string.h>
void studio2_reset(studio2_t *s) {
    cdp1802_reset(&s->cpu);
    memset(s->ram, 0, sizeof(s->ram));
    memset(s->video, 0, sizeof(s->video));
    s->display = 0;
    s->key_latch = 0;
    s->ticks = 0;
    s->frames = 0;
    s->dma_left = 0;
    s->tone_phase = 0;
    s->audio_fraction = 0;
    s->audio_count = 0;
    memset(s->keypad_ef_cycles, 0, sizeof(s->keypad_ef_cycles));
}
void studio2_init(studio2_t *s) {
    memset(s, 0, sizeof(*s));
    memset(s->rom, 0xff, sizeof(s->rom));
    studio2_reset(s);
}
int studio2_load_bios(studio2_t *s, const uint8_t *d, size_t n) {
    if (!s || !d || n != 2048)
        return -1;
    memcpy(s->rom, d, n);
    memset(s->mapped, 0, 16);
    memset(s->mapped, 1, 8);
    studio2_reset(s);
    return 0;
}
int studio2_load_cart(studio2_t *s, const uint8_t *d, size_t n, int st2) {
    unsigned i;
    uint8_t pages[16] = {0};
    if (!s || !d || !n)
        return -1;
    if (st2) {
        if (n < 512 || memcmp(d, "RCA2", 4) || d[4] < 2 || d[4] > 11 || n != (size_t)d[4] * 256)
            return -2;
        for (i = 0; i < (unsigned)d[4] - 1; i++) {
            unsigned page = d[64 + i];
            if (page < 4 || page > 15 || pages[page] || page == 8 || page == 9 || page == 10 ||
                page == 11)
                return -3;
            pages[page] = 1;
        }
    } else if (n > 1024)
        return -2;
    for (i = 4; i < 16; i++) {
        s->mapped[i] = 0;
        memset(s->rom + i * 256, 0xff, 256);
    }
    /* A plugged cartridge replaces the whole built-in 0400-07ff ROM window. */
    memset(s->mapped + 4, 1, 4);
    if (st2) {
        for (i = 0; i < (unsigned)d[4] - 1; i++) {
            unsigned page = d[64 + i];
            if (page >= 12)
                memset(s->mapped + 12, 1, 4);
            s->mapped[page] = 1;
            memcpy(s->rom + page * 256, d + (i + 1) * 256, 256);
        }
    } else
        memcpy(s->rom + 0x400, d, n);
    studio2_reset(s);
    return 0;
}
uint8_t studio2_read(void *o, uint16_t a) {
    studio2_t *s = o;
    if (a < 4096 && s->mapped[a >> 8])
        return s->rom[a];
    return (a & 0x200) ? 0xff : s->ram[a & 511];
}
void studio2_write(void *o, uint16_t a, uint8_t v) {
    studio2_t *s = o;
    if (a < 4096 && s->mapped[a >> 8])
        return;
    if (!(a & 0x200))
        s->ram[a & 511] = v;
}
static uint8_t input(void *o, unsigned port) {
    studio2_t *s = o;
    if (port == 1)
        s->display = 1;
    return 0xff;
}
static void output(void *o, unsigned port, uint8_t v) {
    studio2_t *s = o;
    if (port == 2)
        s->key_latch = v & 15;
}
void studio2_frame(studio2_t *s) {
    cdp1802_bus_t b = {s, studio2_read, studio2_write, input, output};
    unsigned i;
    s->audio_count = 0;
    for (i = 0; i < STUDIO2_FRAME_CYCLES; i++) {
        unsigned pos = (unsigned)(s->ticks % STUDIO2_FRAME_CYCLES), line = pos / 14, x = pos % 14;
        uint8_t ef = 0;
        if ((line >= 76 && line < 80) || (line >= 204 && line < 208))
            ef |= 1;
        if (s->key_latch < 10) {
            if (s->keys[0] & (1u << s->key_latch)) {
                ef |= 4;
                s->keypad_ef_cycles[0]++;
            }
            if (s->keys[1] & (1u << s->key_latch)) {
                ef |= 8;
                s->keypad_ef_cycles[1]++;
            }
        }
        if (s->display && line >= 80 && line < 208 && x == 2) {
            s->dma_left = 8;
            s->dma_column = 0;
            s->dma_line = (uint16_t)(line - 80);
        }
        if (s->dma_left && s->cpu.phase == 0) {
            s->video[s->dma_line][s->dma_column++] = cdp1802_dma_out(&s->cpu, &b);
            s->dma_left--;
        } else
            cdp1802_tick(&s->cpu, &b, ef, s->display && line >= 78 && line < 80);
        s->audio_fraction += STUDIO2_AUDIO_RATE;
        if (s->audio_fraction >= STUDIO2_MACHINE_HZ) {
            s->audio_fraction -= STUDIO2_MACHINE_HZ;
            s->tone_phase = (s->tone_phase + 300) % STUDIO2_AUDIO_RATE;
            if (s->audio_count < 400)
                s->audio[s->audio_count++] =
                    s->cpu.q ? (s->tone_phase < STUDIO2_AUDIO_RATE / 2 ? 3000 : -3000) : 0;
        }
        s->ticks++;
    }
    if (!s->display)
        memset(s->video, 0, sizeof(s->video));
    s->frames++;
}
int studio2_compose(const studio2_t *s, uint16_t *p, unsigned stride, int scanlines) {
    unsigned y;
    if (!s || !p || stride < 320)
        return -1;
    for (y = 0; y < 24; y++) {
        memset(p + y * stride, 0, 320 * sizeof(*p));
        memset(p + (216 + y) * stride, 0, 320 * sizeof(*p));
    }
    for (y = 0; y < 192; y++) {
        const uint8_t *src = s->video[y * 2 / 3];
        uint16_t *dst = p + (24 + y) * stride;
        uint16_t white = scanlines && (y & 1) ? 0x8410 : 0xffff;
        for (unsigned byte = 0; byte < 8; byte++) {
            unsigned packed = src[byte];
            for (unsigned bit = 0; bit < 8; bit++) {
                uint16_t color = packed & (128u >> bit) ? white : 0;
                dst[0] = dst[1] = dst[2] = dst[3] = dst[4] = color;
                dst += 5;
            }
        }
    }
    return 0;
}
uint16_t studio2_keypad(unsigned m, unsigned bank) {
    static const uint8_t map[2][7] = {{4, 6, 2, 8, 0, 5, 1}, {7, 9, 3, 0, 4, 6, 5}};
    unsigned k;
    uint16_t result = 0;
    bank = bank ? 1 : 0;
    for (k = 0; k < 7; k++)
        if (m & (1u << k))
            result |= (uint16_t)(1u << map[bank][k]);
    return result;
}
