/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "compat.h"
#include "diagnostic.h"
#include "menu_adapter.h"
#include "path_adapter.h"
#include "settings_adapter.h"
#include "studio2.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t memory[65536], ports[8];
static void check_compose(studio2_t *s, uint16_t *actual) {
    static uint16_t expected[240 * 324 + 2];
    uint32_t random = 0x12345678u;
    for (unsigned pattern = 0; pattern < 16; pattern++) {
        for (unsigned y = 0; y < 128; y++)
            for (unsigned x = 0; x < 8; x++) {
                random = random * 1664525u + 1013904223u;
                s->video[y][x] = pattern == 0 ? 0 : pattern == 1 ? 255 : (uint8_t)(random >> 24);
            }
        for (unsigned stride = 320; stride <= 324; stride += 4)
            for (int scan = 0; scan < 2; scan++) {
                for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
                    expected[i] = actual[i] = 0x1234;
                assert(!studio2_compose(s, actual + 1, stride, scan));
                for (unsigned y = 0; y < 240; y++)
                    for (unsigned x = 0; x < 320; x++) {
                        uint16_t color = 0;
                        if (y >= 24 && y < 216) {
                            unsigned sy = (y - 24) * 128 / 192, sx = x / 5;
                            if (s->video[sy][sx / 8] & (128u >> (sx & 7)))
                                color = scan && (y & 1) ? 0x8410 : 0xffff;
                        }
                        expected[1 + y * stride + x] = color;
                    }
                assert(!memcmp(actual, expected, sizeof(expected)));
            }
    }
}
static uint8_t rd(void *o, uint16_t a) {
    (void)o;
    return memory[a];
}
static void wr(void *o, uint16_t a, uint8_t v) {
    (void)o;
    memory[a] = v;
}
static uint8_t inp(void *o, unsigned n) {
    (void)o;
    return ports[n];
}
static void out(void *o, unsigned n, uint8_t v) {
    (void)o;
    ports[n] = v;
}
static cdp1802_bus_t bus = {NULL, rd, wr, inp, out};
static cdp1802_t cpu;
static void instruction(uint8_t op, uint8_t ef) {
    memory[cpu.r[cpu.p]] = op;
    cdp1802_tick(&cpu, &bus, ef, 0);
    while (cpu.phase)
        cdp1802_tick(&cpu, &bus, ef, 0);
}
static void arithmetic(void) {
    unsigned a, b, f;
    for (a = 0; a < 256; a++)
        for (b = 0; b < 256; b++)
            for (f = 0; f < 2; f++) {
                cdp1802_reset(&cpu);
                cpu.x = 3;
                cpu.r[3] = 100;
                memory[100] = (uint8_t)b;
                cpu.d = (uint8_t)a;
                cpu.df = (uint8_t)f;
                instruction(0x74, 0);
                assert(cpu.d == (uint8_t)(a + b + f));
                assert(cpu.df == ((a + b + f) > 255));
                cdp1802_reset(&cpu);
                cpu.x = 3;
                cpu.r[3] = 100;
                memory[100] = (uint8_t)b;
                cpu.d = (uint8_t)a;
                cpu.df = (uint8_t)f;
                instruction(0x77, 0);
                assert(cpu.d == (uint8_t)(a - b - (1 - f)));
                assert(cpu.df == ((int)a - (int)b - (int)(1 - f) >= 0));
                cdp1802_reset(&cpu);
                cpu.x = 3;
                cpu.r[3] = 100;
                memory[100] = (uint8_t)b;
                cpu.d = (uint8_t)a;
                cpu.df = (uint8_t)f;
                instruction(0x75, 0);
                assert(cpu.d == (uint8_t)(b - a - (1 - f)));
                assert(cpu.df == ((int)b - (int)a - (int)(1 - f) >= 0));
            }
}
static void cpu_paths(void) {
    unsigned op;
    for (op = 0; op < 256; op++) {
        cdp1802_reset(&cpu);
        cpu.r[0] = 0xfffe;
        cpu.r[2] = 0x200;
        cpu.r[3] = 0x100;
        cpu.x = 3;
        instruction((uint8_t)op, 15);
        assert(cpu.phase == 0);
        assert(cpu.cycles == ((op >> 4) == 12 ? 3u : 2u));
    }
    cdp1802_reset(&cpu);
    cpu.r[0] = 0xfe;
    memory[0xff] = 0x34;
    instruction(0x30, 0);
    assert(cpu.r[0] == 0x34);
    cdp1802_reset(&cpu);
    memory[1] = 0x12;
    memory[2] = 0x34;
    instruction(0xc0, 0);
    assert(cpu.r[0] == 0x1234);
    cdp1802_reset(&cpu);
    cpu.d = 0x81;
    instruction(0xfe, 0);
    assert(cpu.r[0] == 1 && cpu.d == 2 && cpu.df == 1);
    cdp1802_reset(&cpu);
    cpu.x = 3;
    cpu.r[3] = 0x100;
    cpu.r[2] = 0x200;
    instruction(0x79, 0);
    assert(cpu.t == 0x30 && memory[0x200] == 0x30 && cpu.r[2] == 0x1ff && cpu.x == 0);
    cdp1802_reset(&cpu);
    cpu.x = 3;
    cpu.r[3] = 0x100;
    memory[0x100] = 0x41;
    instruction(0x70, 0);
    assert(cpu.p == 1 && cpu.x == 4 && cpu.r[3] == 0x101 && cpu.ie);
    cdp1802_reset(&cpu);
    cpu.x = 3;
    cpu.r[3] = 100;
    memory[100] = 99;
    instruction(0x62, 0);
    assert(ports[2] == 99 && cpu.r[3] == 101);
    ports[1] = 77;
    instruction(0x69, 0);
    assert(cpu.d == 77 && memory[101] == 77 && cpu.r[3] == 101);
    cdp1802_reset(&cpu);
    instruction(0, 0);
    assert(cpu.idle);
    cpu.p = 3;
    cpu.x = 4;
    cdp1802_tick(&cpu, &bus, 0, 1);
    assert(cpu.p == 1 && cpu.x == 2 && cpu.t == 0x43 && !cpu.ie && !cpu.idle);
    cdp1802_reset(&cpu);
    cpu.r[0] = 0xffff;
    memory[0xffff] = 87;
    assert(cdp1802_dma_out(&cpu, &bus) == 87 && cpu.r[0] == 0);
    cdp1802_dma_in(&cpu, &bus, 32);
    assert(memory[0] == 32 && cpu.r[0] == 1);
}
static studio2_t machine, copy;
static uint8_t cart[1024], st2[768];
static uint16_t pixels[240 * 324 + 2];
static void board(void) {
    unsigned i;
    uint64_t samples = 0;
    studio2_init(&machine);
    assert(studio2_load_bios(&machine, diagnostic, 2047));
    assert(!studio2_load_bios(&machine, diagnostic, 2048));
    studio2_write(&machine, 0x800, 42);
    assert(studio2_read(&machine, 0x8400) == 42);
    assert(studio2_read(&machine, 0xa00) == 255);
    studio2_write(&machine, 0, 42);
    assert(studio2_read(&machine, 0) == diagnostic[0]);
    memset(cart, 55, sizeof(cart));
    assert(!studio2_load_cart(&machine, cart, 1024, 0));
    assert(studio2_read(&machine, 0x400) == 55);
    assert(studio2_load_cart(&machine, cart, 1025, 0));
    memset(st2, 0, sizeof(st2));
    memcpy(st2, "RCA2", 4);
    st2[4] = 3;
    st2[64] = 4;
    st2[65] = 12;
    st2[256] = 17;
    st2[512] = 19;
    assert(!studio2_load_cart(&machine, st2, sizeof(st2), 1));
    assert(studio2_read(&machine, 0x400) == 17 && studio2_read(&machine, 0xc00) == 19);
    copy = machine;
    st2[65] = 4;
    assert(studio2_load_cart(&machine, st2, sizeof(st2), 1));
    assert(!memcmp(&copy, &machine, sizeof(copy)));
    st2[65] = 8;
    assert(studio2_load_cart(&machine, st2, sizeof(st2), 1));
    st2[65] = 12;
    assert(studio2_load_cart(&machine, st2, sizeof(st2) - 1, 1));
    assert(!studio2_load_bios(&machine, diagnostic, 2048));
    copy = machine;
    for (i = 0; i < 120; i++) {
        studio2_frame(&machine);
        studio2_frame(&copy);
        assert(!memcmp(&copy, &machine, sizeof(copy)));
        samples += machine.audio_count;
    }
    assert(machine.display && machine.frames == 120 && machine.cpu.cycles == 120ull * 3668);
    assert(samples == 120ull * 3668 * 22050 / 220000);
    assert(machine.video[0][0] == 0xaa);
    assert(machine.video[32][0] == 0x55);
    machine.keys[0] = 2;
    studio2_frame(&machine);
    studio2_frame(&machine);
    assert(machine.cpu.q);
    machine.keys[0] = 0;
    studio2_frame(&machine);
    assert(!machine.cpu.q);
    assert(!studio2_load_bios(&machine, tone_diagnostic, sizeof(tone_diagnostic)));
    studio2_frame(&machine);
    studio2_frame(&machine);
    {
        unsigned nonzero = 0, positive = 0, negative = 0;
        for (i = 0; i < machine.audio_count; i++) {
            nonzero += machine.audio[i] != 0;
            positive += machine.audio[i] > 0;
            negative += machine.audio[i] < 0;
        }
        assert(machine.cpu.q && nonzero && positive && negative);
    }
    {
        unsigned changes = 0, frame;
        int previous = machine.audio[machine.audio_count - 1] > 0;
        for (frame = 0; frame < 60; frame++) {
            studio2_frame(&machine);
            for (i = 0; i < machine.audio_count; i++) {
                int sign = machine.audio[i] > 0;
                changes += sign != previous;
                previous = sign;
            }
        }
        assert(changes >= 599 && changes <= 602);
    }
    memset(machine.video, 0, sizeof(machine.video));
    machine.video[0][0] = 0x80;
    machine.video[127][7] = 1;
    for (i = 0; i < sizeof(pixels) / sizeof(pixels[0]); i++)
        pixels[i] = 0x1234;
    assert(!studio2_compose(&machine, pixels + 1, 324, 0));
    assert(pixels[0] == 0x1234 && pixels[240 * 324 + 1] == 0x1234);
    assert(pixels[1 + 24 * 324] == 0xffff && pixels[1 + 215 * 324 + 319] == 0xffff);
    assert(pixels[1 + 23 * 324] == 0);
    assert(pixels[1 + 24 * 324 + 320] == 0x1234);
    assert(studio2_compose(&machine, pixels, 319, 0));
    check_compose(&machine, pixels);
    assert(studio2_keypad(1u << COMPAT_KEY_START, 0) == 2);
    assert(studio2_keypad(1u << COMPAT_KEY_BACK, 0) == 0);
    assert(studio2_keypad(1u << COMPAT_KEY_LEFT, 1) == 128);
    assert(retropicoes_studio2_rom_profile("game.ST2", NULL));
    assert(retropicoes_studio2_rom_profile("test.s2d", NULL));
    assert(!retropicoes_studio2_rom_profile("not.bin.txt", NULL));
    {
        retropicoes_studio2_settings_t s;
        retropicoes_studio2_settings_defaults(&s);
        s.keypad_bank = 255;
        s.keypad_player = 255;
        assert(retropicoes_studio2_settings_sanitize(&s) > 0 && s.keypad_bank == 0 &&
               s.keypad_player == 0);
    }
    assert(retropicoes_studio2_menu_actions(1u << COMPAT_KEY_BACK, RETROPICOES_CONTROLLER_XBOX) &
           RETROPICOES_UI_MENU);
}
int main(void) {
    arithmetic();
    cpu_paths();
    board();
    puts("STUDIO2: arithmetic exhaustive, 256 opcodes, IRQ/DMA, ROM rejection, deterministic "
         "boot/video/audio, keypad and geometry PASS");
    return 0;
}
