#include "menu_contract.h"

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

typedef struct callback_log {
    unsigned calls;
    retropicoes_menu_result_t result;
} callback_log_t;

static retropicoes_menu_result_t record_call(void *context)
{
    callback_log_t *log = (callback_log_t *)context;
    ++log->calls;
    return log->result;
}

static retropicoes_menu_ops_t make_ops(callback_log_t *log)
{
    retropicoes_menu_ops_t ops;

    memset(&ops, 0, sizeof(ops));
    ops.contract_version = RETROPICOES_MENU_CONTRACT_VERSION;
    ops.struct_size = sizeof(ops);
    ops.context = log;
    return ops;
}

static int test_contract_validation(void)
{
    callback_log_t log = {0u, RETROPICOES_MENU_RESULT_OK};
    retropicoes_menu_ops_t ops = make_ops(&log);

    CHECK(retropicoes_menu_contract_valid(&ops));
    CHECK(!retropicoes_menu_contract_valid(NULL));
    ops.contract_version++;
    CHECK(!retropicoes_menu_contract_valid(&ops));
    ops = make_ops(&log);
    ops.struct_size--;
    CHECK(!retropicoes_menu_contract_valid(&ops));
    return 0;
}

static int test_fixed_item_order_and_capacity(void)
{
    callback_log_t log = {0u, RETROPICOES_MENU_RESULT_OK};
    retropicoes_menu_ops_t ops = make_ops(&log);
    retropicoes_menu_item_id_t items[RETROPICOES_MENU_ITEM_COUNT];
    size_t count;

    ops.capabilities = UINT32_MAX;
    ops.resume = record_call;
    ops.show_controls = record_call;
    ops.open_state_menu = record_call;
    ops.open_settings = record_call;
    ops.reset = record_call;
    ops.open_rom_list = record_call;
    ops.return_loader = record_call;

    count = retropicoes_menu_build_items(&ops, items,
                                         RETROPICOES_MENU_ITEM_COUNT);
    CHECK(count == RETROPICOES_MENU_ITEM_COUNT);
    CHECK(items[0] == RETROPICOES_MENU_ITEM_RESUME);
    CHECK(items[1] == RETROPICOES_MENU_ITEM_CONTROLS);
    CHECK(items[2] == RETROPICOES_MENU_ITEM_STATE);
    CHECK(items[3] == RETROPICOES_MENU_ITEM_SETTINGS);
    CHECK(items[4] == RETROPICOES_MENU_ITEM_RESET);
    CHECK(items[5] == RETROPICOES_MENU_ITEM_ROM_LIST);
    CHECK(items[6] == RETROPICOES_MENU_ITEM_RETURN_LOADER);

    items[0] = RETROPICOES_MENU_ITEM_COUNT;
    items[1] = RETROPICOES_MENU_ITEM_COUNT;
    count = retropicoes_menu_build_items(&ops, items, 2u);
    CHECK(count == RETROPICOES_MENU_ITEM_COUNT);
    CHECK(items[0] == RETROPICOES_MENU_ITEM_RESUME);
    CHECK(items[1] == RETROPICOES_MENU_ITEM_CONTROLS);
    return 0;
}

static int test_capability_and_callback_are_both_required(void)
{
    callback_log_t log = {0u, RETROPICOES_MENU_RESULT_OK};
    retropicoes_menu_ops_t ops = make_ops(&log);
    retropicoes_menu_item_id_t items[3];
    size_t count;

    ops.capabilities = RETROPICOES_MENU_CAP_RESUME |
                       RETROPICOES_MENU_CAP_CONTROLS |
                       RETROPICOES_MENU_CAP_RESET |
                       RETROPICOES_MENU_CAP_RETURN_LOADER;
    ops.resume = record_call;
    ops.show_controls = record_call;
    ops.reset = record_call;
    /* RETURN_LOADER is advertised but deliberately has no callback. */

    count = retropicoes_menu_build_items(&ops, items, 3u);
    CHECK(count == 3u);
    CHECK(items[0] == RETROPICOES_MENU_ITEM_RESUME);
    CHECK(items[1] == RETROPICOES_MENU_ITEM_CONTROLS);
    CHECK(items[2] == RETROPICOES_MENU_ITEM_RESET);
    return 0;
}

static int test_invoke_and_results(void)
{
    callback_log_t log = {0u, RETROPICOES_MENU_RESULT_CANCELLED};
    retropicoes_menu_ops_t ops = make_ops(&log);

    ops.capabilities = RETROPICOES_MENU_CAP_SETTINGS;
    ops.open_settings = record_call;
    CHECK(retropicoes_menu_invoke(&ops, RETROPICOES_MENU_ITEM_SETTINGS) ==
          RETROPICOES_MENU_RESULT_CANCELLED);
    CHECK(log.calls == 1u);
    CHECK(retropicoes_menu_invoke(&ops, RETROPICOES_MENU_ITEM_RESET) ==
          RETROPICOES_MENU_RESULT_UNAVAILABLE);
    CHECK(log.calls == 1u);
    CHECK(retropicoes_menu_invoke(&ops, RETROPICOES_MENU_ITEM_COUNT) ==
          RETROPICOES_MENU_RESULT_INVALID);

    log.result = RETROPICOES_MENU_RESULT_UNAVAILABLE;
    CHECK(retropicoes_menu_invoke(&ops, RETROPICOES_MENU_ITEM_SETTINGS) ==
          RETROPICOES_MENU_RESULT_FAILED);
    return 0;
}

static int test_item_names(void)
{
    CHECK(strcmp(retropicoes_menu_item_name(RETROPICOES_MENU_ITEM_RESUME),
                 "Resume") == 0);
    CHECK(strcmp(retropicoes_menu_item_name(
                     RETROPICOES_MENU_ITEM_RETURN_LOADER),
                 "Return to Loader") == 0);
    CHECK(retropicoes_menu_item_name(RETROPICOES_MENU_ITEM_COUNT) == NULL);
    return 0;
}

int main(void)
{
    CHECK(test_contract_validation() == 0);
    CHECK(test_fixed_item_order_and_capacity() == 0);
    CHECK(test_capability_and_callback_are_both_required() == 0);
    CHECK(test_invoke_and_results() == 0);
    CHECK(test_item_names() == 0);
    puts("common menu contract tests: PASS");
    return 0;
}
