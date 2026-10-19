#ifndef SDLPOP_RETROPICOES_TINYUSB_HOST_STUB_H
#define SDLPOP_RETROPICOES_TINYUSB_HOST_STUB_H

/*
 * S2-B compiles FrensHelpers.cpp for clock/PSRAM/SD services with the TinyUSB
 * host stack disabled. Its unrelated flashrom() function calls tuh_task()
 * without guarding CFG_TUH_ENABLED, so provide the no-op needed to compile
 * that otherwise discarded function. S2-D removes this shim when real PIO-USB
 * host support is linked.
 */
static inline void tuh_task(void) {}

#endif
