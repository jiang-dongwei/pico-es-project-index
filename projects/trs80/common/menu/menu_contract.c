#include "menu_contract.h"

typedef struct menu_item_contract {
    retropicoes_menu_item_id_t item;
    uint32_t capability;
    size_t callback_offset;
    const char *name;
} menu_item_contract_t;

#define CALLBACK_OFFSET(member) offsetof(retropicoes_menu_ops_t, member)

static const menu_item_contract_t menu_items[] = {
    {RETROPICOES_MENU_ITEM_RESUME, RETROPICOES_MENU_CAP_RESUME,
     CALLBACK_OFFSET(resume), "Resume"},
    {RETROPICOES_MENU_ITEM_CONTROLS, RETROPICOES_MENU_CAP_CONTROLS,
     CALLBACK_OFFSET(show_controls), "Controls"},
    {RETROPICOES_MENU_ITEM_STATE, RETROPICOES_MENU_CAP_STATE,
     CALLBACK_OFFSET(open_state_menu), "Save / Load"},
    {RETROPICOES_MENU_ITEM_SETTINGS, RETROPICOES_MENU_CAP_SETTINGS,
     CALLBACK_OFFSET(open_settings), "Settings"},
    {RETROPICOES_MENU_ITEM_RESET, RETROPICOES_MENU_CAP_RESET,
     CALLBACK_OFFSET(reset), "Reset Game"},
    {RETROPICOES_MENU_ITEM_ROM_LIST, RETROPICOES_MENU_CAP_ROM_LIST,
     CALLBACK_OFFSET(open_rom_list), "ROM List"},
    {RETROPICOES_MENU_ITEM_RETURN_LOADER,
     RETROPICOES_MENU_CAP_RETURN_LOADER, CALLBACK_OFFSET(return_loader),
     "Return to Loader"}
};

static const menu_item_contract_t *find_item(retropicoes_menu_item_id_t item)
{
    size_t i;

    for (i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); ++i) {
        if (menu_items[i].item == item) {
            return &menu_items[i];
        }
    }
    return NULL;
}

static retropicoes_menu_callback_t
item_callback(const retropicoes_menu_ops_t *ops,
              const menu_item_contract_t *item)
{
    const unsigned char *base = (const unsigned char *)ops;
    const retropicoes_menu_callback_t *callback =
        (const retropicoes_menu_callback_t *)(base + item->callback_offset);
    return *callback;
}

int retropicoes_menu_contract_valid(const retropicoes_menu_ops_t *ops)
{
    return ops != NULL &&
           ops->contract_version == RETROPICOES_MENU_CONTRACT_VERSION &&
           ops->struct_size >= sizeof(*ops);
}

size_t retropicoes_menu_build_items(const retropicoes_menu_ops_t *ops,
                                    retropicoes_menu_item_id_t *items,
                                    size_t item_capacity)
{
    size_t available = 0;
    size_t i;

    if (!retropicoes_menu_contract_valid(ops)) {
        return 0;
    }

    for (i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); ++i) {
        const menu_item_contract_t *item = &menu_items[i];
        if ((ops->capabilities & item->capability) == 0u ||
            item_callback(ops, item) == NULL) {
            continue;
        }
        if (items != NULL && available < item_capacity) {
            items[available] = item->item;
        }
        ++available;
    }
    return available;
}

retropicoes_menu_result_t
retropicoes_menu_invoke(const retropicoes_menu_ops_t *ops,
                        retropicoes_menu_item_id_t item_id)
{
    const menu_item_contract_t *item;
    retropicoes_menu_callback_t callback;
    retropicoes_menu_result_t result;

    if (!retropicoes_menu_contract_valid(ops)) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }

    item = find_item(item_id);
    if (item == NULL) {
        return RETROPICOES_MENU_RESULT_INVALID;
    }
    callback = item_callback(ops, item);
    if ((ops->capabilities & item->capability) == 0u || callback == NULL) {
        return RETROPICOES_MENU_RESULT_UNAVAILABLE;
    }

    result = callback(ops->context);
    if (result != RETROPICOES_MENU_RESULT_OK &&
        result != RETROPICOES_MENU_RESULT_CANCELLED &&
        result != RETROPICOES_MENU_RESULT_FAILED) {
        return RETROPICOES_MENU_RESULT_FAILED;
    }
    return result;
}

const char *retropicoes_menu_item_name(retropicoes_menu_item_id_t item_id)
{
    const menu_item_contract_t *item = find_item(item_id);
    return item != NULL ? item->name : NULL;
}
