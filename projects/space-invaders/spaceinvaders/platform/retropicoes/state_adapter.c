/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "state_adapter.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define STATE_HEADER_SIZE 20u
#define STATE_RAM_SIZE (SPACE_INVADERS_ADDRESS_SPACE - SPACE_INVADERS_RAM_START)
#define STATE_PAYLOAD_SIZE (24u + 13u + 40u + STATE_RAM_SIZE)
#define STATE_TOTAL_SIZE (STATE_HEADER_SIZE + STATE_PAYLOAD_SIZE)

static void put16(uint8_t **cursor, uint16_t value)
{
    *(*cursor)++ = (uint8_t)value;
    *(*cursor)++ = (uint8_t)(value >> 8);
}

static void put32(uint8_t **cursor, uint32_t value)
{
    for (unsigned shift = 0; shift < 32u; shift += 8u)
        *(*cursor)++ = (uint8_t)(value >> shift);
}

static void put64(uint8_t **cursor, uint64_t value)
{
    for (unsigned shift = 0; shift < 64u; shift += 8u)
        *(*cursor)++ = (uint8_t)(value >> shift);
}

static uint16_t get16(const uint8_t **cursor)
{
    uint16_t value = (*cursor)[0] | ((uint16_t)(*cursor)[1] << 8);
    *cursor += 2;
    return value;
}

static uint32_t get32(const uint8_t **cursor)
{
    uint32_t value = 0u;
    for (unsigned shift = 0; shift < 32u; shift += 8u)
        value |= (uint32_t)*(*cursor)++ << shift;
    return value;
}

static uint64_t get64(const uint8_t **cursor)
{
    uint64_t value = 0u;
    for (unsigned shift = 0; shift < 64u; shift += 8u)
        value |= (uint64_t)*(*cursor)++ << shift;
    return value;
}

static uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffu;
    while (size-- != 0u) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

static uint8_t pack_flags(const i8080 *cpu)
{
    return (uint8_t)((cpu->sf ? 0x80u : 0u) |
                     (cpu->zf ? 0x40u : 0u) |
                     (cpu->hf ? 0x10u : 0u) |
                     (cpu->pf ? 0x04u : 0u) |
                     (cpu->cf ? 0x01u : 0u));
}

size_t space_invaders_state_size(void)
{
    return STATE_TOTAL_SIZE;
}

space_invaders_state_status_t space_invaders_state_save(
    const space_invaders_board_t *board, uint8_t *output, size_t capacity,
    size_t *written)
{
    uint8_t *cursor;
    uint8_t *payload;
    uint32_t payload_crc;
    if (written != NULL) *written = 0u;
    if (board == NULL || !board->rom_ready || output == NULL ||
        capacity < STATE_TOTAL_SIZE)
        return SPACE_INVADERS_STATE_BUFFER_TOO_SMALL;

    cursor = output;
    memcpy(cursor, "SIV1", 4u); cursor += 4;
    put16(&cursor, SPACE_INVADERS_STATE_VERSION);
    put16(&cursor, STATE_HEADER_SIZE);
    put32(&cursor, STATE_PAYLOAD_SIZE);
    put32(&cursor, board->rom_crc32);
    put32(&cursor, 0u);
    payload = cursor;

    put16(&cursor, board->cpu.cpu.pc);
    put16(&cursor, board->cpu.cpu.sp);
    *cursor++ = board->cpu.cpu.a;
    *cursor++ = board->cpu.cpu.b;
    *cursor++ = board->cpu.cpu.c;
    *cursor++ = board->cpu.cpu.d;
    *cursor++ = board->cpu.cpu.e;
    *cursor++ = board->cpu.cpu.h;
    *cursor++ = board->cpu.cpu.l;
    *cursor++ = pack_flags(&board->cpu.cpu);
    *cursor++ = board->cpu.cpu.halted ? 1u : 0u;
    *cursor++ = board->cpu.cpu.interrupt_pending ? 1u : 0u;
    *cursor++ = board->cpu.cpu.interrupt_vector;
    *cursor++ = board->cpu.cpu.interrupt_delay;
    put64(&cursor, (uint64_t)board->cpu.cpu.cyc);

    *cursor++ = board->input0;
    *cursor++ = board->input1;
    *cursor++ = board->input2;
    *cursor++ = board->shift_msb;
    *cursor++ = board->shift_lsb;
    *cursor++ = board->shift_amount;
    *cursor++ = board->sound_port3;
    *cursor++ = board->sound_port5;
    *cursor++ = board->sound_rising3;
    *cursor++ = board->sound_rising5;
    *cursor++ = board->sound_falling3;
    *cursor++ = board->sound_falling5;
    *cursor++ = board->next_interrupt;
    put32(&cursor, board->watchdog_writes);
    put64(&cursor, board->elapsed_cycles);
    put64(&cursor, board->frame_count);
    put32(&cursor, board->scheduler.remainder);
    put64(&cursor, board->scheduler.scheduled_cycles);
    put64(&cursor, board->scheduler.tick_index);
    memcpy(cursor, &board->memory[SPACE_INVADERS_RAM_START], STATE_RAM_SIZE);
    cursor += STATE_RAM_SIZE;

    payload_crc = crc32(payload, STATE_PAYLOAD_SIZE);
    output[16] = (uint8_t)payload_crc;
    output[17] = (uint8_t)(payload_crc >> 8);
    output[18] = (uint8_t)(payload_crc >> 16);
    output[19] = (uint8_t)(payload_crc >> 24);
    if ((size_t)(cursor - output) != STATE_TOTAL_SIZE)
        return SPACE_INVADERS_STATE_BAD_SIZE;
    if (written != NULL) *written = STATE_TOTAL_SIZE;
    return SPACE_INVADERS_STATE_OK;
}

