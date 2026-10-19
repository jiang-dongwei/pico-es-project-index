#include "core_events.h"

#include <assert.h>
#include <stdint.h>

static void test_invalid_arguments(void) {
    pico286_core_events_t events;
    pico286_core_event_stats_t stats;
    pico286_core_events_init(NULL);
    pico286_core_events_init(&events);

    assert(pico286_core_events_publish_pit(NULL, 1u) == 0u);
    assert(pico286_core_events_publish_pit(&events, 0u) == 0u);
    assert(pico286_core_events_consume_pit(NULL, 1u) == 0u);
    assert(pico286_core_events_consume_pit(&events, 0u) == 0u);
    assert(!pico286_core_events_snapshot(NULL, &stats));
    assert(!pico286_core_events_snapshot(&events, NULL));
}

static void test_publish_and_partial_consume(void) {
    pico286_core_events_t events;
    pico286_core_event_stats_t stats;
    pico286_core_events_init(&events);

    assert(pico286_core_events_publish_pit(&events, 3u) == 3u);
    assert(pico286_core_events_consume_pit(&events, 2u) == 2u);
    assert(pico286_core_events_publish_pit(&events, 4u) == 4u);
    assert(pico286_core_events_consume_pit(&events, UINT32_MAX) == 5u);
    assert(pico286_core_events_consume_pit(&events, 1u) == 0u);

    assert(pico286_core_events_snapshot(&events, &stats));
    assert(stats.pending_pit_ticks == 0u);
    assert(stats.accepted_pit_ticks == 7u);
    assert(stats.consumed_pit_ticks == 7u);
    assert(stats.dropped_pit_ticks == 0u);
}

static void test_bounded_backlog(void) {
    pico286_core_events_t events;
    pico286_core_event_stats_t stats;
    pico286_core_events_init(&events);

    assert(pico286_core_events_publish_pit(&events, UINT32_MAX) ==
           PICO286_MAX_PIT_CATCHUP);
    assert(pico286_core_events_publish_pit(&events, 2u) == 0u);
    assert(pico286_core_events_snapshot(&events, &stats));
    assert(stats.pending_pit_ticks == PICO286_MAX_PIT_CATCHUP);
    assert(stats.accepted_pit_ticks == PICO286_MAX_PIT_CATCHUP);
    assert(stats.dropped_pit_ticks == UINT32_MAX - 6u);

    assert(pico286_core_events_consume_pit(&events, 3u) == 3u);
    assert(pico286_core_events_publish_pit(&events, 5u) == 3u);
    assert(pico286_core_events_snapshot(&events, &stats));
    assert(stats.pending_pit_ticks == PICO286_MAX_PIT_CATCHUP);
    assert(stats.accepted_pit_ticks == PICO286_MAX_PIT_CATCHUP + 3u);
    assert(stats.consumed_pit_ticks == 3u);
    assert(stats.dropped_pit_ticks == UINT32_MAX - 4u);
}

int main(void) {
    test_invalid_arguments();
    test_publish_and_partial_consume();
    test_bounded_backlog();
    return 0;
}
