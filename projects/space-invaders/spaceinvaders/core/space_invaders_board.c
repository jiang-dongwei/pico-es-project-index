/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "space_invaders_board.h"

#include <stdio.h>
#include <string.h>

const char *const space_invaders_rom_names[SPACE_INVADERS_ROM_COUNT] = {
    "invaders.h", "invaders.g", "invaders.f", "invaders.e"
};

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
    while (size-- != 0u) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}

static uint8_t board_read(void *context, uint16_t address)
{
    space_invaders_board_t *board = (space_invaders_board_t *)context;
    if (address < SPACE_INVADERS_ADDRESS_SPACE)
        return board->memory[address];
    if (address < 0x6000u)
        return board->memory[address - 0x2000u];
    return 0u;
}

static void board_write(void *context, uint16_t address, uint8_t value)
{
    space_invaders_board_t *board = (space_invaders_board_t *)context;
    if (address >= SPACE_INVADERS_RAM_START &&
        address < SPACE_INVADERS_ADDRESS_SPACE)
        board->memory[address] = value;
}

static uint8_t board_port_in(void *context, uint8_t port)
{
    space_invaders_board_t *board = (space_invaders_board_t *)context;
    switch (port) {
    case 0u:
        return board->input0;
    case 1u:
        return board->input1;
    case 2u:
        return board->input2;
    case 3u: {
        uint16_t value = (uint16_t)((uint16_t)board->shift_msb << 8) |
                         board->shift_lsb;
        return (uint8_t)(value >> (8u - board->shift_amount));
    }
    default:
        return 0xffu;
    }
}

static void update_sound_edges(uint8_t old_value, uint8_t new_value,
                               uint8_t *rising, uint8_t *falling)
{
    *rising |= (uint8_t)(new_value & (uint8_t)~old_value);
    *falling |= (uint8_t)(old_value & (uint8_t)~new_value);
}

static void board_port_out(void *context, uint8_t port, uint8_t value)
{
    space_invaders_board_t *board = (space_invaders_board_t *)context;
    switch (port) {
    case 2u:
        board->shift_amount = value & 7u;
        break;
    case 3u:
        update_sound_edges(board->sound_port3, value,
                           &board->sound_rising3, &board->sound_falling3);
        board->sound_port3 = value;
        break;
    case 4u:
        board->shift_lsb = board->shift_msb;
        board->shift_msb = value;
        break;
    case 5u:
        update_sound_edges(board->sound_port5, value,
                           &board->sound_rising5, &board->sound_falling5);
        board->sound_port5 = value;
        break;
    case 6u:
        ++board->watchdog_writes;
        break;
    default:
        break;
    }
}

bool space_invaders_board_init(space_invaders_board_t *board)
{
    arcade_cpu_bus_t bus;
    if (board == NULL)
        return false;
    memset(board, 0, sizeof(*board));
    board->input0 = 0x0fu;
    board->input1 = SPACE_INVADERS_INPUT_ALWAYS_ON;
    board->next_interrupt = 0xcfu;
    if (!arcade_cycle_scheduler_init(&board->scheduler,
                                     SPACE_INVADERS_CPU_HZ,
                                     SPACE_INVADERS_HALF_FRAMES_PER_SECOND))
        return false;
    bus.context = board;
    bus.read8 = board_read;
    bus.write8 = board_write;
    bus.port_in = board_port_in;
    bus.port_out = board_port_out;
    return space_invaders_cpu_init(&board->cpu, &bus);
}

space_invaders_rom_result_t space_invaders_validate_roms(
    const space_invaders_rom_set_t *roms)
{
    space_invaders_rom_result_t result = {
        SPACE_INVADERS_ROM_OK, 0u, SPACE_INVADERS_ROM_CHIP_SIZE,
        SPACE_INVADERS_ROM_CHIP_SIZE
    };
    for (unsigned chip = 0; chip < SPACE_INVADERS_ROM_COUNT; ++chip) {
        if (roms == NULL || roms->data[chip] == NULL) {
            result.status = SPACE_INVADERS_ROM_MISSING;
            result.chip_index = chip;
            result.actual_size = 0u;
            return result;
        }
        if (roms->size[chip] != SPACE_INVADERS_ROM_CHIP_SIZE) {
            result.status = SPACE_INVADERS_ROM_WRONG_SIZE;
            result.chip_index = chip;
            result.actual_size = roms->size[chip];
            return result;
        }
    }
    return result;
}

