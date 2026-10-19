/* RetroPicoES entry point for the single native Keen Dreams target. */

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>

#include "refkeen.h"
#include "be_st_cfg.h"
#include "backend/filesystem/be_filesystem_mods.h"

#include "compat.h"
#include "frontend.h"
#include "startup_port.h"

/* These normally live in actual_main.c, which is deliberately outside the
 * Pico source set because it owns the SDL launcher and command-line UI. */
int g_be_argc;
const char **g_be_argv;
void (*be_lastSetMainFuncPtr)(void);
const char *be_main_arg_datadir = NULL;
const char *be_main_arg_newcfgdir = NULL;
BE_GameVer_T refkeen_current_gamever;
int retropicoes_refkeen_find_game_installation(void);

int main(void)
{
    static char program_name[] = "KDREAMS.EXE";
    static char *arguments[] = {program_name, NULL};
    jmp_buf exit_target;
    int launched_from_loader;
    int frontend_stage;
    volatile int result = 0;

    if (compat_init() != 0) {
        printf("REFKEEN ERROR: platform initialization failed\n");
        return 1;
    }

    launched_from_loader = retropicoes_launched_from_bootloader();
    frontend_stage = retropicoes_refkeen_frontend_boot();
    if (frontend_stage != RETROPICOES_REFKEEN_FRONTEND_BOOT_OK) {
        printf("REFKEEN ERROR: frontend boot failed at stage %d\n",
               frontend_stage);
        compat_shutdown();
        return 2;
    }

    retropicoes_refkeen_set_exit_target(&exit_target);
    if (setjmp(exit_target) == 0) {
        BE_ST_InitCommon();
        g_refKeenCfg.isBilinear = false;
        g_refKeenCfg.scaleFactor = 1;
        printf("REFKEEN VIDEO: single-stage scaling, text coverage enabled\n");
        BE_Cross_PrepareGameInstallations();
        printf("REFKEEN DATA: compatible installations=%d\n",
               retropicoes_refkeen_find_game_installation());
        printf("REFKEEN STAGE: game init begin\n");
        BE_Cross_InitGame(BE_GAMEVER_LAST);
        printf("REFKEEN STAGE: game init complete\n");
        BE_Cross_SetModPath(NULL);
        printf("REFKEEN STAGE: game main begin\n");
        BE_Cross_StartGame(1, arguments, NULL);
    } else {
        result = retropicoes_refkeen_exit_requested() ? 0 : 3;
    }
    retropicoes_refkeen_clear_exit_target();

    /* The game may return normally or leave via BE_ST_QuickExit.  Both paths
     * close the shared lifecycle and invoke Keen's registered ShutdownId once. */
    (void)retropicoes_refkeen_frontend_finished();
    retropicoes_refkeen_frontend_shutdown();
    BE_ST_ShutdownAll();
    compat_shutdown();

    if (launched_from_loader ||
        retropicoes_refkeen_frontend_loader_requested())
        retropicoes_return_to_bootloader();

    return result;
}
