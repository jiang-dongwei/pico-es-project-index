/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "trs80.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned failures;
#define CHECK(e) do { if (!(e)) { printf("FAIL %d: %s\n", __LINE__, #e); ++failures; } } while (0)
static void program(const uint8_t *bytes, size_t size)
{
    uint8_t cmd[264];
    CHECK(size > 0u && size <= 256u);
    cmd[0] = 1u; cmd[1] = (uint8_t)(size + 2u); cmd[2] = 0u; cmd[3] = 0x40u;
    memcpy(cmd + 4u, bytes, size);
    cmd[size + 4u] = 2u; cmd[size + 5u] = 2u;
    cmd[size + 6u] = 0u; cmd[size + 7u] = 0x40u;
    CHECK(trs80_load_cmd(cmd, size + 8u) == TRS80_OK);
}
static void test_cpu(void)
{
    /* RAM loop copies bytes via IX/IY, adds, rotates, then writes video. */
    static const uint8_t code[] = {
        0xdd,0x21,0x00,0x41, 0xdd,0x36,0x01,0x42,
        0xdd,0x7e,0x01, 0xc6,0x01, 0xcb,0x07,
        0xfd,0x21,0x00,0x42, 0xfd,0x77,0x02,
        0x32,0x00,0x3c, 0xc3,0x19,0x40
    };
    for (unsigned i = 0u; i < 3u; ++i) {
        trs80_model_t model = i == 0u ? TRS80_MODEL_I : (i == 1u ? TRS80_MODEL_III : TRS80_MODEL_4);
        CHECK(trs80_init(model) == TRS80_OK);
        program(code, sizeof(code));
        CHECK(trs80_run_cycles(300u) == TRS80_OK);
        CHECK(trs80_read(0x4101u) == 0x42u);
        CHECK(trs80_read(0x4202u) == 0x86u);
        CHECK(trs80_read(0x3c00u) == 0x86u);
        CHECK(trs80_status().pc == 0x4019u);
        CHECK(trs80_status().ix == 0x4100u && trs80_status().iy == 0x4200u);
    }
    CHECK(trs80_run_cycles(0u) == TRS80_INVALID);
    CHECK(trs80_run_cycles(10000001u) == TRS80_INVALID);
}
static void test_memory(void)
{
    static const uint8_t boot[] = {0x3e,0x55,0x32,0x00,0x40,0xc3,0x05,0x00};
    CHECK(trs80_init(TRS80_MODEL_I) == TRS80_OK);
    CHECK(trs80_load_bios(boot, sizeof(boot)) == TRS80_OK);
    trs80_write(0u, 0xaau);
    CHECK(trs80_read(0u) == boot[0]);
    CHECK(trs80_run_cycles(100u) == TRS80_OK);
    CHECK(trs80_read(0x4000u) == 0x55u);
    CHECK(trs80_read(0x3000u) == 0xffu);
    CHECK(trs80_load_bios(NULL, 4u) == TRS80_INVALID);
    CHECK(trs80_load_bios(boot, 0x3001u) == TRS80_INVALID);
    CHECK(trs80_init(TRS80_MODEL_4) == TRS80_OK);
    trs80_write(0x8000u, 0x11u);
    trs80_out(0x84u, 0x23u);
    trs80_write(0x8000u, 0x22u);
    trs80_write(0x0000u, 0x44u);
    trs80_out(0x84u, 0x33u);
    trs80_write(0x8000u, 0x33u);
    trs80_out(0x84u, 0x63u);
    CHECK(trs80_read(0x0000u) == 0x22u);
    trs80_write(0x0000u, 0x66u);
    trs80_out(0x84u, 0x73u);
    trs80_write(0x0000u, 0x77u);
    trs80_out(0x84u, 0x03u);
    CHECK(trs80_read(0x8000u) == 0x11u && trs80_read(0u) == 0x44u);
    trs80_out(0x84u, 0x23u); CHECK(trs80_read(0x8000u) == 0x66u);
    trs80_out(0x84u, 0x33u); CHECK(trs80_read(0x8000u) == 0x77u);
    trs80_out(0x84u, 0x73u); CHECK(trs80_read(0x0000u) == 0x77u);
    trs80_reset();
    trs80_write(0x3c00u, 0x41u);
    trs80_out(0x84u, 0x80u); trs80_write(0x3c00u, 0x42u);
    trs80_out(0x84u, 0u); CHECK(trs80_read(0x3c00u) == 0x41u);
    trs80_out(0x84u, 2u);
    CHECK(trs80_read(0xf800u) == 0x41u);
    trs80_write(0xff7fu, 0x43u);
    size_t count = 0u; const uint8_t *video = trs80_video(&count);
    CHECK(count == 2048u && video[1919] == 0x43u);
    trs80_out(0x84u, 7u);
    CHECK(trs80_status().columns == 80u && trs80_status().rows == 24u);
    trs80_out(0x84u, 0x10u); CHECK(trs80_status().fault);
    static uint8_t external[0x10002u];
    CHECK(trs80_bind_expansion_ram(NULL,0x10000u) == TRS80_INVALID);
    CHECK(trs80_bind_expansion_ram(external,0x10001u) == TRS80_INVALID);
    external[0]=0xaau; external[0x10001u]=0xbbu;
    CHECK(trs80_bind_expansion_ram(external+1u,0x10000u) == TRS80_OK);
    CHECK(trs80_init(TRS80_MODEL_4) == TRS80_OK);
    trs80_out(0x84u,0x33u); trs80_write(0xffffu,0xeeu);
    trs80_out(0x84u,0x73u); CHECK(trs80_read(0x7fffu)==0xeeu);
    CHECK(external[0]==0xaau && external[0x10001u]==0xbbu);
}
static void test_keyboard(void)
{
    static const uint8_t code[] = {0x3a,0x40,0x38,0x32,0x00,0x3c,0xc3,0x00,0x40};
    CHECK(trs80_init(TRS80_MODEL_III) == TRS80_OK);
    program(code, sizeof(code));
    CHECK(trs80_key(TRS80_MATRIX_ENTER, 1));
    CHECK(trs80_run_cycles(100u) == TRS80_OK);
    CHECK(trs80_read(0x3c00u) == 1u);
    CHECK(trs80_key(1u, 1));
    CHECK(trs80_read(0x3841u) == 3u);
    CHECK(trs80_key(TRS80_MATRIX_ENTER, 0));
    CHECK(trs80_run_cycles(100u) == TRS80_OK);
    CHECK(trs80_read(0x3c00u) == 0u);
    trs80_release_keys(); CHECK(trs80_read(0x38ffu) == 0u);
    CHECK(!trs80_key(TRS80_MATRIX_CTRL, 1));
    CHECK(!trs80_key(64u, 1));
    CHECK(trs80_init(TRS80_MODEL_4) == TRS80_OK);
    CHECK(trs80_key(TRS80_MATRIX_CTRL, 1));
    trs80_out(0x84u, 2u); CHECK(trs80_read(0xf480u) == 4u);
    trs80_reset(); CHECK(trs80_read(0x3880u) == 0u);
}
static void test_loader(void)
{
    static const uint8_t valid[] = {5,3,'T','E','S', 1,3,0,0x40,0xc3, 2,2,0,0x40};
    CHECK(trs80_init(TRS80_MODEL_III) == TRS80_OK);
    for (size_t size = 1u; size < sizeof(valid); ++size) {
        trs80_write(0x4000u, 0xa5u);
        CHECK(trs80_load_cmd(valid, size) != TRS80_OK);
        CHECK(trs80_read(0x4000u) == 0xa5u);
        CHECK(trs80_status().pc == 0u);
    }
    CHECK(trs80_load_cmd(valid, sizeof(valid)) == TRS80_OK);
    CHECK(trs80_read(0x4000u) == 0xc3u && trs80_status().pc == 0x4000u);
    static const uint8_t protected_cmd[] = {1,3,0,0x00,0xff,2,2,0,0};
    CHECK(trs80_load_cmd(protected_cmd, sizeof(protected_cmd)) == TRS80_PROTECTED);
    static const uint8_t isam[] = {8,1,0};
    CHECK(trs80_load_cmd(isam, sizeof(isam)) == TRS80_UNSUPPORTED);
    static const uint8_t no_entry[] = {1,3,0,0x40,0};
    CHECK(trs80_load_cmd(no_entry, sizeof(no_entry)) == TRS80_NO_ENTRY);
    CHECK(trs80_load_cmd(NULL, 1u) == TRS80_INVALID);
    CHECK(trs80_load_cmd(valid, TRS80_MAX_CMD_BYTES + 1u) == TRS80_INVALID);
    for (size_t size = 254u; size <= 256u; ++size) {
        uint8_t payload[256]; memset(payload, 0x5au, sizeof(payload));
        program(payload, size);
        CHECK(trs80_read((uint16_t)(0x4000u + size - 1u)) == 0x5au);
    }
    static const uint8_t wrapping[] = {1,4,0xff,0xff,0x12,0x34,2,2,0xff,0xff};
    CHECK(trs80_load_cmd(wrapping, sizeof(wrapping)) == TRS80_PROTECTED);
    CHECK(trs80_init(TRS80_MODEL_4) == TRS80_OK);
    trs80_out(0x84u, 3u);
    CHECK(trs80_load_cmd(wrapping, sizeof(wrapping)) == TRS80_OK);
    CHECK(trs80_read(0xffffu) == 0x12u && trs80_read(0u) == 0x34u);
}
static void test_interrupt(void)
{
    uint8_t boot[64]; memset(boot, 0u, sizeof(boot));
    boot[0x38] = 0x3eu; boot[0x39] = 0x5au;
    boot[0x3a] = 0x32u; boot[0x3b] = 0u; boot[0x3c] = 0x41u;
    boot[0x3d] = 0x76u;
    static const uint8_t code[] = {0xed,0x56,0xfb,0x76,0xc3,0x03,0x40};
    CHECK(trs80_init(TRS80_MODEL_III) == TRS80_OK);
    CHECK(trs80_load_bios(boot, sizeof(boot)) == TRS80_OK);
    program(code, sizeof(code));
    trs80_out(0xe0u, 4u);
    CHECK(trs80_run_cycles(68000u) == TRS80_OK);
    CHECK(trs80_read(0x4100u) == 0x5au);
    CHECK((trs80_in(0xe0u) & 4u) == 0u);
    CHECK(trs80_in(0xecu) == 0xffu);
    CHECK((trs80_in(0xe0u) & 4u) != 0u);
    CHECK(trs80_init(TRS80_MODEL_4) == TRS80_OK);
    CHECK(trs80_status().clock_hz == 2027520u);
    trs80_out(0xecu, 0x48u); CHECK(trs80_status().clock_hz == 4055040u);
    trs80_out(0xecu, 8u); CHECK(trs80_status().clock_hz == 2027520u);
}
static void test_determinism(void)
{
    static const uint8_t code[] = {0x3c,0x32,0x00,0x3c,0xc3,0x00,0x40};
    CHECK(trs80_init(TRS80_MODEL_III) == TRS80_OK);
    program(code, sizeof(code)); CHECK(trs80_run_cycles(50000u) == TRS80_OK);
    trs80_status_t a = trs80_status(); uint8_t v = trs80_read(0x3c00u);
    CHECK(trs80_init(TRS80_MODEL_III) == TRS80_OK);
    program(code, sizeof(code)); CHECK(trs80_run_cycles(50000u) == TRS80_OK);
    trs80_status_t b = trs80_status();
    CHECK(a.pc == b.pc && a.af == b.af && a.cycles == b.cycles);
    CHECK(v == trs80_read(0x3c00u));
    static const uint8_t trap[] = {0xed,0x28,0xc3,0x00,0x40};
    program(trap, sizeof(trap)); CHECK(trs80_run_cycles(100u) == TRS80_CPU_FAULT);
    trs80_reset(); CHECK(!trs80_status().fault);
    CHECK(trs80_init((trs80_model_t)2) == TRS80_INVALID);
}
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    switch (atoi(argv[1])) {
    case 1: test_cpu(); break; case 2: test_memory(); break;
    case 3: test_keyboard(); break; case 4: test_loader(); break;
    case 5: test_interrupt(); break; case 6: test_determinism(); break;
    default: return 2;
    }
    if (failures) return 1;
    puts("TRS80 MACHINE: PASS"); return 0;
}
