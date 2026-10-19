/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "state_adapter.h"

#include <stdio.h>
#include <stdlib.h>
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

static space_invaders_rom_set_t rom_set(uint8_t seed)
{
    space_invaders_rom_set_t roms;
    for (unsigned chip = 0; chip < SPACE_INVADERS_ROM_COUNT; ++chip) {
        memset(rom_data[chip], seed + chip, sizeof(rom_data[chip]));
        roms.data[chip] = rom_data[chip];
        roms.size[chip] = sizeof(rom_data[chip]);
    }
    return roms;
}

int main(void)
{
    static space_invaders_board_t source;
    static space_invaders_board_t restored;
    space_invaders_rom_set_t roms = rom_set(0x20u);
    size_t size = space_invaders_state_size();
    size_t written = 0u;
    uint8_t *data = (uint8_t *)malloc(size);
    char path[64];

    CHECK(data != NULL);
    if (data == NULL) return 1;
    CHECK(space_invaders_board_init(&source));
    CHECK(space_invaders_board_init(&restored));
    CHECK(space_invaders_board_load_roms(&source, &roms).status ==
          SPACE_INVADERS_ROM_OK);
    CHECK(space_invaders_board_load_roms(&restored, &roms).status ==
          SPACE_INVADERS_ROM_OK);
    source.cpu.cpu.pc = 0x1234u;
    source.cpu.cpu.sp = 0x3ff0u;
    source.cpu.cpu.a = 0xa1u;
    source.cpu.cpu.zf = true;
    source.cpu.cpu.cf = true;
    source.cpu.cpu.cyc = 1234567ul;
    source.shift_msb = 0xccu;
    source.shift_lsb = 0xaau;
    source.shift_amount = 3u;
    source.sound_port3 = 5u;
    source.next_interrupt = 0xd7u;
    source.elapsed_cycles = 998400u;
    source.frame_count = 30u;
    source.scheduler.remainder = 7u;
    source.scheduler.scheduled_cycles = 998400u;
    source.scheduler.tick_index = 60u;
    memset(&source.memory[SPACE_INVADERS_RAM_START], 0x5a,
           SPACE_INVADERS_ADDRESS_SPACE - SPACE_INVADERS_RAM_START);

    CHECK(space_invaders_state_save(&source, data, size, &written) ==
          SPACE_INVADERS_STATE_OK);
    CHECK(written == size);
    CHECK(space_invaders_state_load(&restored, data, size) ==
          SPACE_INVADERS_STATE_OK);
    CHECK(restored.cpu.cpu.pc == source.cpu.cpu.pc &&
          restored.cpu.cpu.sp == source.cpu.cpu.sp &&
          restored.cpu.cpu.a == source.cpu.cpu.a);
    CHECK(restored.cpu.cpu.zf && restored.cpu.cpu.cf);
    CHECK(restored.cpu.cpu.read_byte != NULL &&
          restored.cpu.cpu.userdata == &restored);
    CHECK(restored.shift_msb == 0xccu && restored.shift_lsb == 0xaau &&
          restored.next_interrupt == 0xd7u);
    CHECK(restored.frame_count == 30u &&
          restored.scheduler.tick_index == 60u);
    CHECK(restored.memory[SPACE_INVADERS_RAM_START] == 0x5au &&
          restored.memory[SPACE_INVADERS_ADDRESS_SPACE - 1u] == 0x5au);

    data[size - 1u] ^= 1u;
    CHECK(space_invaders_state_load(&restored, data, size) ==
          SPACE_INVADERS_STATE_BAD_CRC);
    data[size - 1u] ^= 1u;
    data[4] = 2u;
    CHECK(space_invaders_state_load(&restored, data, size) ==
          SPACE_INVADERS_STATE_BAD_VERSION);
    data[4] = SPACE_INVADERS_STATE_VERSION;
    restored.rom_crc32 ^= 1u;
    CHECK(space_invaders_state_load(&restored, data, size) ==
          SPACE_INVADERS_STATE_ROM_MISMATCH);
    CHECK(space_invaders_state_load(&restored, data, size - 1u) ==
          SPACE_INVADERS_STATE_BAD_SIZE);

    CHECK(space_invaders_state_path(path, sizeof(path)));
    CHECK(strcmp(path, "/SAVES/spaceinvaders/state.bin") == 0);
    CHECK(!space_invaders_state_path(path, 8u));
    free(data);
    if (failures != 0u) return 1;
    puts("Space Invaders versioned state checks passed");
    return 0;
}
