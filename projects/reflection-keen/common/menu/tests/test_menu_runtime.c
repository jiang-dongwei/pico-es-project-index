#include "menu_runtime.h"

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

typedef struct runtime_log {
    unsigned enter_calls;
    unsigned leave_calls;
    unsigned resume_calls;
    unsigned controls_calls;
    unsigned reset_calls;
    unsigned rom_calls;
    unsigned loader_calls;
    retropicoes_menu_result_t enter_result;
    retropicoes_menu_result_t leave_result;
    retropicoes_menu_result_t action_result;
} runtime_log_t;

static retropicoes_menu_result_t enter_menu(void *context)
{
    runtime_log_t *log = (runtime_log_t *)context;
    ++log->enter_calls;
    return log->enter_result;
}

static retropicoes_menu_result_t leave_menu(void *context)
{
    runtime_log_t *log = (runtime_log_t *)context;
    ++log->leave_calls;
    return log->leave_result;
}

#define ACTION_CALLBACK(name, member)                                         \
    static retropicoes_menu_result_t name(void *context)                       \
    {                                                                          \
        runtime_log_t *log = (runtime_log_t *)context;                         \
        ++log->member;                                                         \
        return log->action_result;                                             \
    }

ACTION_CALLBACK(resume, resume_calls)
ACTION_CALLBACK(controls, controls_calls)
ACTION_CALLBACK(reset, reset_calls)
ACTION_CALLBACK(rom_list, rom_calls)
ACTION_CALLBACK(return_loader, loader_calls)

static retropicoes_menu_ops_t ops(runtime_log_t *log)
{
    retropicoes_menu_ops_t value;

    memset(&value, 0, sizeof(value));
    value.contract_version = RETROPICOES_MENU_CONTRACT_VERSION;
    value.struct_size = sizeof(value);
    value.context = log;
    value.capabilities = RETROPICOES_MENU_CAP_RESUME |
                         RETROPICOES_MENU_CAP_CONTROLS |
                         RETROPICOES_MENU_CAP_RESET |
                         RETROPICOES_MENU_CAP_ROM_LIST |
                         RETROPICOES_MENU_CAP_RETURN_LOADER;
    value.resume = resume;
    value.show_controls = controls;
    value.reset = reset;
    value.open_rom_list = rom_list;
    value.return_loader = return_loader;
    return value;
}

static retropicoes_menu_lifecycle_t lifecycle(runtime_log_t *log)
{
    retropicoes_menu_lifecycle_t value;

    memset(&value, 0, sizeof(value));
    value.version = RETROPICOES_MENU_LIFECYCLE_VERSION;
    value.struct_size = sizeof(value);
    value.context = log;
    value.enter_menu = enter_menu;
    value.leave_menu = leave_menu;
    return value;
}

static runtime_log_t initial_log(void)
{
    runtime_log_t log;
    memset(&log, 0, sizeof(log));
    log.enter_result = RETROPICOES_MENU_RESULT_OK;
    log.leave_result = RETROPICOES_MENU_RESULT_OK;
    log.action_result = RETROPICOES_MENU_RESULT_OK;
    return log;
}

static void move_down(retropicoes_menu_runtime_t *runtime, unsigned count)
{
    unsigned i;
    for (i = 0u; i < count; ++i) {
        (void)retropicoes_menu_runtime_update(runtime,
                                              RETROPICOES_UI_DOWN);
    }
}

static int test_open_and_cancel_resume(void)
{
    runtime_log_t log = initial_log();
    retropicoes_menu_ops_t menu_ops = ops(&log);
    retropicoes_menu_lifecycle_t hooks = lifecycle(&log);
    retropicoes_menu_runtime_t runtime;
    retropicoes_menu_runtime_event_t result;

    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, &hooks, 7u) ==
          RETROPICOES_MENU_RESULT_OK);
    CHECK(log.enter_calls == 1u);
    CHECK(retropicoes_menu_runtime_is_open(&runtime));
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CANCEL);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CLOSE);
    CHECK(result.item == RETROPICOES_MENU_ITEM_RESUME);
    CHECK(log.resume_calls == 1u);
    CHECK(log.leave_calls == 1u);
    CHECK(!retropicoes_menu_runtime_is_open(&runtime));
    return 0;
}

static int test_submenu_results_stay_open(void)
{
    runtime_log_t log = initial_log();
    retropicoes_menu_ops_t menu_ops = ops(&log);
    retropicoes_menu_runtime_t runtime;
    retropicoes_menu_runtime_event_t result;

    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, NULL, 7u) ==
          RETROPICOES_MENU_RESULT_OK);
    move_down(&runtime, 1u);
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_REDRAW);
    CHECK(log.controls_calls == 1u);
    CHECK(retropicoes_menu_runtime_is_open(&runtime));

    log.action_result = RETROPICOES_MENU_RESULT_CANCELLED;
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_REDRAW);
    CHECK(retropicoes_menu_runtime_is_open(&runtime));

    log.action_result = RETROPICOES_MENU_RESULT_FAILED;
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_ERROR);
    CHECK(result.result == RETROPICOES_MENU_RESULT_FAILED);
    CHECK(retropicoes_menu_runtime_is_open(&runtime));
    return 0;
}

