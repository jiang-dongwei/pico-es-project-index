#include "menu_runtime.h"

#include <string.h>

static retropicoes_menu_runtime_event_t
runtime_event(retropicoes_menu_event_type_t type,
              retropicoes_menu_item_id_t item,
              retropicoes_menu_result_t result)
{
    retropicoes_menu_runtime_event_t value = {type, item, result};
    return value;
}

int retropicoes_menu_lifecycle_valid(
    const retropicoes_menu_lifecycle_t *lifecycle)
{
    return lifecycle != NULL &&
           lifecycle->version == RETROPICOES_MENU_LIFECYCLE_VERSION &&
           lifecycle->struct_size >= sizeof(*lifecycle);
}

static int has_resume(const retropicoes_menu_ops_t *ops)
{
    return (ops->capabilities & RETROPICOES_MENU_CAP_RESUME) != 0u &&
           ops->resume != NULL;
}

retropicoes_menu_result_t
retropicoes_menu_runtime_open(retropicoes_menu_runtime_t *runtime,
                              const retropicoes_menu_ops_t *ops,
                              const retropicoes_menu_lifecycle_t *lifecycle,
                              size_t page_rows)
{
    retropicoes_menu_result_t result;
    retropicoes_menu_ops_t ops_copy;
    retropicoes_menu_lifecycle_t lifecycle_copy;

    if (runtime == NULL) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }
    if (!retropicoes_menu_contract_valid(ops) ||
        !has_resume(ops) ||
        (lifecycle != NULL && !retropicoes_menu_lifecycle_valid(lifecycle))) {
        memset(runtime, 0, sizeof(*runtime));
        return RETROPICOES_MENU_RESULT_INVALID;
    }

    ops_copy = *ops;
    if (lifecycle != NULL) {
        lifecycle_copy = *lifecycle;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->ops = ops_copy;
    if (lifecycle != NULL) {
        runtime->lifecycle = lifecycle_copy;
        runtime->has_lifecycle = 1;
    }
    runtime->last_item = RETROPICOES_MENU_ITEM_COUNT;
    runtime->last_result = RETROPICOES_MENU_RESULT_OK;

    if (!retropicoes_system_menu_open(&runtime->menu, &runtime->ops,
                                      page_rows)) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }
    if (runtime->has_lifecycle && runtime->lifecycle.enter_menu != NULL) {
        result = runtime->lifecycle.enter_menu(runtime->lifecycle.context);
        if (result != RETROPICOES_MENU_RESULT_OK) {
            runtime->menu.open = 0;
            runtime->last_result = RETROPICOES_MENU_RESULT_FAILED;
            return runtime->last_result;
        }
    }
    runtime->open = 1;
    return RETROPICOES_MENU_RESULT_OK;
}

static int is_submenu(retropicoes_menu_item_id_t item)
{
    return item == RETROPICOES_MENU_ITEM_CONTROLS ||
           item == RETROPICOES_MENU_ITEM_STATE ||
           item == RETROPICOES_MENU_ITEM_SETTINGS;
}

static int is_terminal_transfer(retropicoes_menu_item_id_t item)
{
    return item == RETROPICOES_MENU_ITEM_ROM_LIST ||
           item == RETROPICOES_MENU_ITEM_RETURN_LOADER;
}

static retropicoes_menu_runtime_event_t
fail_and_stay_open(retropicoes_menu_runtime_t *runtime,
                   retropicoes_menu_item_id_t item,
                   retropicoes_menu_result_t result)
{
    runtime->menu.open = 1;
    runtime->open = 1;
    runtime->last_item = item;
    runtime->last_result = result;
    return runtime_event(RETROPICOES_MENU_EVENT_ERROR, item, result);
}

static retropicoes_menu_runtime_event_t
leave_after_action(retropicoes_menu_runtime_t *runtime,
                   retropicoes_menu_item_id_t item)
{
    retropicoes_menu_result_t result = RETROPICOES_MENU_RESULT_OK;

    if (runtime->has_lifecycle && runtime->lifecycle.leave_menu != NULL) {
        result = runtime->lifecycle.leave_menu(runtime->lifecycle.context);
    }
    if (result != RETROPICOES_MENU_RESULT_OK) {
        return fail_and_stay_open(runtime, item,
                                  RETROPICOES_MENU_RESULT_FAILED);
    }
    runtime->menu.open = 0;
    runtime->open = 0;
    runtime->last_item = item;
    runtime->last_result = RETROPICOES_MENU_RESULT_OK;
    return runtime_event(RETROPICOES_MENU_EVENT_CLOSE, item,
                         RETROPICOES_MENU_RESULT_OK);
}

static retropicoes_menu_runtime_event_t
activate(retropicoes_menu_runtime_t *runtime,
         retropicoes_menu_item_id_t item)
{
    retropicoes_menu_result_t result =
        retropicoes_menu_invoke(&runtime->ops, item);

    runtime->last_item = item;
    runtime->last_result = result;
    if (result == RETROPICOES_MENU_RESULT_CANCELLED) {
        runtime->menu.open = 1;
        runtime->open = 1;
        return runtime_event(RETROPICOES_MENU_EVENT_REDRAW, item, result);
    }
    if (result != RETROPICOES_MENU_RESULT_OK) {
        return fail_and_stay_open(runtime, item, result);
    }
    if (is_submenu(item)) {
        return runtime_event(RETROPICOES_MENU_EVENT_REDRAW, item, result);
    }
    if (is_terminal_transfer(item)) {
        runtime->menu.open = 0;
        runtime->open = 0;
        return runtime_event(RETROPICOES_MENU_EVENT_CLOSE, item, result);
    }
    return leave_after_action(runtime, item);
}

retropicoes_menu_runtime_event_t
retropicoes_menu_runtime_update(retropicoes_menu_runtime_t *runtime,
                                retropicoes_ui_actions_t pressed_actions)
{
    retropicoes_system_menu_event_t state_event;

    if (runtime == NULL || !runtime->open) {
        return runtime_event(RETROPICOES_MENU_EVENT_NONE,
                             RETROPICOES_MENU_ITEM_COUNT,
                             RETROPICOES_MENU_RESULT_INVALID);
    }

    state_event = retropicoes_system_menu_update(&runtime->menu,
                                                 pressed_actions);
    if (state_event.type == RETROPICOES_MENU_EVENT_CLOSE) {
        /* Cancel/Menu is the same safe path as selecting Resume. */
        return activate(runtime, RETROPICOES_MENU_ITEM_RESUME);
    }
    if (state_event.type == RETROPICOES_MENU_EVENT_ACTIVATE) {
        return activate(runtime, state_event.item);
    }
    return runtime_event(state_event.type, state_event.item,
                         RETROPICOES_MENU_RESULT_OK);
}

int retropicoes_menu_runtime_is_open(
    const retropicoes_menu_runtime_t *runtime)
{
    return runtime != NULL && runtime->open;
}

const char *retropicoes_menu_result_name(retropicoes_menu_result_t result)
{
    switch (result) {
    case RETROPICOES_MENU_RESULT_OK: return "OK";
    case RETROPICOES_MENU_RESULT_CANCELLED: return "CANCELLED";
    case RETROPICOES_MENU_RESULT_UNAVAILABLE: return "UNAVAILABLE";
    case RETROPICOES_MENU_RESULT_FAILED: return "FAILED";
    case RETROPICOES_MENU_RESULT_INVALID: return "INVALID";
    default: return "INVALID";
    }
}
