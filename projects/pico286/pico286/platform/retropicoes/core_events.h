#ifndef PICO286_RETROPICOES_CORE_EVENTS_H
#define PICO286_RETROPICOES_CORE_EVENTS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#include "platform_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pico286_core_events {
    atomic_uint_least32_t pending_pit_ticks;
    atomic_uint_least32_t accepted_pit_ticks;
    atomic_uint_least32_t consumed_pit_ticks;
    atomic_uint_least32_t dropped_pit_ticks;
} pico286_core_events_t;

typedef struct pico286_core_event_stats {
    uint32_t pending_pit_ticks;
    uint32_t accepted_pit_ticks;
    uint32_t consumed_pit_ticks;
    uint32_t dropped_pit_ticks;
} pico286_core_event_stats_t;

void pico286_core_events_init(pico286_core_events_t *events);

uint32_t pico286_core_events_publish_pit(pico286_core_events_t *events,
                                         uint32_t tick_count);

uint32_t pico286_core_events_consume_pit(pico286_core_events_t *events,
                                         uint32_t max_ticks);

bool pico286_core_events_snapshot(const pico286_core_events_t *events,
                                  pico286_core_event_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
