/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "trs80.h"
#include "z80.h"
#include "trs.h"
#include "disk.h"
#include <string.h>

int trs_model;
static uint8_t ram[0x10000], bios[TRS80_MAX_BIOS_BYTES], screen[0x800];
#ifndef TRS80_EXTERNAL_EXPANSION
static uint8_t expansion_storage[0x10000];
static uint8_t *expansion = expansion_storage;
#else
static uint8_t *expansion;
#endif
static uint8_t keys[8], irq_latch, irq_mask, mode_image, map_image;
static size_t bios_size;
static unsigned banks[2], video_page;
static uint32_t clock_hz, timer_period;
static uint64_t next_timer;
static int cpu_fault;
trs80_result_t trs80_bind_expansion_ram(uint8_t *data, size_t size)
{
    if (!data || size != 0x10000u) return TRS80_INVALID;
    expansion = data; return TRS80_OK;
}
static uint8_t read_ram(uint16_t address)
{
    unsigned a = address + banks[address >> 15];
    if (a < sizeof(ram)) return ram[a];
    if (expansion) return expansion[a - sizeof(ram)];
    cpu_fault = 1; return 0xffu;
}
static void write_ram(uint16_t address, uint8_t value)
{
    unsigned a = address + banks[address >> 15];
    if (a < sizeof(ram)) ram[a] = value;
    else if (expansion) expansion[a - sizeof(ram)] = value;
    else cpu_fault = 1;
}
static uint8_t nmi_mask;
static trs80_audio_sink_t audio_sink;
static void *audio_context;
static uint64_t audio_phase;
static int16_t audio_level;
void trs80_set_audio_sink(trs80_audio_sink_t sink, void *context)
{ audio_sink = sink; audio_context = context; audio_phase = 0u; }
static void update_disk(void)
{
    int intrq = trs80_disk_irq(z80_state.t_count);
    if (trs_model == 1) {
        irq_latch = (uint8_t)((irq_latch & ~0x40u) | (intrq ? 0x40u : 0u));
        z80_state.irq = irq_latch != 0u;
    } else {
        z80_state.nmi = intrq && (nmi_mask & 0x80u);
        if (!z80_state.nmi) z80_state.nmi_seen = 0;
    }
}
static unsigned map_mode(void) { return map_image & 3u; }
static int video_address(uint16_t address)
{
    if (trs_model == 4 && map_mode() == 3u) return -1;
    if (trs_model == 4 && map_mode() == 2u)
        return address >= 0xf800u ? (int)(address - 0xf800u) : -1;
    if (address >= 0x3c00u && address < 0x4000u)
        return (int)(address - 0x3c00u + (trs_model == 4 ? video_page : 0u));
    return -1;
}
static int keyboard_address(uint16_t address)
{
    if (trs_model == 4 && map_mode() == 3u) return 0;
    if (trs_model == 4 && map_mode() == 2u)
        return address >= 0xf400u && address < 0xf800u;
    return address >= 0x3800u && address < 0x3c00u;
}
static int writable(uint16_t address)
{
    if (video_address(address) >= 0) return 1;
    if (keyboard_address(address)) return 0;
    if (trs_model == 4 && map_mode() != 0u) return 1;
    return address >= 0x4000u;
}
static void update_irq(void)
{
    z80_state.irq = trs_model == 1 ? irq_latch != 0u : (irq_latch & irq_mask) != 0u;
}
static void set_speed(int fast)
{
    clock_hz = trs_model == 1 ? 1774080u : (trs_model == 4 && fast ? 4055040u : 2027520u);
    timer_period = clock_hz / (trs_model == 1 ? 40u : (trs_model == 4 && fast ? 60u : 30u));
    next_timer = z80_state.t_count + timer_period;
    z80_state.clockMHz = (float)clock_hz / 1000000.0f;
}
trs80_result_t trs80_init(trs80_model_t model)
{
    if (model != TRS80_MODEL_I && model != TRS80_MODEL_III && model != TRS80_MODEL_4)
        return TRS80_INVALID;
    trs_model = model;
    memset(ram, 0, sizeof(ram));
    if (expansion) memset(expansion, 0, 0x10000u);
    memset(bios, 0xff, sizeof(bios));
    memset(screen, 0x20, sizeof(screen));
    bios_size = 0u;
    trs80_unmount_disks();
    trs80_reset();
    return TRS80_OK;
}
void trs80_reset(void)
{
    memset(&z80_state, 0, sizeof(z80_state));
    z80_reset();
    irq_latch = irq_mask = map_image = 0u;
    nmi_mask = 0u;
    trs80_disk_reset();
    mode_image = 8u;
    banks[0] = banks[1] = video_page = 0u;
    cpu_fault = 0;
    audio_level = 0; audio_phase = 0u;
    x_poll_count = 0;
    trs80_release_keys();
    set_speed(0);
}
trs80_result_t trs80_load_bios(const uint8_t *data, size_t size)
{
    size_t limit = trs_model == 1 ? 0x3000u : TRS80_MAX_BIOS_BYTES;
    if (data == NULL || size == 0u || size > limit) return TRS80_INVALID;
    memset(bios, 0xff, sizeof(bios));
    memcpy(bios, data, size);
    bios_size = size;
    trs80_reset();
    return TRS80_OK;
}
uint8_t trs80_read(uint16_t address)
{
    if (trs_model == 1 && address >= 0x37ecu && address <= 0x37efu) {
        uint8_t value = trs80_disk_read(address - 0x37ecu, z80_state.t_count, clock_hz);
        update_disk();
        return value;
    }
    int vaddr = video_address(address);
    if (vaddr >= 0) return screen[vaddr];
    if (keyboard_address(address)) {
        uint8_t value = 0u;
        for (unsigned row = 0; row < 8u; ++row)
            if ((address & (1u << row)) != 0u) value |= keys[row];
        return value;
    }
    if (trs_model == 4 && map_mode() != 0u) return read_ram(address);
    if (address >= 0x4000u) return read_ram(address);
    if (address < bios_size) return bios[address];
    if (trs_model == 1 && (address & 0xfffcu) == 0x37e0u) {
        uint8_t value = irq_latch;
        irq_latch &= (uint8_t)~0x80u;
        update_irq();
        return value;
    }
    return 0xffu;
}
void trs80_write(uint16_t address, uint8_t value)
{
    if (trs_model == 1 && (address & 0xfffcu) == 0x37e0u) {
        trs80_disk_select(value, trs_model, z80_state.t_count, clock_hz);
        return;
    }
    if (trs_model == 1 && address >= 0x37ecu && address <= 0x37efu) {
        trs80_disk_write(address - 0x37ecu, value, z80_state.t_count, clock_hz);
        update_disk();
        return;
    }
    int vaddr = video_address(address);
    if (vaddr >= 0) screen[vaddr] = value;
    else if (writable(address)) write_ram(address, value);
}
int trs80_key(unsigned key, int down)
{
    if (key >= 63u || (trs_model < 4 && key >= 58u)) return 0;
    if (trs_model == 1 && key == 57u) key = 56u;
    if (down) keys[key >> 3] |= (uint8_t)(1u << (key & 7u));
    else keys[key >> 3] &= (uint8_t)~(1u << (key & 7u));
    return 1;
}
void trs80_release_keys(void) { memset(keys, 0, sizeof(keys)); }
const uint8_t *trs80_video(size_t *size)
{
    if (size != NULL) *size = trs_model == 4 ? 0x800u : 0x400u;
    return screen;
}
uint8_t trs80_in(uint8_t port)
{
    if (trs_model >= 3 && port >= 0xf0u && port <= 0xf3u) {
        uint8_t value = trs80_disk_read(port - 0xf0u, z80_state.t_count, clock_hz);
        update_disk();
        return value;
    }
    if (trs_model >= 3 && port == 0xe4u)
        return (uint8_t)~(trs80_disk_irq(z80_state.t_count) ? 0x80u : 0u);
    if (trs_model >= 3 && port == 0xe0u) return (uint8_t)~irq_latch;
    if (trs_model >= 3 && port >= 0xecu && port <= 0xefu) {
        irq_latch &= (uint8_t)~4u;
        update_irq();
    }
    if (trs_model >= 3 && port == 0xffu) return mode_image & 0x7eu;
    return 0xffu;
}
void trs80_out(uint8_t port, uint8_t value)
{
    if (trs_model >= 3 && port >= 0xf0u && port <= 0xf3u) {
        trs80_disk_write(port - 0xf0u, value, z80_state.t_count, clock_hz);
        update_disk();
    } else if (trs_model >= 3 && port >= 0xf4u && port <= 0xf7u) {
        trs80_disk_select(value, trs_model, z80_state.t_count, clock_hz);
        if (value & 0x40u) z80_state.t_count = trs80_disk_wait(z80_state.t_count);
        update_disk();
    } else if (trs_model >= 3 && port == 0xe4u) {
        nmi_mask = value;
        update_disk();
    } else
    if (trs_model == 4 && port >= 0x84u && port <= 0x87u) {
        unsigned bank = (value >> 4) & 7u;
        if (bank != 0u && bank != 2u && bank != 3u && bank != 6u && bank != 7u) {
            cpu_fault = 1;
            return;
        }
        map_image = value;
        video_page = (value & 0x80u) != 0u ? 1024u : 0u;
        banks[0] = bank >= 6u ? (bank == 6u ? 0x10000u : 0x18000u) : 0u;
        banks[1] = bank == 2u ? 0x8000u : (bank == 3u ? 0x10000u : 0u);
    } else if (trs_model >= 3 && port == 0xe0u) {
        irq_mask = value;
        update_irq();
    } else if (trs_model >= 3 && port >= 0xecu && port <= 0xefu) {
        int changed = (mode_image ^ value) & 0x40u;
        mode_image = value;
        if (changed) set_speed((value & 0x40u) != 0u);
    } else if (port == 0xffu) {
        static const int16_t levels[4] = {0, 12000, -12000, 0};
        audio_level = levels[value & 3u];
        if (trs_model == 1) mode_image = (uint8_t)((mode_image & ~4u) | ((value & 8u) >> 1));
    } else if (trs_model >= 3 && port >= 0x90u && port <= 0x93u) {
        audio_level = (value & 1u) ? 12000 : -12000;
    }
}
trs80_status_t trs80_status(void)
{
    trs80_status_t s = {REG_PC, REG_SP, REG_AF, REG_BC, REG_DE, REG_HL, REG_IX, REG_IY,
        z80_state.t_count, clock_hz, trs_model == 4 && (map_image & 4u) ? 80u : 64u,
        trs_model == 4 && (map_image & 4u) ? 24u : 16u, map_mode(), cpu_fault,
        trs_model == 4 && map_mode() < 2u ? video_page : 0u,
        trs_model == 4 && (map_image & 8u) != 0u, (mode_image & 8u) == 0u};
    if (mode_image & 4u) s.columns /= 2u;
    return s;
}
trs80_result_t trs80_run_cycles(uint32_t cycles)
{
    uint64_t start = z80_state.t_count;
    if (cycles == 0u || cycles > 10000000u) return TRS80_INVALID;
    while (z80_state.t_count - start < cycles) {
        if (cpu_fault) return TRS80_CPU_FAULT;
        uint64_t before = z80_state.t_count;
        uint32_t hz = clock_hz;
        if (z80_run(0) != 0) cpu_fault = 1;
        if (audio_sink) {
            audio_phase += (z80_state.t_count - before) * 22050u;
            while (audio_phase >= hz) { audio_phase -= hz; audio_sink(audio_level, audio_context); }
        }
        update_disk();
        if (z80_state.t_count >= next_timer) {
            irq_latch |= trs_model == 1 ? 0x80u : 4u;
            update_irq();
            do { next_timer += timer_period; } while (next_timer <= z80_state.t_count);
        }
    }
    return cpu_fault ? TRS80_CPU_FAULT : TRS80_OK;
}
int mem_read(int address) { return trs80_read((uint16_t)address); }
void mem_write(int address, int value) { trs80_write((uint16_t)address, (uint8_t)value); }
int mem_read_word(int address) { return mem_read(address) | (mem_read(address + 1) << 8); }
void mem_write_word(int address, int value)
{ mem_write(address, value); mem_write(address + 1, value >> 8); }
int mem_block_transfer(Ushort dest, Ushort source, int direction, Ushort count)
{
    int last;
    do {
        last = mem_read(source);
        mem_write(dest, last);
        dest = (Ushort)(dest + direction);
        source = (Ushort)(source + direction);
    } while (--count != 0u);
    return last;
}
int z80_in(int port) { return trs80_in((uint8_t)port); }
void z80_out(int port, int value) { trs80_out((uint8_t)port, (uint8_t)value); }
void trs_get_event(int wait) { (void)wait; }
void trs_reset(int poweron)
{
    if (poweron || trs_model >= 3) trs80_reset();
    else { z80_state.nmi = 1; z80_state.nmi_seen = 0; }
}
void trs_reset_button_interrupt(int state)
{ z80_state.nmi = state; if (!state) z80_state.nmi_seen = 0; }
trs_event_func trs_event_scheduled(void) { return NULL; }
void trs_do_event(void) { z80_state.sched = 0u; }
int disassemble(unsigned short pc) { (void)pc; return 0; }
void error(const char *fmt, ...) { (void)fmt; cpu_fault = 1; }
#define DENY_TRAP(name) void name(void) { error("unsupported xtrs host trap"); }
DENY_TRAP(do_emt_system) DENY_TRAP(do_emt_mouse)
DENY_TRAP(do_emt_getddir) DENY_TRAP(do_emt_setddir)
DENY_TRAP(do_emt_open) DENY_TRAP(do_emt_close)
DENY_TRAP(do_emt_read) DENY_TRAP(do_emt_write)
DENY_TRAP(do_emt_lseek) DENY_TRAP(do_emt_strerror)
DENY_TRAP(do_emt_time) DENY_TRAP(do_emt_opendir)
DENY_TRAP(do_emt_closedir) DENY_TRAP(do_emt_readdir)
DENY_TRAP(do_emt_chdir) DENY_TRAP(do_emt_getcwd)
DENY_TRAP(do_emt_misc) DENY_TRAP(do_emt_ftruncate)
DENY_TRAP(do_emt_opendisk) DENY_TRAP(do_emt_closedisk)

