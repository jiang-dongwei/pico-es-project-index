#ifndef PICO286_BOARD_PROFILE_H
#define PICO286_BOARD_PROFILE_H

#include <stdint.h>

#define PICO286_HW_CONFIG 16u
#define PICO286_SYSTEM_CLOCK_KHZ 252000u

#define PICO286_PSRAM_CS_PIN 47u
#define PICO286_PSRAM_XIP_BASE 0x11000000u
#define PICO286_PSRAM_SIZE_BYTES (8u * 1024u * 1024u)

#define PICO286_HSTX_D0_PIN 16u
#define PICO286_HSTX_D1_PIN 14u
#define PICO286_HSTX_D2_PIN 12u
#define PICO286_HSTX_CLOCK_PIN 18u

#define PICO286_USB0_DP_PIN 28u
#define PICO286_USB1_DP_PIN 26u

#define PICO286_NES0_CLOCK_PIN 6u
#define PICO286_NES0_DATA_PIN 8u
#define PICO286_NES0_LATCH_PIN 7u
#define PICO286_NES1_CLOCK_PIN 9u
#define PICO286_NES1_DATA_PIN 11u
#define PICO286_NES1_LATCH_PIN 10u

#define PICO286_SD_CS_PIN 43u
#define PICO286_SD_SCK_PIN 30u
#define PICO286_SD_MOSI_PIN 31u
#define PICO286_SD_MISO_PIN 40u

#define PICO286_UART_TX_PIN 2u
#define PICO286_UART_RX_PIN 3u

typedef struct pico286_board_profile {
    uint32_t hw_config;
    uint32_t system_clock_khz;
    uint32_t psram_cs_pin;
    uint32_t psram_xip_base;
    uint32_t psram_size_bytes;
    uint32_t hstx_d0_pin;
    uint32_t hstx_d1_pin;
    uint32_t hstx_d2_pin;
    uint32_t hstx_clock_pin;
    uint32_t usb0_dp_pin;
    uint32_t usb1_dp_pin;
    uint32_t nes0_clock_pin;
    uint32_t nes0_data_pin;
    uint32_t nes0_latch_pin;
    uint32_t nes1_clock_pin;
    uint32_t nes1_data_pin;
    uint32_t nes1_latch_pin;
    uint32_t sd_cs_pin;
    uint32_t sd_sck_pin;
    uint32_t sd_mosi_pin;
    uint32_t sd_miso_pin;
    uint32_t uart_tx_pin;
    uint32_t uart_rx_pin;
} pico286_board_profile_t;

typedef enum pico286_board_status {
    PICO286_BOARD_OK = 0,
    PICO286_BOARD_NULL,
    PICO286_BOARD_WRONG_REVISION,
    PICO286_BOARD_WRONG_CLOCK,
    PICO286_BOARD_WRONG_PSRAM,
    PICO286_BOARD_INSUFFICIENT_PSRAM,
    PICO286_BOARD_WRONG_HSTX,
    PICO286_BOARD_WRONG_USB,
    PICO286_BOARD_WRONG_NES,
    PICO286_BOARD_WRONG_SD,
    PICO286_BOARD_WRONG_UART
} pico286_board_status_t;

extern const pico286_board_profile_t pico286_hw16_board;

pico286_board_status_t pico286_board_validate(
    const pico286_board_profile_t *profile,
    uint32_t required_psram_bytes);
const char *pico286_board_status_string(pico286_board_status_t status);

#endif
