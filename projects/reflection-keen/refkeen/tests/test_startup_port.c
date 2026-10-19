#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "be_cross.h"
#include "be_st.h"
#include "startup_port.h"

extern int g_stRequestedSensorsMask;

static int g_failures;
static int g_assertions;
static char g_trace[64];
static size_t g_trace_len;

static void mark(char value)
{
    if (g_trace_len + 1u < sizeof(g_trace)) {
        g_trace[g_trace_len++] = value;
        g_trace[g_trace_len] = '\0';
    }
}

#define CHECK(condition) do { \
    ++g_assertions; \
    if (!(condition)) { \
        ++g_failures; \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
    } \
} while (0)

void BE_Cross_PrepareAppPaths(void) { mark('P'); }
void BEL_ST_InitKeyMap(void) { mark('K'); }
void BEL_ST_ParseConfigFiles(void) { mark('C'); }
void BE_ST_InitGfx(void) { mark('G'); }
void BE_ST_InitTiming(void) { mark('T'); }
void retropicoes_refkeen_event_source_reset(void) { mark('E'); }
void BE_ST_ResetEmuMouse(void) { mark('M'); }
void BE_ST_ShutdownAudio(void) { mark('A'); }
void BE_ST_ShutdownGfx(void) { mark('V'); }
void BEL_ST_SaveConfigFiles(void) { mark('S'); }

void BE_Cross_LogMessage(BE_Log_Message_Class_T msg_class, const char *format, ...)
{
    (void)msg_class;
    (void)format;
    mark('L');
}

static void reset_trace(void)
{
    memset(g_trace, 0, sizeof(g_trace));
    g_trace_len = 0u;
}

int main(void)
{
    jmp_buf exit_target;
    volatile int jumped = 0;

    reset_trace();
    BE_ST_InitCommon();
    CHECK(0 == strcmp(g_trace, "PKC"));

    reset_trace();
    g_stRequestedSensorsMask = 7;
    BE_ST_PrepareForGameStartupWithoutAudio();
    CHECK(0 == strcmp(g_trace, "GTEM"));
    CHECK(0 == g_stRequestedSensorsMask);

    reset_trace();
    BE_ST_ShutdownAll();
    CHECK(0 == strcmp(g_trace, "AV"));

    reset_trace();
    retropicoes_refkeen_set_exit_target(&exit_target);
    if (0 == setjmp(exit_target))
        BE_ST_QuickExit();
    else
        jumped = 1;
    CHECK(1 == jumped);
    CHECK(0 == strcmp(g_trace, "S"));
    CHECK(1 == retropicoes_refkeen_exit_requested());

    /* A repeated request still escapes, but does not save config twice. */
    if (0 == setjmp(exit_target))
        BE_ST_HandleExit(1);
    CHECK(0 == strcmp(g_trace, "S"));

    retropicoes_refkeen_clear_exit_target();

    printf("REFKEEN STARTUP PORT: %s (%d assertions)\n",
           g_failures ? "FAIL" : "PASS", g_assertions);
    return g_failures ? 1 : 0;
}
