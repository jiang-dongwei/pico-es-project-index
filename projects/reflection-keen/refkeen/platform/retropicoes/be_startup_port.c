/* Pico replacement for backend/startup/be_startup_sdl.c. */

#include <setjmp.h>
#include <stddef.h>

#include "be_st.h"
#include "backend/input/be_input_tables.h"
#include "be_cross.h"

#include "be_events_port.h"
#include "startup_port.h"

void BEL_ST_ParseConfigFiles(void);
void BEL_ST_SaveConfigFiles(void);
void BE_ST_InitGfx(void);
void BE_ST_InitTiming(void);
void BE_ST_ShutdownAudio(void);
void BE_ST_ShutdownGfx(void);

/* be_input_sdl.c normally owns this.  That SDL TU is outside the Pico source
 * boundary, so the replacement platform owns the requested-sensor state. */
int g_stRequestedSensorsMask;

static jmp_buf *g_refkeenExitTarget;
static int g_refkeenExitRequested;

void retropicoes_refkeen_set_exit_target(jmp_buf *target)
{
    g_refkeenExitTarget = target;
    g_refkeenExitRequested = 0;
}

void retropicoes_refkeen_clear_exit_target(void)
{
    g_refkeenExitTarget = NULL;
}

int retropicoes_refkeen_exit_requested(void)
{
    return g_refkeenExitRequested;
}

void BE_ST_InitCommon(void)
{
    /* Preserve upstream's ordering: paths before config, key map before binds. */
    BE_Cross_PrepareAppPaths();
    BEL_ST_InitKeyMap();
    BEL_ST_ParseConfigFiles();
}

void BE_ST_PrepareForGameStartupWithoutAudio(void)
{
    g_stRequestedSensorsMask = 0;
    BE_ST_InitGfx();
    BE_ST_InitTiming();

    /* The Pico event source owns the physical-key edge history.  Starting a
     * game with an old mask would manufacture releases from the prior run. */
    retropicoes_refkeen_event_source_reset();
    BE_ST_ResetEmuMouse();
}

void BE_ST_InitSensors(void)
{
    /* RetroPicoES v2.1 exposes no motion sensors to this native game. */
}

void BE_ST_ShutdownAll(void)
{
    BE_ST_ShutdownAudio();
    BE_ST_ShutdownGfx();
}

void BE_ST_HandleExit(int status)
{
    (void)status;
    BE_ST_QuickExit();
}

void BE_ST_ExitWithErrorMsg(const char *msg)
{
    BE_Cross_LogMessage(BE_LOG_MSG_ERROR, "%s\n", msg ? msg : "ReflectionHLE error");
    BE_ST_QuickExit();
}

void BE_ST_QuickExit(void)
{
    jmp_buf *target;

    if (!g_refkeenExitRequested)
        BEL_ST_SaveConfigFiles();
    g_refkeenExitRequested = 1;

    target = g_refkeenExitTarget;
    if (target)
        longjmp(*target, 1);

    /* A fatal error may occur before main installs its escape target.  There is
     * no process to exit on bare metal, so leave a durable request for main and
     * stop here instead of returning into code that assumes QuickExit is
     * noreturn. */
    for (;;)
    {
    }
}
