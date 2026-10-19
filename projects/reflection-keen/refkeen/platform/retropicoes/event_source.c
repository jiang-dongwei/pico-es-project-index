/* See event_source.h for the contract and the reasoning behind it. */

#include "event_source.h"

/* Local spellings, so the arithmetic below reads like the problem rather than
 * like a chain of shifts. Deliberately not exported: the sibling ports keep
 * their equivalent helper in input_mapping.c, and the port's input_mapping.c is
 * a different file with a different job (compat channel -> SDL scancode), so
 * putting a third name here would imply a shared contract that does not exist. */
static uint32_t rising_edges(uint32_t current_mask, uint32_t previous_mask)
{
    return current_mask & ~previous_mask;
}

size_t retropicoes_refkeen_key_events_from_masks(
    uint32_t current_mask, uint32_t previous_mask,
    retropicoes_refkeen_key_event_t *out, size_t capacity)
{
    uint16_t scancodes[RETROPICOES_REFKEEN_SCANCODE_MAX];
    size_t written = 0;
    size_t count;
    size_t index;

    if ((NULL == out) || (0 == capacity))
        return 0;

    /* Releases first: the last event for any given scancode must agree with
     * current_mask. See the header comment. */
    count = retropicoes_refkeen_scancodes_from_mask(
        rising_edges(previous_mask, current_mask), scancodes,
        RETROPICOES_REFKEEN_SCANCODE_MAX);
    for (index = 0; (index < count) && (written < capacity); ++index) {
        out[written].scancode = scancodes[index];
        out[written].pressed = 0;
        ++written;
    }

    count = retropicoes_refkeen_scancodes_from_mask(
        rising_edges(current_mask, previous_mask), scancodes,
        RETROPICOES_REFKEEN_SCANCODE_MAX);
    for (index = 0; (index < count) && (written < capacity); ++index) {
        out[written].scancode = scancodes[index];
        out[written].pressed = 1;
        ++written;
    }

    return written;
}
