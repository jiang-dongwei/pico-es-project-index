/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "trs80.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { printf("line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static uint8_t jv1[2560u * 2u], jv3[8704u + 256u + 128u];
static int sector(unsigned count, uint64_t *now, unsigned base)
{
    for (unsigned i = 0; i < count; ++i) {
        *now = trs80_disk_wait(*now);
        CHECK(trs80_disk_read(0u, *now, 2000000u) & 2u);
        CHECK(trs80_disk_read(3u, *now, 2000000u) == (uint8_t)(base + i));
    }
    return 0;
}
int main(void)
{
    uint64_t now = 0u;
    CHECK(trs80_init(TRS80_MODEL_I) == TRS80_OK);
    CHECK(trs80_disk_read(0u, now, 2000000u) == 255u);
    CHECK(trs80_mount_disk(4u, jv1, sizeof(jv1), TRS80_DISK_JV1) == TRS80_INVALID);
    CHECK(trs80_mount_disk(0u, jv1, 2561u, TRS80_DISK_JV1) == TRS80_INVALID);
    for (unsigned i = 0; i < sizeof(jv1); ++i) jv1[i] = (uint8_t)i;
    CHECK(trs80_mount_disk(0u, jv1, sizeof(jv1), TRS80_DISK_JV1) == TRS80_OK);
    trs80_disk_select(1u, 1, now, 2000000u);
    trs80_disk_write(0u, 0x80u, now, 2000000u);
    CHECK(!trs80_disk_irq(now));
    CHECK(!(trs80_disk_read(0u, now, 2000000u) & 2u));
    CHECK(sector(256u, &now, 0u) == 0);
    now = trs80_disk_wait(now);
    CHECK(trs80_disk_irq(now));
    CHECK(!(trs80_disk_read(0u, now, 2000000u) & 1u));
    CHECK(!trs80_disk_irq(now));
    trs80_disk_write(2u, 10u, now, 2000000u);
    trs80_disk_write(0u, 0x80u, now, 2000000u);
    now = trs80_disk_wait(now);
    CHECK(trs80_disk_read(0u, now, 2000000u) & 0x10u);
    trs80_disk_write(0u, 0xa0u, now, 2000000u);
    now = trs80_disk_wait(now);
    CHECK(trs80_disk_read(0u, now, 2000000u) & 0x40u);
    CHECK(jv1[0] == 0u);
    memset(jv3, 255, 8704u);
    jv3[0] = 0; jv3[1] = 1; jv3[2] = 0x80;
    jv3[3] = 0; jv3[4] = 2; jv3[5] = 0x81;
    for (unsigned i = 0; i < 384u; ++i) jv3[8704u+i] = (uint8_t)i;
    CHECK(trs80_mount_disk(0u, jv3, sizeof(jv3)-1u, TRS80_DISK_JV3) == TRS80_TRUNCATED);
    CHECK(trs80_mount_disk(0u, jv3, sizeof(jv3), TRS80_DISK_JV3) == TRS80_OK);
    trs80_disk_reset();
    trs80_disk_select(0x81u, 3, now, 2000000u);
    trs80_disk_write(2u, 1u, now, 2000000u);
    trs80_disk_write(0u, 0x90u, now, 2000000u);
    CHECK(sector(384u, &now, 0u) == 0);
    now = trs80_disk_wait(now);
    CHECK(trs80_disk_irq(now));
    CHECK(trs80_disk_read(0u, now, 2000000u) & 0x10u);
    trs80_disk_select(1u, 3, now, 2000000u);
    trs80_disk_write(2u, 1u, now, 2000000u);
    trs80_disk_write(0u, 0x80u, now, 2000000u);
    now = trs80_disk_wait(now);
    CHECK(trs80_disk_read(0u, now, 2000000u) & 0x10u);
    trs80_disk_select(0x81u, 3, now, 2000000u);
    trs80_disk_write(0u, 0x80u, now, 2000000u);
    trs80_disk_write(0u, 0xd0u, now, 2000000u);
    CHECK(!trs80_disk_irq(now));
    CHECK(!(trs80_disk_read(0u, now, 2000000u) & 1u));
    trs80_disk_write(0u, 0x80u, now, 2000000u);
    now = trs80_disk_wait(now);
    now += 256u;
    CHECK(trs80_disk_irq(now));
    CHECK(trs80_disk_read(0u, now, 2000000u) & 4u);
    CHECK(trs80_disk_read(0u, now + 4000000u, 2000000u) & 0x80u);
    CHECK(trs80_init(TRS80_MODEL_I)==TRS80_OK);
    CHECK(trs80_mount_disk(0u,jv1,sizeof(jv1),TRS80_DISK_JV1)==TRS80_OK);
    trs80_write(0x37e0u,1u); trs80_write(0x37ecu,0xd8u);
    CHECK(trs80_read(0x37e0u)==0x40u);
    CHECK(trs80_read(0x37e0u)==0x40u);
    (void)trs80_read(0x37ecu); CHECK(trs80_read(0x37e0u)==0u);
    CHECK(trs80_init(TRS80_MODEL_III)==TRS80_OK);
    CHECK(trs80_mount_disk(0u,jv3,sizeof(jv3),TRS80_DISK_JV3)==TRS80_OK);
    trs80_out(0xf4u,0x81u); trs80_out(0xf0u,0xd8u);
    CHECK(trs80_in(0xe4u)==0x7fu);
    (void)trs80_in(0xf0u); CHECK(trs80_in(0xe4u)==0xffu);
    puts("disk controller: PASS");
    return 0;
}
