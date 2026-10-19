#include "board_profile.h"
#include "memory_layout.h"

#include <assert.h>
#include <string.h>

static void expect_status(
    pico286_board_profile_t profile,
    pico286_board_status_t expected)
{
    assert(pico286_board_validate(&profile, PICO286_PSRAM_EMULATOR_BYTES) ==
           expected);
}

int main(void)
{
    pico286_board_profile_t profile = pico286_hw16_board;

    assert(pico286_board_validate(0, 0u) == PICO286_BOARD_NULL);
    assert(pico286_board_validate(&profile, PICO286_PSRAM_EMULATOR_BYTES) ==
           PICO286_BOARD_OK);
    assert(pico286_board_validate(&profile, PICO286_PSRAM_SIZE_BYTES) ==
           PICO286_BOARD_OK);
    assert(pico286_board_validate(&profile, PICO286_PSRAM_SIZE_BYTES + 1u) ==
           PICO286_BOARD_INSUFFICIENT_PSRAM);
    assert(strcmp(pico286_board_status_string(PICO286_BOARD_OK), "ok") == 0);
    assert(strcmp(pico286_board_status_string((pico286_board_status_t)99),
                  "unknown board status") == 0);

    profile.hw_config = 15u;
    expect_status(profile, PICO286_BOARD_WRONG_REVISION);
    profile = pico286_hw16_board;
    profile.system_clock_khz = 504000u;
    expect_status(profile, PICO286_BOARD_WRONG_CLOCK);
    profile = pico286_hw16_board;
    profile.psram_cs_pin = 0u;
    expect_status(profile, PICO286_BOARD_WRONG_PSRAM);
    profile = pico286_hw16_board;
    profile.hstx_d0_pin = 0u;
    expect_status(profile, PICO286_BOARD_WRONG_HSTX);
    profile = pico286_hw16_board;
    profile.usb0_dp_pin = 0u;
    expect_status(profile, PICO286_BOARD_WRONG_USB);
    profile = pico286_hw16_board;
    profile.nes0_clock_pin = 0u;
    expect_status(profile, PICO286_BOARD_WRONG_NES);
    profile = pico286_hw16_board;
    profile.sd_cs_pin = 0u;
    expect_status(profile, PICO286_BOARD_WRONG_SD);
    profile = pico286_hw16_board;
    profile.uart_tx_pin = 0u;
    expect_status(profile, PICO286_BOARD_WRONG_UART);

    return 0;
}
