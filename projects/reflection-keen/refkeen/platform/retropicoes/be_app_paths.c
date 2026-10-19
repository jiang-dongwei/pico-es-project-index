/* ReflectionHLE application paths for the RetroPicoES Keen Dreams port. */

#include <string.h>

#include "compat.h"
#include "path_mapping.h"

#include "backend/filesystem/be_filesystem_app_paths.h"
#include "backend/filesystem/be_filesystem_gameinst.h"
#include "backend/filesystem/be_filesystem_len_bounds.h"

/* Avoid be_filesystem_root_paths.h here: it includes the upstream
 * BEL_Cross_IsDir inline whose Windows/Unix-only body has no Pico return path.
 * This is the exact public declaration implemented by root_paths.c. */
void BEL_Cross_AddRootPath(
    const TCHAR *root_path, const char *root_path_key,
    const char *root_path_name);

TCHAR g_be_appDataPath[BE_CROSS_PATH_LEN_BOUND];
TCHAR g_be_appNewCfgPath[BE_CROSS_PATH_LEN_BOUND];

void BE_Cross_PrepareAppPaths(void)
{
    const char *save_root =
        retropicoes_path_role_base(RETROPICOES_PATH_ROLE_SAVE_ROOT);
    const char *game_data =
        retropicoes_path_role_base(RETROPICOES_PATH_ROLE_GAME_DATA);

    /* The board image/Loader owns /SAVES. This port owns its child. The FatFS
     * compat operation is recursive and idempotent. */
    (void)compat_dir_create(save_root);

    strncpy(g_be_appDataPath, save_root, sizeof(g_be_appDataPath) - 1u);
    g_be_appDataPath[sizeof(g_be_appDataPath) - 1u] = '\0';
    memcpy(g_be_appNewCfgPath, g_be_appDataPath, sizeof(g_be_appDataPath));

    /* Upstream scans this one installation root against all seven supported
     * Keen Dreams data signatures. */
    BEL_Cross_AddRootPath(game_data, "refkeen", "Keen Dreams data");
}

int retropicoes_refkeen_find_game_installation(void)
{
    const char *game_data =
        retropicoes_path_role_base(RETROPICOES_PATH_ROLE_GAME_DATA);

    for (int i = 0; i < BE_GAMEVER_LAST; ++i)
        BEL_Cross_ConditionallyAddGameInstallation(
            g_be_gamever_ptrs[i], game_data, "SD card");

    return g_be_gameinstallations_num;
}
