#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "backend/filesystem/be_filesystem_gameinst.h"
#include "path_mapping.h"

void BE_Cross_PrepareAppPaths(void);
int retropicoes_refkeen_find_game_installation(void);

static BE_GameVerDetails_T g_versions[BE_GAMEVER_LAST];
const BE_GameVerDetails_T *g_be_gamever_ptrs[BE_GAMEVER_LAST];
int g_be_gameinstallations_num;
static int g_probe_count;
static int g_root_count;

int compat_dir_create(const char *path)
{
    assert(strcmp(path, "/SAVES/refkeen") == 0);
    return 0;
}

const char *retropicoes_path_role_base(retropicoes_path_role_t role)
{
    return role == RETROPICOES_PATH_ROLE_GAME_DATA
        ? "/data/refkeen" : "/SAVES/refkeen";
}

void BEL_Cross_AddRootPath(const char *path, const char *key, const char *name)
{
    assert(strcmp(path, "/data/refkeen") == 0);
    assert(strcmp(key, "refkeen") == 0);
    assert(strcmp(name, "Keen Dreams data") == 0);
    ++g_root_count;
}

void BEL_Cross_ConditionallyAddGameInstallation_WithReturnedErrMsg(
    const BE_GameVerDetails_T *details, const char *searchdir,
    const char *description, BE_TryAddGameInstallation_ErrorMsg_T *error)
{
    assert(details == &g_versions[g_probe_count]);
    assert(strcmp(searchdir, "/data/refkeen") == 0);
    assert(strcmp(description, "SD card") == 0);
    assert(error == NULL);
    ++g_probe_count;
    if (details->verId == BE_GAMEVER_KDREAMSE113)
        ++g_be_gameinstallations_num;
}

int main(void)
{
    for (int i = 0; i < BE_GAMEVER_LAST; ++i) {
        g_versions[i].verId = i;
        g_be_gamever_ptrs[i] = &g_versions[i];
    }
    BE_Cross_PrepareAppPaths();
    assert(g_root_count == 1);
    assert(retropicoes_refkeen_find_game_installation() == 1);
    assert(g_probe_count == BE_GAMEVER_LAST);
    puts("REFKEEN GAME INSTALLATION PATH: PASS");
    return 0;
}
