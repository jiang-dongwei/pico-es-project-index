#include "settings_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct fake_host {
    unsigned polls;
    unsigned renders;
    unsigned delays;
    unsigned resets;
    unsigned updates;
    unsigned changes;
    unsigned saves;
    unsigned cancels;
    unsigned begins;
    unsigned filtered;
} fake_host_t;

static void reset_owner(void *context)
{
    ++((fake_host_t *)context)->resets;
}

static void update_owner(void *context)
{
    ++((fake_host_t *)context)->updates;
}

static retropicoes_ui_actions_t read_actions(
    void *context, retropicoes_menu_repeat_t *repeat)
{
    fake_host_t *fake = (fake_host_t *)context;
    (void)repeat;
    if (fake->polls == 1u) return RETROPICOES_UI_RIGHT;
    if (fake->polls == 2u || fake->polls == 3u)
        return RETROPICOES_UI_MENU;
    return 0u;
}

static void begin_frame(void *context)
{
    fake_host_t *fake = (fake_host_t *)context;
    assert(fake->begins == fake->renders);
    ++fake->begins;
}

static retropicoes_ui_actions_t filter_actions(
    void *context, retropicoes_ui_actions_t actions)
{
    fake_host_t *fake = (fake_host_t *)context;
    if ((actions & RETROPICOES_UI_MENU) != 0u && fake->filtered++ == 0u)
        return actions & ~RETROPICOES_UI_MENU;
    return actions;
}

static void render(void *context,
                   const retropicoes_settings_menu_state_t *state)
{
    fake_host_t *fake = (fake_host_t *)context;
    assert(state->open);
    assert(fake->begins == fake->renders + 1u);
    ++fake->renders;
}

static void poll_input(void *context)
{
    ++((fake_host_t *)context)->polls;
}

static void delay_ms(void *context, unsigned milliseconds)
{
    assert(milliseconds == 16u);
    ++((fake_host_t *)context)->delays;
}

static int on_event(void *context, retropicoes_settings_event_t event)
{
    fake_host_t *fake = (fake_host_t *)context;
    if (event.type == RETROPICOES_SETTINGS_EVENT_CHANGE) ++fake->changes;
    if (event.type == RETROPICOES_SETTINGS_EVENT_SAVE) ++fake->saves;
    if (event.type == RETROPICOES_SETTINGS_EVENT_CANCEL) {
        ++fake->cancels;
        return 0;
    }
    return 1;
}

int main(void)
{
    fake_host_t fake;
    retropicoes_settings_platform_t platform;
    retropicoes_settings_host_t host;
    retropicoes_settings_home_gate_t gate;
    memset(&fake, 0, sizeof(fake));
    memset(&platform, 0, sizeof(platform));
    memset(&host, 0, sizeof(host));
    platform.context = &fake;
    platform.reset_owner = reset_owner;
    platform.update_owner = update_owner;
    platform.read_actions = read_actions;
    host.context = &fake;
    host.platform = &platform;
    host.item_count = 1u;
    host.begin_frame = begin_frame;
    host.render = render;
    host.poll_input = poll_input;
    host.delay_ms = delay_ms;
    host.filter_actions = filter_actions;
    host.on_event = on_event;
    assert(retropicoes_settings_host_run(&host) == 1);
    assert(fake.resets == 1u && fake.polls == 3u);
    assert(fake.begins == 3u && fake.renders == 3u && fake.updates == 3u);
    assert(fake.changes == 1u && fake.cancels == 1u);
    assert(fake.saves == 0u && fake.delays == 2u);
    assert(retropicoes_settings_host_run(NULL) == 0);

    retropicoes_settings_home_gate_init(&gate, 0);
    assert(retropicoes_settings_home_gate_filter(
        &gate, RETROPICOES_UI_MENU, 1, 100u, 250u) == 0u);
    assert(retropicoes_settings_home_gate_filter(
        &gate, 0u, 0, 150u, 250u) == 0u);
    assert(retropicoes_settings_home_gate_filter(
        &gate, 0u, 0, 399u, 250u) == 0u);
    assert(retropicoes_settings_home_gate_filter(
        &gate, RETROPICOES_UI_MENU, 1, 400u, 250u) == 0u);
    assert(retropicoes_settings_home_gate_filter(
        &gate, 0u, 0, 450u, 250u) == 0u);
    assert(retropicoes_settings_home_gate_filter(
        &gate, 0u, 0, 700u, 250u) == 0u);
    assert(gate.armed == 1u);
    assert(retropicoes_settings_home_gate_filter(
        &gate, RETROPICOES_UI_MENU, 1, 716u, 250u) ==
           RETROPICOES_UI_MENU);
    retropicoes_settings_home_gate_init(&gate, 1);
    assert(retropicoes_settings_home_gate_filter(
        &gate, RETROPICOES_UI_MENU, 1, 0u, 250u) ==
           RETROPICOES_UI_MENU);
    {
        retropicoes_settings_legacy_nav_t nav = {0, 0, 0, 0};
        retropicoes_settings_host_legacy_move(
            &nav, 15u, 12u, RETROPICOES_UI_UP);
        assert(nav.on_action_row == 1);
        retropicoes_settings_host_legacy_move(
            &nav, 15u, 12u, RETROPICOES_UI_UP);
        assert(nav.on_action_row == 0 && nav.selected == 14 &&
               nav.first_visible == 3);
        retropicoes_settings_host_legacy_move(
            &nav, 15u, 12u, RETROPICOES_UI_DOWN);
        assert(nav.on_action_row == 1);
        retropicoes_settings_host_legacy_move(
            &nav, 15u, 12u, RETROPICOES_UI_RIGHT);
        assert(nav.action == 1);
        retropicoes_settings_host_legacy_move(
            &nav, 15u, 12u, RETROPICOES_UI_LEFT);
        assert(nav.action == 0);
    }
    puts("settings host tests: PASS");
    return 0;
}
