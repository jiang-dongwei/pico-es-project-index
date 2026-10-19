#ifndef SDLPOP_RETROPICOES_RUNTIME_H
#define SDLPOP_RETROPICOES_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int retropicoes_storage_ready(void);
int retropicoes_psram_ready(void);
size_t retropicoes_psram_size(void);
uint32_t retropicoes_system_clock_hz(void);
int retropicoes_pointer_in_psram(const void *ptr);
int retropicoes_launched_from_bootloader(void);
void retropicoes_return_to_bootloader(void);

#ifdef __cplusplus
}
#endif

#endif
