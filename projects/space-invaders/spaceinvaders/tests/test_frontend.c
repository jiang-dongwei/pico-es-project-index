/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "compat.h"
#include "frontend.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;
static uint8_t rom_data[SPACE_INVADERS_ROM_COUNT]
                       [SPACE_INVADERS_ROM_CHIP_SIZE];
static uint16_t frame[SPACE_INVADERS_FRAME_WIDTH *
                      SPACE_INVADERS_FRAME_HEIGHT];
static int16_t audio_samples[SPACE_INVADERS_AUDIO_RATE / 60u];

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

static uint32_t key(compat_key_t value)
{
    return 1u << (unsigned)value;
}

static int has_audio(void)
{
    for (size_t i = 0; i < sizeof(audio_samples) / sizeof(audio_samples[0]); ++i)
        if (audio_samples[i] != 0) return 1;
    return 0;
}

int main(void)
{
    space_invaders_frontend_t frontend;
    space_invaders_rom_set_t roms;
    space_invaders_frontend_result_t result;

    CHECK(space_invaders_frontend_init(&frontend));
    result = space_invaders_frontend_frame(
        &frontend, 0u, 0u, 0u, frame, SPACE_INVADERS_FRAME_WIDTH,
        audio_samples, sizeof(audio_samples) / sizeof(audio_samples[0]));
    CHECK(result.status == SPACE_INVADERS_RUN_ROM_NOT_READY);

    for (unsigned chip = 0; chip < SPACE_INVADERS_ROM_COUNT; ++chip) {
        memset(rom_data[chip], 0u, sizeof(rom_data[chip]));
        roms.data[chip] = rom_data[chip];
        roms.size[chip] = sizeof(rom_data[chip]);
    }
    rom_data[0][0] = 0xf3u; /* DI */
    rom_data[0][1] = 0xc3u; /* JMP 0001 */
    rom_data[0][2] = 0x01u;
    rom_data[0][3] = 0x00u;
    CHECK(space_invaders_frontend_load_roms(&frontend, &roms).status ==
          SPACE_INVADERS_ROM_OK);
    frontend.board.memory[SPACE_INVADERS_VRAM_START + 31u] = 0x80u;
    frontend.board.sound_rising3 = 0x01u;
    result = space_invaders_frontend_frame(
        &frontend, key(COMPAT_KEY_LEFT) | key(COMPAT_KEY_ACTION), 0u, 10u,
        frame, SPACE_INVADERS_FRAME_WIDTH, audio_samples,
        sizeof(audio_samples) / sizeof(audio_samples[0]));
    CHECK(result.status == SPACE_INVADERS_RUN_OK && result.frame_ready);
    CHECK(result.cpu_cycles >= 33280u && result.cpu_instructions != 0u);
    CHECK(frontend.board.frame_count == 1u);
    CHECK((frontend.board.input1 & (SPACE_INVADERS_INPUT_LEFT |
                                    SPACE_INVADERS_INPUT_FIRE)) ==
          (SPACE_INVADERS_INPUT_LEFT | SPACE_INVADERS_INPUT_FIRE));
    CHECK(frame[55] == 0xffffu);
    CHECK(has_audio());

    if (failures != 0u) return 1;
    puts("Space Invaders frontend checks passed");
    return 0;
}