space_invaders_state_status_t space_invaders_state_load(
    space_invaders_board_t *board, const uint8_t *data, size_t size)
{
    const uint8_t *cursor;
    uint16_t version;
    uint16_t header_size;
    uint32_t payload_size;
    uint32_t rom_crc;
    uint32_t expected_crc;
    uint64_t cpu_cycles;
    uint8_t flags;
    if (board == NULL || data == NULL || size < STATE_HEADER_SIZE)
        return SPACE_INVADERS_STATE_BAD_SIZE;
    if (memcmp(data, "SIV1", 4u) != 0)
        return SPACE_INVADERS_STATE_BAD_MAGIC;
    cursor = data + 4;
    version = get16(&cursor);
    header_size = get16(&cursor);
    payload_size = get32(&cursor);
    rom_crc = get32(&cursor);
    expected_crc = get32(&cursor);
    if (version != SPACE_INVADERS_STATE_VERSION)
        return SPACE_INVADERS_STATE_BAD_VERSION;
    if (header_size != STATE_HEADER_SIZE || payload_size != STATE_PAYLOAD_SIZE ||
        size != STATE_TOTAL_SIZE)
        return SPACE_INVADERS_STATE_BAD_SIZE;
    if (!board->rom_ready || rom_crc != board->rom_crc32)
        return SPACE_INVADERS_STATE_ROM_MISMATCH;
    if (crc32(data + STATE_HEADER_SIZE, STATE_PAYLOAD_SIZE) != expected_crc)
        return SPACE_INVADERS_STATE_BAD_CRC;
    cursor = data + STATE_HEADER_SIZE + 16u;
    cpu_cycles = get64(&cursor);
    if (cpu_cycles > ULONG_MAX)
        return SPACE_INVADERS_STATE_VALUE_OUT_OF_RANGE;

    cursor = data + STATE_HEADER_SIZE;
    board->cpu.cpu.pc = get16(&cursor);
    board->cpu.cpu.sp = get16(&cursor);
    board->cpu.cpu.a = *cursor++;
    board->cpu.cpu.b = *cursor++;
    board->cpu.cpu.c = *cursor++;
    board->cpu.cpu.d = *cursor++;
    board->cpu.cpu.e = *cursor++;
    board->cpu.cpu.h = *cursor++;
    board->cpu.cpu.l = *cursor++;
    flags = *cursor++;
    board->cpu.cpu.sf = (flags & 0x80u) != 0u;
    board->cpu.cpu.zf = (flags & 0x40u) != 0u;
    board->cpu.cpu.hf = (flags & 0x10u) != 0u;
    board->cpu.cpu.pf = (flags & 0x04u) != 0u;
    board->cpu.cpu.cf = (flags & 0x01u) != 0u;
    board->cpu.cpu.halted = *cursor++ != 0u;
    board->cpu.cpu.interrupt_pending = *cursor++ != 0u;
    board->cpu.cpu.interrupt_vector = *cursor++;
    board->cpu.cpu.interrupt_delay = *cursor++;
    cpu_cycles = get64(&cursor);
    board->cpu.cpu.cyc = (unsigned long)cpu_cycles;

    board->input0 = *cursor++;
    board->input1 = *cursor++;
    board->input2 = *cursor++;
    board->shift_msb = *cursor++;
    board->shift_lsb = *cursor++;
    board->shift_amount = *cursor++;
    board->sound_port3 = *cursor++;
    board->sound_port5 = *cursor++;
    board->sound_rising3 = *cursor++;
    board->sound_rising5 = *cursor++;
    board->sound_falling3 = *cursor++;
    board->sound_falling5 = *cursor++;
    board->next_interrupt = *cursor++;
    board->watchdog_writes = get32(&cursor);
    board->elapsed_cycles = get64(&cursor);
    board->frame_count = get64(&cursor);
    board->scheduler.remainder = get32(&cursor);
    board->scheduler.scheduled_cycles = get64(&cursor);
    board->scheduler.tick_index = get64(&cursor);
    memcpy(&board->memory[SPACE_INVADERS_RAM_START], cursor, STATE_RAM_SIZE);
    board->scheduler.clock_hz = SPACE_INVADERS_CPU_HZ;
    board->scheduler.ticks_per_second =
        SPACE_INVADERS_HALF_FRAMES_PER_SECOND;
    return SPACE_INVADERS_STATE_OK;
}

bool space_invaders_state_path(char *output, size_t capacity)
{
    static const char path[] = SPACE_INVADERS_SAVE_ROOT "/state.bin";
    if (output == NULL || capacity < sizeof(path)) return false;
    memcpy(output, path, sizeof(path));
    return true;
}
