#include "core_events.h"

#include <stddef.h>

static void add_saturating_counter(atomic_uint_least32_t *counter,
                                   uint32_t increment) {
    uint_least32_t current = atomic_load_explicit(counter,
                                                   memory_order_relaxed);
    while (increment != 0u) {
        const uint_least32_t room = UINT32_MAX - current;
        const uint_least32_t next = room < increment
                                        ? UINT32_MAX
                                        : current + increment;
        if (atomic_compare_exchange_weak_explicit(counter,
                                                  &current,
                                                  next,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed)) {
            return;
        }
    }
}

void pico286_core_events_init(pico286_core_events_t *events) {
    if (events == NULL) {
        return;
    }
    atomic_init(&events->pending_pit_ticks, 0u);
    atomic_init(&events->accepted_pit_ticks, 0u);
    atomic_init(&events->consumed_pit_ticks, 0u);
    atomic_init(&events->dropped_pit_ticks, 0u);
}

uint32_t pico286_core_events_publish_pit(pico286_core_events_t *events,
                                         uint32_t tick_count) {
    if (events == NULL || tick_count == 0u) {
        return 0u;
    }

    uint_least32_t pending = atomic_load_explicit(&events->pending_pit_ticks,
                                                  memory_order_relaxed);
    uint32_t accepted = 0u;
    for (;;) {
        const uint32_t room = pending >= PICO286_MAX_PIT_CATCHUP
                                  ? 0u
                                  : PICO286_MAX_PIT_CATCHUP -
                                        (uint32_t)pending;
        accepted = tick_count < room ? tick_count : room;
        const uint_least32_t next = pending + accepted;
        if (atomic_compare_exchange_weak_explicit(&events->pending_pit_ticks,
                                                  &pending,
                                                  next,
                                                  memory_order_release,
                                                  memory_order_relaxed)) {
            break;
        }
    }

    add_saturating_counter(&events->accepted_pit_ticks, accepted);
    add_saturating_counter(&events->dropped_pit_ticks,
                           tick_count - accepted);
    return accepted;
}

uint32_t pico286_core_events_consume_pit(pico286_core_events_t *events,
                                         uint32_t max_ticks) {
    if (events == NULL || max_ticks == 0u) {
        return 0u;
    }

    uint_least32_t pending = atomic_load_explicit(&events->pending_pit_ticks,
                                                  memory_order_acquire);
    uint32_t consumed = 0u;
    for (;;) {
        consumed = pending < max_ticks ? (uint32_t)pending : max_ticks;
        const uint_least32_t next = pending - consumed;
        if (atomic_compare_exchange_weak_explicit(&events->pending_pit_ticks,
                                                  &pending,
                                                  next,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
            break;
        }
    }

    add_saturating_counter(&events->consumed_pit_ticks, consumed);
    return consumed;
}

bool pico286_core_events_snapshot(const pico286_core_events_t *events,
                                  pico286_core_event_stats_t *stats) {
    if (events == NULL || stats == NULL) {
        return false;
    }
    stats->pending_pit_ticks = (uint32_t)atomic_load_explicit(
        &events->pending_pit_ticks, memory_order_acquire);
    stats->accepted_pit_ticks = (uint32_t)atomic_load_explicit(
        &events->accepted_pit_ticks, memory_order_relaxed);
    stats->consumed_pit_ticks = (uint32_t)atomic_load_explicit(
        &events->consumed_pit_ticks, memory_order_relaxed);
    stats->dropped_pit_ticks = (uint32_t)atomic_load_explicit(
        &events->dropped_pit_ticks, memory_order_relaxed);
    return true;
}
