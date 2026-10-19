#include "menu_contract.h"
#include "menu_input.h"
#include "menu_render.h"
#include "menu_runtime.h"
#include "menu_state.h"

#include <cstring>

int main()
{
    retropicoes_menu_ops_t ops{};
    retropicoes_menu_repeat_t repeat{};
    retropicoes_system_menu_state_t state{};
    retropicoes_menu_surface_t surface{};
    retropicoes_menu_runtime_t runtime{};
    const auto *labels = retropicoes_menu_controller_labels(
        RETROPICOES_CONTROLLER_NINTENDO);

    ops.contract_version = RETROPICOES_MENU_CONTRACT_VERSION;
    ops.struct_size = sizeof(ops);
    retropicoes_menu_repeat_reset(&repeat);
    (void)state;
    (void)surface;
    (void)runtime;
    return retropicoes_menu_contract_valid(&ops) &&
                   std::strcmp(labels->confirm, "A") == 0
               ? 0
               : 1;
}
