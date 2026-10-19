#ifndef REFKEEN_RETROPICOES_STARTUP_PORT_H
#define REFKEEN_RETROPICOES_STARTUP_PORT_H

#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Install the outer runtime's escape target.  The caller owns the jmp_buf and
 * must keep it alive until retropicoes_refkeen_clear_exit_target() is called.
 * This is intentionally separate from ReflectionHLE's g_be_mainFuncJumpBuffer:
 * that buffer implements DOS exec chaining inside the game, while this one
 * leaves the entire game and returns control to the Pico frontend. */
void retropicoes_refkeen_set_exit_target(jmp_buf *target);
void retropicoes_refkeen_clear_exit_target(void);

/* Non-zero after BE_ST_QuickExit/BE_ST_HandleExit requested an outer exit.
 * Primarily useful to make teardown robust when a fatal error happens before
 * the target has been installed. */
int retropicoes_refkeen_exit_requested(void);

#ifdef __cplusplus
}
#endif

#endif
