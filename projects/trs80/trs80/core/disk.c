/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "disk.h"
#include <string.h>
typedef struct { const uint8_t *data; size_t size; trs80_disk_format_t format; unsigned head; } drive_t;
static drive_t drives[4];
static struct {
    uint8_t track, sector, data, command, status, tail;
    unsigned drive, side, density, direction, selected, intrq;
    const uint8_t *transfer;
    size_t remaining;
    uint32_t byte_cycles;
    uint64_t due, motor, lost;
} fdc;
static void done(void) { fdc.status &= (uint8_t)~3u; fdc.intrq = 1u; fdc.due = fdc.lost = 0u; }
static int find_sector(int address)
{
    drive_t *d = &drives[fdc.drive];
    size_t offset = 8704u;
    fdc.tail = 0u;
    if (!fdc.selected || !d->data) return 0;
    if (d->format == TRS80_DISK_JV1) {
        if (fdc.side || fdc.density || fdc.sector >= 10u || d->head != fdc.track) return 0;
        offset = ((size_t)fdc.track * 10u + fdc.sector) * 256u;
        if (offset + 256u > d->size) return 0;
        fdc.transfer = d->data + offset; fdc.remaining = 256u;
        if (fdc.track == 17u) fdc.tail = 0x20u;
        return !address;
    }
    for (unsigned i = 0u; i < 2901u; ++i) {
        const uint8_t *id = d->data + i * 3u;
        static const unsigned sizes[4] = {256u,128u,1024u,512u};
        unsigned size = sizes[(id[2] & 3u) ^ (id[0] == 255u ? 2u : 0u)];
        if (id[0] != 255u && offset + size > d->size) return 0;
        if (id[0] == d->head && id[0] == fdc.track &&
            (address || id[1] == fdc.sector) && ((id[2] >> 4) & 1u) == fdc.side &&
            (id[2] >> 7) == fdc.density) {
            if (address) return 0; /* READ ADDRESS is not implemented yet. */
            fdc.transfer = d->data + offset; fdc.remaining = size;
            fdc.tail = (uint8_t)((id[2] & 8u) | (id[2] & 0x60u));
            return 1;
        }
        offset += size;
    }
    return 0;
}
trs80_result_t trs80_mount_disk(unsigned drive, const uint8_t *data, size_t size,
                               trs80_disk_format_t format)
{
    if (drive >= 4u || !data) return TRS80_INVALID;
    if (format == TRS80_DISK_JV1) {
        if (!size || size % 2560u || size > 2560u * 256u) return TRS80_INVALID;
    } else if (format == TRS80_DISK_JV3) {
        if (size < 8704u) return TRS80_TRUNCATED;
        size_t offset = 8704u;
        for (unsigned i = 0; i < 2901u; ++i) {
            const uint8_t *id = data + i * 3u;
            static const unsigned sizes[4] = {256u,128u,1024u,512u};
            unsigned n = sizes[(id[2] & 3u) ^ (id[0] == 255u ? 2u : 0u)];
            if (id[0] != 255u && (offset > size || n > size - offset)) return TRS80_TRUNCATED;
            offset += n;
        }
        if (size > offset) return TRS80_UNSUPPORTED; /* Two-block JV3 needs a separate parser. */
    } else return TRS80_UNSUPPORTED;
    drives[drive] = (drive_t){data, size, format, 0u};
    if (fdc.drive == drive) trs80_disk_reset();
    return TRS80_OK;
}
void trs80_unmount_disks(void) { memset(drives, 0, sizeof(drives)); trs80_disk_reset(); }
void trs80_disk_reset(void) { memset(&fdc, 0, sizeof(fdc)); }
void trs80_disk_select(uint8_t value, int model, uint64_t now, uint32_t hz)
{
    unsigned mask = value & 15u;
    fdc.side = model == 1 ? ((mask & 7u) && (mask & 8u)) : ((value >> 4) & 1u);
    if (model == 1 && fdc.side) mask &= 7u;
    fdc.density = model == 1 ? 0u : value >> 7;
    fdc.selected = mask && !(mask & (mask - 1u));
    for (unsigned i = 0; i < 4u; ++i) if (mask == (1u << i)) fdc.drive = i;
    if (fdc.selected) fdc.motor = now + (uint64_t)hz * 2u;
}
static void tick(uint64_t now)
{
    if (fdc.due && now >= fdc.due) {
        fdc.due = 0;
        if (fdc.remaining) { fdc.status |= 2u; fdc.lost = now + fdc.byte_cycles * 2u; }
        else done();
    }
    if (fdc.lost && now >= fdc.lost) {
        fdc.remaining = 0u; fdc.status |= 4u; done();
    }
}
int trs80_disk_irq(uint64_t now) { tick(now); return (int)fdc.intrq; }
uint64_t trs80_disk_wait(uint64_t now)
{
    if (fdc.due > now) now = fdc.due;
    tick(now);
    return now;
}
uint8_t trs80_disk_read(unsigned reg, uint64_t now, uint32_t hz)
{
    if (!drives[0].data && !drives[1].data && !drives[2].data && !drives[3].data) return 0xffu;
    tick(now);
    if (reg == 0u) {
        uint8_t status = fdc.status;
        fdc.intrq = 0u;
        if (fdc.command < 0x80u || (fdc.command & 0xf0u) == 0xd0u) {
            status = (uint8_t)((status & ~6u) | (drives[fdc.drive].head == 0u ? 4u : 0u));
            if (now % (hz / 5u) < hz / 100u) status |= 2u;
            status |= 0x40u;
        }
        if (!fdc.selected || !drives[fdc.drive].data || now >= fdc.motor) status |= 0x80u;
        return status;
    }
    if (reg == 1u) return fdc.track;
    if (reg == 2u) return fdc.sector;
    if (fdc.remaining && (fdc.status & 2u)) {
        fdc.data = *fdc.transfer++; --fdc.remaining;
        fdc.lost = 0u;
        fdc.status &= (uint8_t)~2u;
        if (!fdc.remaining) {
            fdc.status |= fdc.tail;
            if ((fdc.command & 0xf0u) == 0x90u) {
                ++fdc.sector;
                if (!find_sector(0)) fdc.status |= 0x10u;
            }
        }
        fdc.due = now + hz / (fdc.density ? 31250u : 15625u);
    }
    return fdc.data;
}
void trs80_disk_write(unsigned reg, uint8_t value, uint64_t now, uint32_t hz)
{
    if (reg == 1u) { fdc.track = value; return; }
    if (reg == 2u) { fdc.sector = value; return; }
    if (reg == 3u) { fdc.data = value; return; }
    fdc.command = value; fdc.intrq = 0u; fdc.remaining = 0u; fdc.status = 1u;
    fdc.lost = 0u;
    fdc.byte_cycles = hz / (fdc.density ? 31250u : 15625u);
    fdc.due = now + hz / 500u;
    drive_t *d = &drives[fdc.drive];
    unsigned cmd = value >> 4;
    if (cmd < 8u) {
        if (cmd == 0u) { d->head = 0; fdc.track = 0; }
        else if (cmd == 1u) { d->head = fdc.data; fdc.track = fdc.data; }
        else {
            if (cmd >= 4u) fdc.direction = cmd < 6u;
            if (fdc.direction) { if (d->head < 255u) ++d->head; }
            else if (d->head) --d->head;
            if (value & 0x10u) fdc.track = (uint8_t)d->head;
        }
    } else if (cmd == 8u || cmd == 9u) {
        if (!find_sector(0)) fdc.status |= 0x10u;
    } else if (cmd == 13u) {
        fdc.status = 0u; fdc.due = 0u; fdc.intrq = (value & 8u) != 0u;
    } else if (cmd == 10u || cmd == 11u || cmd == 15u) fdc.status |= 0x40u;
    else fdc.status |= 0x10u;
}
