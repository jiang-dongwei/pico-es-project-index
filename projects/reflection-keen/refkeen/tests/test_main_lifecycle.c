#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "be_cross.h"
#include "be_st_cfg.h"
#include "frontend.h"
#include "startup_port.h"

int refkeen_port_main(void);
RefKeenConfig g_refKeenCfg;

static char g_trace[128];
static size_t g_trace_len;
static jmp_buf *g_target;
static int g_mode;

static void mark(char value)
{
    g_trace[g_trace_len++] = value;
    g_trace[g_trace_len] = '\0';
}

int compat_init(void) { mark('I'); return 0; }
void compat_shutdown(void) { mark('X'); }
int retropicoes_launched_from_bootloader(void) { mark('B'); return 0; }
void retropicoes_return_to_bootloader(void) { mark('R'); }
int retropicoes_refkeen_frontend_boot(void) { mark('F'); return 0; }
retropicoes_menu_result_t retropicoes_refkeen_frontend_finished(void)
{ mark('D'); return RETROPICOES_MENU_RESULT_OK; }
void retropicoes_refkeen_frontend_shutdown(void) { mark('Q'); }
int retropicoes_refkeen_frontend_loader_requested(void)
{ mark('L'); return g_mode == 1; }
void retropicoes_refkeen_set_exit_target(jmp_buf *target)
{ mark('J'); g_target = target; }
void retropicoes_refkeen_clear_exit_target(void) { mark('C'); g_target = NULL; }
int retropicoes_refkeen_exit_requested(void) { mark('E'); return 1; }
void BE_ST_InitCommon(void) { mark('N'); }
void BE_Cross_PrepareGameInstallations(void) { mark('P'); }
int retropicoes_refkeen_find_game_installation(void) { mark('K'); return 1; }
void BE_Cross_InitGame(int game_ver) { (void)game_ver; mark('G'); }
void BE_Cross_SetModPath(const char *path) { (void)path; mark('M'); }
void BE_Cross_StartGame(int argc, char **argv, void (*main_func)(void))
{
    (void)argc; (void)argv; (void)main_func;
    mark('S');
    if (g_mode == 1)
        longjmp(*g_target, 1);
}
void BE_ST_ShutdownAll(void) { mark('A'); }

static int check(const char *expected)
{
    if (strcmp(g_trace, expected) == 0)
        return 0;
    fprintf(stderr, "FAIL: expected %s, got %s\n", expected, g_trace);
    return 1;
}

int main(void)
{
    int failures = 0;

    g_mode = 0;
    g_trace_len = 0;
    g_trace[0] = '\0';
    failures += refkeen_port_main() != 0;
    failures += check("IBFJNPKGMSCDQAXL");

    g_mode = 1;
    g_trace_len = 0;
    g_trace[0] = '\0';
    failures += refkeen_port_main() != 0;
    failures += check("IBFJNPKGMSECDQAXLR");

    printf("REFKEEN MAIN LIFECYCLE: %s (2 paths)\n",
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
