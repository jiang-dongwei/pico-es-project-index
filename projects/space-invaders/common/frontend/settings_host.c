#include "settings_host.h"

void retropicoes_settings_home_gate_init(
    retropicoes_settings_home_gate_t *gate, int initially_armed)
{
    if (gate == NULL) return;
    gate->neutral_since_ms = 0u;
    gate->neutral_started = 0u;
    gate->armed = initially_armed != 0;
}

retropicoes_ui_actions_t retropicoes_settings_home_gate_filter(
    retropicoes_settings_home_gate_t *gate,
    retropicoes_ui_actions_t actions,
    int physical_buttons_down,
    uint32_t now_ms,
    uint32_t stable_release_ms)
{
    if (gate == NULL || gate->armed) return actions;
    if (physical_buttons_down) {
        gate->neutral_started = 0u;
    } else if (!gate->neutral_started) {
        gate->neutral_since_ms = now_ms;
        gate->neutral_started = 1u;
    } else if ((uint32_t)(now_ms - gate->neutral_since_ms) >=
               stable_release_ms) {
        gate->armed = 1u;
    }
    /* The neutral sample which arms the gate is still consumed. */
    return 0u;
}

void retropicoes_settings_host_legacy_move(
    retropicoes_settings_legacy_nav_t *nav,
    size_t item_count,
    size_t page_rows,
    retropicoes_ui_actions_t direction)
{
    retropicoes_settings_menu_state_t state;
    if (nav == NULL || item_count > 0x7fffffffu) return;
    if (nav->selected < 0 || nav->first_visible < 0) return;
    if (!retropicoes_settings_menu_open(&state, item_count, page_rows)) return;
    state.list.selected = (size_t)nav->selected;
    state.list.first_visible = (size_t)nav->first_visible;
    state.on_action_row = nav->on_action_row;
    state.action = (size_t)nav->action;
    (void)retropicoes_settings_menu_update(&state, direction);
    nav->selected = (int)state.list.selected;
    nav->first_visible = (int)state.list.first_visible;
    nav->on_action_row = state.on_action_row;
    nav->action = (int)state.action;
}

int retropicoes_settings_host_run(const retropicoes_settings_host_t *host)
{
    retropicoes_settings_menu_state_t state;
    retropicoes_menu_repeat_t repeat;
    const retropicoes_settings_platform_t *platform;

    if (host == NULL || host->platform == NULL || host->render == NULL ||
        host->poll_input == NULL || host->delay_ms == NULL ||
        host->on_event == NULL || host->item_count == 0u)
        return 0;
    platform = host->platform;
    if (platform->reset_owner == NULL || platform->update_owner == NULL ||
        platform->read_actions == NULL ||
        !retropicoes_settings_menu_open(&state, host->item_count,
                                        host->page_rows))
        return 0;
    platform->reset_owner(platform->context);
    retropicoes_menu_repeat_reset(&repeat);
    for (;;) {
        retropicoes_settings_event_t event;
        retropicoes_ui_actions_t actions;
        if (host->begin_frame != NULL) host->begin_frame(host->context);
        host->render(host->context, &state);
        host->poll_input(host->context);
        platform->update_owner(platform->context);
        actions = platform->read_actions(platform->context, &repeat);
        if (host->filter_actions != NULL)
            actions = host->filter_actions(host->context, actions);
        event = retropicoes_settings_menu_update(&state, actions);
        if (event.type != RETROPICOES_SETTINGS_EVENT_NONE &&
            event.type != RETROPICOES_SETTINGS_EVENT_REDRAW &&
            !host->on_event(host->context, event))
            return 1;
        host->delay_ms(host->context, 16u);
    }
}

