/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "space_invaders_board.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;
static uint8_t rom_data[SPACE_INVADERS_ROM_COUNT]
                       [SPACE_INVADERS_ROM_CHIP_SIZE];

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static space_invaders_rom_set_t make_rom_set(void)
{
    space_invaders_rom_set_t roms;
    for (unsigned chip = 0; chip < SPACE_INVADERS_ROM_COUNT; ++chip) {
        memset(rom_data[chip], (int)(0x10u + chip),
               SPACE_INVADERS_ROM_CHIP_SIZE);
        roms.data[chip] = rom_data[chip];
        roms.size[chip] = SPACE_INVADERS_ROM_CHIP_SIZE;
    }
    return roms;
}

static void test_rom_gate_and_map(void)
{
    space_invaders_board_t board;
    space_invaders_rom_set_t roms = make_rom_set();
    space_invaders_rom_result_t result;
    char text[160];

    CHECK(space_invaders_board_init(&board));
    result = space_invaders_validate_roms(NULL);
    CHECK(result.status == SPACE_INVADERS_ROM_MISSING &&
          result.chip_index == 0u);
    roms.data[2] = NULL;
    result = space_invaders_validate_roms(&roms);
    CHECK(result.status == SPACE_INVADERS_ROM_MISSING &&
          result.chip_index == 2u);
    CHECK(space_invaders_rom_error_text(&result, text, sizeof(text)));
    CHECK(strcmp(text,
        "ROM MISSING: /roms/SPACEINVADERS/invaders.f (EXPECTED 2048 BYTES)")
          == 0);
    roms = make_rom_set();
    roms.size[1] = SPACE_INVADERS_ROM_CHIP_SIZE - 1u;
    result = space_invaders_validate_roms(&roms);
    CHECK(result.status == SPACE_INVADERS_ROM_WRONG_SIZE &&
          result.chip_index == 1u &&
          result.expected_size == SPACE_INVADERS_ROM_CHIP_SIZE);
    CHECK(space_invaders_rom_error_text(&result, text, sizeof(text)));
    CHECK(strstr(text, "/roms/SPACEINVADERS/invaders.g") != NULL);
    CHECK(strstr(text, "GOT 2047, EXPECTED 2048") != NULL);
    CHECK(space_invaders_rom_path(3u, text, sizeof(text)));
    CHECK(strcmp(text, "/roms/SPACEINVADERS/invaders.e") == 0);
    CHECK(!space_invaders_rom_path(4u, text, sizeof(text)));
    CHECK(!space_invaders_rom_path(0u, text, 4u));

    roms = make_rom_set();
    result = space_invaders_board_load_roms(&board, &roms);
    CHECK(result.status == SPACE_INVADERS_ROM_OK && board.rom_ready);
    CHECK(board.memory[0x0000] == 0x10u);
    CHECK(board.memory[0x0800] == 0x11u);
    CHECK(board.memory[0x1000] == 0x12u);
    CHECK(board.memory[0x1800] == 0x13u);
    CHECK(board.rom_crc32 != 0u);

    board.cpu.cpu.write_byte(board.cpu.cpu.userdata, 0x0100u, 0xaau);
    CHECK(board.memory[0x0100] == 0x10u);
    board.cpu.cpu.write_byte(board.cpu.cpu.userdata, 0x2000u, 0x5au);
    CHECK(board.memory[0x2000] == 0x5au);
    CHECK(board.cpu.cpu.read_byte(board.cpu.cpu.userdata, 0x4000u) == 0x5au);
    CHECK(board.cpu.cpu.read_byte(board.cpu.cpu.userdata, 0x6000u) == 0u);
}

static void test_ports_and_sound_edges(void)
{
    space_invaders_board_t board;
    uint8_t rise3, fall3, rise5, fall5;

    CHECK(space_invaders_board_init(&board));
    space_invaders_board_set_inputs(
        &board, SPACE_INVADERS_INPUT_LEFT | SPACE_INVADERS_INPUT_FIRE,
        SPACE_INVADERS_INPUT_RIGHT | 2u);
    CHECK(board.cpu.cpu.port_in(board.cpu.cpu.userdata, 1u) ==
          (SPACE_INVADERS_INPUT_LEFT | SPACE_INVADERS_INPUT_FIRE |
           SPACE_INVADERS_INPUT_ALWAYS_ON));
    CHECK(board.cpu.cpu.port_in(board.cpu.cpu.userdata, 2u) ==
          (SPACE_INVADERS_INPUT_RIGHT | 2u));

    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 4u, 0xaau);
    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 4u, 0xccu);
    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 2u, 0u);
    CHECK(board.cpu.cpu.port_in(board.cpu.cpu.userdata, 3u) == 0xccu);
    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 2u, 2u);
    CHECK(board.cpu.cpu.port_in(board.cpu.cpu.userdata, 3u) == 0x32u);

    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 3u, 0x05u);
    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 3u, 0x04u);
    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 5u, 0x12u);
    board.cpu.cpu.port_out(board.cpu.cpu.userdata, 6u, 0u);
    space_invaders_board_take_sound_edges(
        &board, &rise3, &fall3, &rise5, &fall5);
    CHECK(rise3 == 0x05u && fall3 == 0x01u);
    CHECK(rise5 == 0x12u && fall5 == 0u);
    CHECK(board.watchdog_writes == 1u);
    space_invaders_board_take_sound_edges(
        &board, &rise3, &fall3, &rise5, &fall5);
    CHECK((rise3 | fall3 | rise5 | fall5) == 0u);
}

static void test_half_frame_scheduler(void)
{
    space_invaders_board_t board;
    space_invaders_rom_set_t roms = make_rom_set();
    space_invaders_half_frame_result_t first;
    space_invaders_half_frame_result_t second;

    CHECK(space_invaders_board_init(&board));
    first = space_invaders_board_run_half_frame(&board);
    CHECK(first.status == SPACE_INVADERS_RUN_ROM_NOT_READY);

    /* DI; JMP 0001 keeps the generated test ROM deterministic. */
    rom_data[0][0] = 0xf3u;
    rom_data[0][1] = 0xc3u;
    rom_data[0][2] = 0x01u;
    rom_data[0][3] = 0x00u;
    CHECK(space_invaders_board_load_roms(&board, &roms).status ==
          SPACE_INVADERS_ROM_OK);
    first = space_invaders_board_run_half_frame(&board);
    second = space_invaders_board_run_half_frame(&board);
    CHECK(first.status == SPACE_INVADERS_RUN_OK &&
          first.scheduled_cycles == 16640u &&
          first.executed_cycles >= first.scheduled_cycles &&
          first.interrupt_opcode == 0xcfu && !first.frame_complete);
    CHECK(second.status == SPACE_INVADERS_RUN_OK &&
          second.interrupt_opcode == 0xd7u && second.frame_complete);
    CHECK(board.frame_count == 1u);
    CHECK(board.scheduler.scheduled_cycles == 33280u);
    CHECK(board.elapsed_cycles >= board.scheduler.scheduled_cycles);
}

int main(void)
{
    CHECK(strcmp(space_invaders_rom_names[0], "invaders.h") == 0);
    CHECK(strcmp(space_invaders_rom_names[3], "invaders.e") == 0);
    test_rom_gate_and_map();
    test_ports_and_sound_edges();
    test_half_frame_scheduler();
    if (failures != 0u) {
        fprintf(stderr, "%u Space Invaders board checks failed\n", failures);
        return 1;
    }
    puts("Space Invaders board checks passed");
    return 0;
}
