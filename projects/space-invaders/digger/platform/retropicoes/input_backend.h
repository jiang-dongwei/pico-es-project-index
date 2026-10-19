#ifndef SDLPOP_RETROPICOES_INPUT_BACKEND_H
#define SDLPOP_RETROPICOES_INPUT_BACKEND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t polls;
    uint32_t source_masks[4]; /* USB1, USB2, NES1, NES2 */
    uint32_t current_mask;
    uint32_t pressed_mask;
    uint8_t usb_connected_mask;
    uint8_t nes_pad_type[2];
    uint8_t nes_initialized_mask;
    int usb_host_initialized;
    int usb_second_port_result;
} retropicoes_input_stats_t;

int retropicoes_input_init(void);
void retropicoes_input_shutdown(void);
int retropicoes_input_ready(void);
void retropicoes_input_get_stats(retropicoes_input_stats_t *stats);
int retropicoes_input_menu_pressed(void);

#ifdef __cplusplus
}
#endif

#endif