static int test_reset_confirmation_closes_and_restores(void)
{
    runtime_log_t log = initial_log();
    retropicoes_menu_ops_t menu_ops = ops(&log);
    retropicoes_menu_lifecycle_t hooks = lifecycle(&log);
    retropicoes_menu_runtime_t runtime;
    retropicoes_menu_runtime_event_t result;

    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, &hooks, 7u) ==
          RETROPICOES_MENU_RESULT_OK);
    move_down(&runtime, 2u);
    CHECK(runtime.menu.items[runtime.menu.list.selected] ==
          RETROPICOES_MENU_ITEM_RESET);
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CONFIRM);
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CLOSE);
    CHECK(log.reset_calls == 1u);
    CHECK(log.leave_calls == 1u);
    return 0;
}

static int test_terminal_transfer_does_not_resume_game(void)
{
    runtime_log_t log = initial_log();
    retropicoes_menu_ops_t menu_ops = ops(&log);
    retropicoes_menu_lifecycle_t hooks = lifecycle(&log);
    retropicoes_menu_runtime_t runtime;
    retropicoes_menu_runtime_event_t result;

    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, &hooks, 7u) ==
          RETROPICOES_MENU_RESULT_OK);
    move_down(&runtime, 3u);
    CHECK(runtime.menu.items[runtime.menu.list.selected] ==
          RETROPICOES_MENU_ITEM_ROM_LIST);
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CLOSE);
    CHECK(log.rom_calls == 1u);
    CHECK(log.leave_calls == 0u);
    CHECK(!retropicoes_menu_runtime_is_open(&runtime));

    log = initial_log();
    menu_ops = ops(&log);
    hooks = lifecycle(&log);
    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, &hooks, 7u) ==
          RETROPICOES_MENU_RESULT_OK);
    move_down(&runtime, 4u);
    CHECK(runtime.menu.items[runtime.menu.list.selected] ==
          RETROPICOES_MENU_ITEM_RETURN_LOADER);
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CONFIRM);
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CONFIRM);
    CHECK(result.type == RETROPICOES_MENU_EVENT_CLOSE);
    CHECK(log.loader_calls == 1u);
    CHECK(log.leave_calls == 0u);
    return 0;
}

static int test_failures_do_not_drop_menu(void)
{
    runtime_log_t log = initial_log();
    retropicoes_menu_ops_t menu_ops = ops(&log);
    retropicoes_menu_lifecycle_t hooks = lifecycle(&log);
    retropicoes_menu_runtime_t runtime;
    retropicoes_menu_runtime_event_t result;

    log.enter_result = RETROPICOES_MENU_RESULT_FAILED;
    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, &hooks, 7u) ==
          RETROPICOES_MENU_RESULT_FAILED);
    CHECK(!retropicoes_menu_runtime_is_open(&runtime));

    log = initial_log();
    menu_ops = ops(&log);
    hooks = lifecycle(&log);
    log.leave_result = RETROPICOES_MENU_RESULT_FAILED;
    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, &hooks, 7u) ==
          RETROPICOES_MENU_RESULT_OK);
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CANCEL);
    CHECK(result.type == RETROPICOES_MENU_EVENT_ERROR);
    CHECK(retropicoes_menu_runtime_is_open(&runtime));

    log.leave_result = RETROPICOES_MENU_RESULT_OK;
    log.action_result = RETROPICOES_MENU_RESULT_CANCELLED;
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_CANCEL);
    CHECK(result.type == RETROPICOES_MENU_EVENT_REDRAW);
    CHECK(retropicoes_menu_runtime_is_open(&runtime));
    result = retropicoes_menu_runtime_update(&runtime,
                                             RETROPICOES_UI_DOWN);
    CHECK(result.type == RETROPICOES_MENU_EVENT_REDRAW);
    return 0;
}

static int test_invalid_contracts(void)
{
    runtime_log_t log = initial_log();
    retropicoes_menu_ops_t menu_ops = ops(&log);
    retropicoes_menu_lifecycle_t hooks = lifecycle(&log);
    retropicoes_menu_runtime_t runtime;

    menu_ops.capabilities &= ~RETROPICOES_MENU_CAP_RESUME;
    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, NULL, 7u) ==
          RETROPICOES_MENU_RESULT_INVALID);
    CHECK(!retropicoes_menu_runtime_is_open(&runtime));
    menu_ops = ops(&log);
    hooks.version++;
    CHECK(retropicoes_menu_runtime_open(&runtime, &menu_ops, &hooks, 7u) ==
          RETROPICOES_MENU_RESULT_INVALID);
    CHECK(!retropicoes_menu_runtime_is_open(&runtime));
    CHECK(strcmp(retropicoes_menu_result_name(
                     RETROPICOES_MENU_RESULT_UNAVAILABLE),
                 "UNAVAILABLE") == 0);
    return 0;
}

int main(void)
{
    CHECK(test_open_and_cancel_resume() == 0);
    CHECK(test_submenu_results_stay_open() == 0);
    CHECK(test_reset_confirmation_closes_and_restores() == 0);
    CHECK(test_terminal_transfer_does_not_resume_game() == 0);
    CHECK(test_failures_do_not_drop_menu() == 0);
    CHECK(test_invalid_contracts() == 0);
    puts("common menu runtime tests: PASS");
    return 0;
}
