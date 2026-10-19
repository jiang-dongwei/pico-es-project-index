#include "menu_state.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
                    #condition);                                               \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static retropicoes_menu_result_t callback(void *context)
{
    (void)context;
    return RETROPICOES_MENU_RESULT_OK;
}

static retropicoes_menu_ops_t full_ops(void)
{
    retropicoes_menu_ops_t ops;

    memset(&ops, 0, sizeof(ops));
    ops.contract_version = RETROPICOES_MENU_CONTRACT_VERSION;
    ops.struct_size = sizeof(ops);
    ops.capabilities = UINT32_MAX;
    ops.resume = callback;
    ops.show_controls = callback;
    ops.open_state_menu = callback;
    ops.open_settings = callback;
    ops.reset = callback;
    ops.open_rom_list = callback;
    ops.return_loader = callback;
    return ops;
}

static int test_list_wrap_and_pages(void)
{
    retropicoes_menu_list_state_t state;
    retropicoes_menu_event_t result;

    retropicoes_menu_list_init(&state, 23u, 10u);
    result = retropicoes_menu_list_update(&state, RETROPICOES_UI_UP);
    CHECK(result.type == RETROPICOES_MENU_EVENT_REDRAW);
    CHECK(state.selected == 22u);
    CHECK(state.first_visible == 20u);
    CHECK(retropicoes_menu_list_visible_count(&state) == 3u);
    (void)retropicoes_menu_list_update(&state, RETROPICOES_UI_RIGHT);
    CHECK(state.selected == 0u);
    CHECK(state.first_visible == 0u);
    (void)retropicoes_menu_list_update(&state, RETROPICOES_UI_LEFT);
    CHECK(state.selected == 22u);
    (void)retropicoes_menu_list_update(&state, RETROPICOES_UI_DOWN);
    CHECK(state.selected == 0u);

    state.selected = 22u;
    retropicoes_menu_list_set_count(&state, 5u);
    CHECK(state.selected == 4u);
    CHECK(state.first_visible == 0u);
    return 0;
}

static int test_list_action_priority_and_empty(void)
{
    retropicoes_menu_list_state_t state;
    retropicoes_menu_event_t result;

    retropicoes_menu_list_init(&state, 3u, 0u);
    CHECK(state.page_rows == RETROPICOES_MENU_DEFAULT_PAGE_ROWS);
    result = retropicoes_menu_list_update(
        &state, RETROPICOES_UI_CANCEL | RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CLOSE);

    retropicoes_menu_list_set_count(&state, 0u);
    result = retropicoes_menu_list_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_NONE);
    return 0;
}

static int test_system_menu_confirmation(void)
{
    retropicoes_menu_ops_t ops = full_ops();
    retropicoes_system_menu_state_t state;
    retropicoes_system_menu_event_t result;
    unsigned i;

    CHECK(retropicoes_system_menu_open(&state, &ops, 5u));
    CHECK(state.item_count == RETROPICOES_MENU_ITEM_COUNT);
    for (i = 0u; i < 4u; ++i) {
        (void)retropicoes_system_menu_update(&state, RETROPICOES_UI_DOWN);
    }
    CHECK(state.items[state.list.selected] == RETROPICOES_MENU_ITEM_RESET);
    result = retropicoes_system_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CONFIRM);
    CHECK(state.confirmation_active);

    result = retropicoes_system_menu_update(&state, RETROPICOES_UI_DOWN);
    CHECK(result.type == RETROPICOES_MENU_EVENT_NONE);
    CHECK(state.list.selected == 4u);
    result = retropicoes_system_menu_update(&state, RETROPICOES_UI_CANCEL);
    CHECK(result.type == RETROPICOES_MENU_EVENT_REDRAW);
    CHECK(!state.confirmation_active);

    (void)retropicoes_system_menu_update(&state, RETROPICOES_UI_CONFIRM);
    result = retropicoes_system_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_ACTIVATE);
    CHECK(result.item == RETROPICOES_MENU_ITEM_RESET);
    return 0;
}

static int test_system_menu_close_and_missing_contract(void)
{
    retropicoes_menu_ops_t ops = full_ops();
    retropicoes_system_menu_state_t state;
    retropicoes_system_menu_event_t result;

    CHECK(!retropicoes_system_menu_open(&state, NULL, 7u));
    CHECK(!state.open);
    CHECK(retropicoes_system_menu_open(&state, &ops, 7u));
    result = retropicoes_system_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_ACTIVATE);
    CHECK(result.item == RETROPICOES_MENU_ITEM_RESUME);

    result = retropicoes_system_menu_update(&state, RETROPICOES_UI_CANCEL);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CLOSE);
    CHECK(!state.open);
    result = retropicoes_system_menu_update(&state, RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_NONE);
    return 0;
}

int main(void)
{
    CHECK(test_list_wrap_and_pages() == 0);
    CHECK(test_list_action_priority_and_empty() == 0);
    CHECK(test_system_menu_confirmation() == 0);
    CHECK(test_system_menu_close_and_missing_contract() == 0);
    puts("common menu state tests: PASS");
    return 0;
}
