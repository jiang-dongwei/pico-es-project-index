#include "board_profile.h"

const pico286_board_profile_t pico286_hw16_board = {
    .hw_config = PICO286_HW_CONFIG,
    .system_clock_khz = PICO286_SYSTEM_CLOCK_KHZ,
    .psram_cs_pin = PICO286_PSRAM_CS_PIN,
    .psram_xip_base = PICO286_PSRAM_XIP_BASE,
    .psram_size_bytes = PICO286_PSRAM_SIZE_BYTES,
    .hstx_d0_pin = PICO286_HSTX_D0_PIN,
    .hstx_d1_pin = PICO286_HSTX_D1_PIN,
    .hstx_d2_pin = PICO286_HSTX_D2_PIN,
    .hstx_clock_pin = PICO286_HSTX_CLOCK_PIN,
    .usb0_dp_pin = PICO286_USB0_DP_PIN,
    .usb1_dp_pin = PICO286_USB1_DP_PIN,
    .nes0_clock_pin = PICO286_NES0_CLOCK_PIN,
    .nes0_data_pin = PICO286_NES0_DATA_PIN,
    .nes0_latch_pin = PICO286_NES0_LATCH_PIN,
    .nes1_clock_pin = PICO286_NES1_CLOCK_PIN,
    .nes1_data_pin = PICO286_NES1_DATA_PIN,
    .nes1_latch_pin = PICO286_NES1_LATCH_PIN,
    .sd_cs_pin = PICO286_SD_CS_PIN,
    .sd_sck_pin = PICO286_SD_SCK_PIN,
    .sd_mosi_pin = PICO286_SD_MOSI_PIN,
    .sd_miso_pin = PICO286_SD_MISO_PIN,
    .uart_tx_pin = PICO286_UART_TX_PIN,
    .uart_rx_pin = PICO286_UART_RX_PIN,
};

pico286_board_status_t pico286_board_validate(
    const pico286_board_profile_t *profile,
    uint32_t required_psram_bytes)
{
    if (profile == 0) {
        return PICO286_BOARD_NULL;
    }
    if (profile->hw_config != PICO286_HW_CONFIG) {
        return PICO286_BOARD_WRONG_REVISION;
    }
    if (profile->system_clock_khz != PICO286_SYSTEM_CLOCK_KHZ) {
        return PICO286_BOARD_WRONG_CLOCK;
    }
    if ((profile->psram_cs_pin != PICO286_PSRAM_CS_PIN) ||
        (profile->psram_xip_base != PICO286_PSRAM_XIP_BASE) ||
        (profile->psram_size_bytes != PICO286_PSRAM_SIZE_BYTES)) {
        return PICO286_BOARD_WRONG_PSRAM;
    }
    if (required_psram_bytes > profile->psram_size_bytes) {
        return PICO286_BOARD_INSUFFICIENT_PSRAM;
    }
    if ((profile->hstx_d0_pin != PICO286_HSTX_D0_PIN) ||
        (profile->hstx_d1_pin != PICO286_HSTX_D1_PIN) ||
        (profile->hstx_d2_pin != PICO286_HSTX_D2_PIN) ||
        (profile->hstx_clock_pin != PICO286_HSTX_CLOCK_PIN)) {
        return PICO286_BOARD_WRONG_HSTX;
    }
    if ((profile->usb0_dp_pin != PICO286_USB0_DP_PIN) ||
        (profile->usb1_dp_pin != PICO286_USB1_DP_PIN)) {
        return PICO286_BOARD_WRONG_USB;
    }
    if ((profile->nes0_clock_pin != PICO286_NES0_CLOCK_PIN) ||
        (profile->nes0_data_pin != PICO286_NES0_DATA_PIN) ||
        (profile->nes0_latch_pin != PICO286_NES0_LATCH_PIN) ||
        (profile->nes1_clock_pin != PICO286_NES1_CLOCK_PIN) ||
        (profile->nes1_data_pin != PICO286_NES1_DATA_PIN) ||
        (profile->nes1_latch_pin != PICO286_NES1_LATCH_PIN)) {
        return PICO286_BOARD_WRONG_NES;
    }
    if ((profile->sd_cs_pin != PICO286_SD_CS_PIN) ||
        (profile->sd_sck_pin != PICO286_SD_SCK_PIN) ||
        (profile->sd_mosi_pin != PICO286_SD_MOSI_PIN) ||
        (profile->sd_miso_pin != PICO286_SD_MISO_PIN)) {
        return PICO286_BOARD_WRONG_SD;
    }
    if ((profile->uart_tx_pin != PICO286_UART_TX_PIN) ||
        (profile->uart_rx_pin != PICO286_UART_RX_PIN)) {
        return PICO286_BOARD_WRONG_UART;
    }
    return PICO286_BOARD_OK;
}

const char *pico286_board_status_string(pico286_board_status_t status)
{
    static const char *const names[] = {
        "ok",
        "null profile",
        "wrong hardware revision",
        "wrong system clock",
        "wrong PSRAM configuration",
        "insufficient PSRAM",
        "wrong HSTX configuration",
        "wrong USB configuration",
        "wrong NES configuration",
        "wrong SD configuration",
        "wrong UART configuration",
    };
    const uint32_t index = (uint32_t)status;
    if (index >= (uint32_t)(sizeof(names) / sizeof(names[0]))) {
        return "unknown board status";
    }
    return names[index];
}