static trs80_result_t parse_cmd(const uint8_t *data, size_t size, int commit,
                                uint16_t *entry)
{
    size_t pos = 0u, loaded = 0u;
    while (pos < size) {
        unsigned type = data[pos++], count;
        uint16_t address;
        if (pos == size) return TRS80_TRUNCATED;
        count = data[pos++];
        if (type == 1u) {
            /* The address and data length is encoded modulo 256. */
            count = (count + 254u) % 256u;
            if (count == 0u) count = 256u;
            if (size - pos < count + 2u) return TRS80_TRUNCATED;
            address = (uint16_t)(data[pos] | ((unsigned)data[pos + 1u] << 8));
            pos += 2u;
            for (unsigned i = 0u; i < count; ++i) {
                uint16_t target = (uint16_t)(address + i);
                if (!writable(target)) return TRS80_PROTECTED;
                if (commit) trs80_write(target, data[pos + i]);
            }
            pos += count;
            loaded += count;
        } else if (type == 2u) {
            if (count != 2u) return TRS80_INVALID;
            if (size - pos < 2u) return TRS80_TRUNCATED;
            *entry = (uint16_t)(data[pos] | ((unsigned)data[pos + 1u] << 8));
            return loaded == 0u ? TRS80_NO_ENTRY : TRS80_OK;
        } else if (type == 0u || type == 5u || type == 7u) {
            if (count == 0u) count = 256u;
            if (size - pos < count) return TRS80_TRUNCATED;
            pos += count;
        } else {
            return TRS80_UNSUPPORTED;
        }
    }
    return TRS80_NO_ENTRY;
}
trs80_result_t trs80_load_cmd(const uint8_t *data, size_t size)
{
    uint16_t entry = 0u;
    trs80_result_t result;
    if (data == NULL || size == 0u || size > TRS80_MAX_CMD_BYTES) return TRS80_INVALID;
    result = parse_cmd(data, size, 0, &entry);
    if (result != TRS80_OK) return result;
    result = parse_cmd(data, size, 1, &entry);
    if (result != TRS80_OK) return result;
    memset(&z80_state, 0, sizeof(z80_state));
    z80_reset();
    REG_PC = entry;
    cpu_fault = 0;
    irq_latch = 0u;
    x_poll_count = 0;
    trs80_release_keys();
    set_speed((mode_image & 0x40u) != 0u);
    return TRS80_OK;
}