space_invaders_rom_result_t space_invaders_board_load_roms(
    space_invaders_board_t *board, const space_invaders_rom_set_t *roms)
{
    space_invaders_rom_result_t result = space_invaders_validate_roms(roms);
    if (result.status != SPACE_INVADERS_ROM_OK || board == NULL)
        return result;

    memset(board->memory, 0, sizeof(board->memory));
    board->rom_crc32 = 0xffffffffu;
    for (unsigned chip = 0; chip < SPACE_INVADERS_ROM_COUNT; ++chip) {
        size_t address = chip * SPACE_INVADERS_ROM_CHIP_SIZE;
        memcpy(&board->memory[address], roms->data[chip],
               SPACE_INVADERS_ROM_CHIP_SIZE);
        board->rom_crc32 = crc32_update(board->rom_crc32, roms->data[chip],
                                        SPACE_INVADERS_ROM_CHIP_SIZE);
    }
    board->rom_crc32 ^= 0xffffffffu;
    board->rom_ready = true;
    board->elapsed_cycles = 0u;
    board->frame_count = 0u;
    board->next_interrupt = 0xcfu;
    arcade_cycle_scheduler_init(&board->scheduler, SPACE_INVADERS_CPU_HZ,
                                SPACE_INVADERS_HALF_FRAMES_PER_SECOND);
    space_invaders_cpu_reset(&board->cpu);
    return result;
}

bool space_invaders_rom_path(unsigned chip_index, char *output,
                             size_t capacity)
{
    int length;
    if (chip_index >= SPACE_INVADERS_ROM_COUNT || output == NULL ||
        capacity == 0u)
        return false;
    length = snprintf(output, capacity, "%s/%s", SPACE_INVADERS_ROM_ROOT,
                      space_invaders_rom_names[chip_index]);
    return length > 0 && (size_t)length < capacity;
}

bool space_invaders_rom_error_text(const space_invaders_rom_result_t *result,
                                   char *output, size_t capacity)
{
    char path[64];
    int length;
    if (result == NULL || output == NULL || capacity == 0u ||
        result->status == SPACE_INVADERS_ROM_OK ||
        !space_invaders_rom_path(result->chip_index, path, sizeof(path)))
        return false;
    if (result->status == SPACE_INVADERS_ROM_MISSING) {
        length = snprintf(output, capacity,
                          "ROM MISSING: %s (EXPECTED %u BYTES)", path,
                          SPACE_INVADERS_ROM_CHIP_SIZE);
    } else {
        length = snprintf(output, capacity,
                          "ROM SIZE ERROR: %s (GOT %u, EXPECTED %u)", path,
                          (unsigned)result->actual_size,
                          SPACE_INVADERS_ROM_CHIP_SIZE);
    }
    return length > 0 && (size_t)length < capacity;
}

void space_invaders_board_set_inputs(space_invaders_board_t *board,
                                     uint8_t input1, uint8_t input2)
{
    if (board == NULL)
        return;
    board->input1 = (uint8_t)(input1 | SPACE_INVADERS_INPUT_ALWAYS_ON);
    board->input2 = input2;
}

void space_invaders_board_take_sound_edges(space_invaders_board_t *board,
                                           uint8_t *rising3,
                                           uint8_t *falling3,
                                           uint8_t *rising5,
                                           uint8_t *falling5)
{
    if (board == NULL)
        return;
    if (rising3 != NULL) *rising3 = board->sound_rising3;
    if (falling3 != NULL) *falling3 = board->sound_falling3;
    if (rising5 != NULL) *rising5 = board->sound_rising5;
    if (falling5 != NULL) *falling5 = board->sound_falling5;
    board->sound_rising3 = 0u;
    board->sound_falling3 = 0u;
    board->sound_rising5 = 0u;
    board->sound_falling5 = 0u;
}

space_invaders_half_frame_result_t space_invaders_board_run_half_frame(
    space_invaders_board_t *board)
{
    space_invaders_half_frame_result_t result;
    uint64_t start;
    uint64_t target;

    memset(&result, 0, sizeof(result));
    if (board == NULL || !board->rom_ready) {
        result.status = SPACE_INVADERS_RUN_ROM_NOT_READY;
        return result;
    }
    result.status = SPACE_INVADERS_RUN_OK;
    result.scheduled_cycles = arcade_cycle_scheduler_next(&board->scheduler);
    start = board->elapsed_cycles;
    target = board->scheduler.scheduled_cycles;
    while (board->elapsed_cycles < target) {
        unsigned long elapsed = space_invaders_cpu_step(&board->cpu);
        if (elapsed == 0ul) {
            if (board->cpu.cpu.halted) {
                board->elapsed_cycles = target;
                break;
            }
            result.status = SPACE_INVADERS_RUN_CPU_STALLED;
            break;
        }
        board->elapsed_cycles += elapsed;
        ++result.instructions;
    }
    result.executed_cycles = (uint32_t)(board->elapsed_cycles - start);
    result.interrupt_opcode = board->next_interrupt;
    space_invaders_cpu_interrupt(&board->cpu, board->next_interrupt);
    if (board->next_interrupt == 0xd7u) {
        result.frame_complete = true;
        ++board->frame_count;
        board->next_interrupt = 0xcfu;
    } else {
        board->next_interrupt = 0xd7u;
    }
    return result;
}
