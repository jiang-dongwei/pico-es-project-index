/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROPICOES_SPACE_INVADERS_BOARD_H
#define RETROPICOES_SPACE_INVADERS_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arcade_machine.h"
#include "space_invaders_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPACE_INVADERS_CPU_HZ 1996800u
#define SPACE_INVADERS_HALF_FRAMES_PER_SECOND 120u
#define SPACE_INVADERS_ROM_COUNT 4u
#define SPACE_INVADERS_ROM_CHIP_SIZE 0x0800u
#define SPACE_INVADERS_ROM_BYTES 0x2000u
#define SPACE_INVADERS_ROM_ROOT "/roms/SPACEINVADERS"
#define SPACE_INVADERS_RAM_START 0x2000u
#define SPACE_INVADERS_VRAM_START 0x2400u
#define SPACE_INVADERS_ADDRESS_SPACE 0x4000u

enum {
    SPACE_INVADERS_INPUT_COIN = 1u << 0,
    SPACE_INVADERS_INPUT_START_2P = 1u << 1,
    SPACE_INVADERS_INPUT_START_1P = 1u << 2,
    SPACE_INVADERS_INPUT_ALWAYS_ON = 1u << 3,
    SPACE_INVADERS_INPUT_FIRE = 1u << 4,
    SPACE_INVADERS_INPUT_LEFT = 1u << 5,
    SPACE_INVADERS_INPUT_RIGHT = 1u << 6
};

typedef struct {
    const uint8_t *data[SPACE_INVADERS_ROM_COUNT];
    size_t size[SPACE_INVADERS_ROM_COUNT];
} space_invaders_rom_set_t;

typedef enum {
    SPACE_INVADERS_ROM_OK = 0,
    SPACE_INVADERS_ROM_MISSING,
    SPACE_INVADERS_ROM_WRONG_SIZE
} space_invaders_rom_status_t;

typedef struct {
    space_invaders_rom_status_t status;
    unsigned chip_index;
    size_t actual_size;
    size_t expected_size;
} space_invaders_rom_result_t;

typedef enum {
    SPACE_INVADERS_RUN_OK = 0,
    SPACE_INVADERS_RUN_ROM_NOT_READY,
    SPACE_INVADERS_RUN_CPU_STALLED
} space_invaders_run_status_t;

typedef struct {
    space_invaders_run_status_t status;
    uint32_t scheduled_cycles;
    uint32_t executed_cycles;
    uint32_t instructions;
    uint8_t interrupt_opcode;
    bool frame_complete;
} space_invaders_half_frame_result_t;

typedef struct {
    space_invaders_cpu_t cpu;
    arcade_cycle_scheduler_t scheduler;
    uint8_t memory[SPACE_INVADERS_ADDRESS_SPACE];
    uint8_t input0;
    uint8_t input1;
    uint8_t input2;
    uint8_t shift_msb;
    uint8_t shift_lsb;
    uint8_t shift_amount;
    uint8_t sound_port3;
    uint8_t sound_port5;
    uint8_t sound_rising3;
    uint8_t sound_rising5;
    uint8_t sound_falling3;
    uint8_t sound_falling5;
    uint8_t next_interrupt;
    uint32_t watchdog_writes;
    uint32_t rom_crc32;
    uint64_t elapsed_cycles;
    uint64_t frame_count;
    bool rom_ready;
} space_invaders_board_t;

extern const char *const
    space_invaders_rom_names[SPACE_INVADERS_ROM_COUNT];

bool space_invaders_board_init(space_invaders_board_t *board);
space_invaders_rom_result_t space_invaders_validate_roms(
    const space_invaders_rom_set_t *roms);
space_invaders_rom_result_t space_invaders_board_load_roms(
    space_invaders_board_t *board, const space_invaders_rom_set_t *roms);
bool space_invaders_rom_path(unsigned chip_index, char *output,
                             size_t capacity);
bool space_invaders_rom_error_text(const space_invaders_rom_result_t *result,
                                   char *output, size_t capacity);
void space_invaders_board_set_inputs(space_invaders_board_t *board,
                                     uint8_t input1, uint8_t input2);
void space_invaders_board_take_sound_edges(space_invaders_board_t *board,
                                           uint8_t *rising3,
                                           uint8_t *falling3,
                                           uint8_t *rising5,
                                           uint8_t *falling5);
space_invaders_half_frame_result_t space_invaders_board_run_half_frame(
    space_invaders_board_t *board);

#ifdef __cplusplus
}
#endif
#endif
