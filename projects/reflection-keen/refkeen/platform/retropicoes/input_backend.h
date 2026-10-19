#ifndef REFKEEN_RETROPICOES_INPUT_BACKEND_H
#define REFKEEN_RETROPICOES_INPUT_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

int retropicoes_input_init(void);
void retropicoes_input_shutdown(void);
int retropicoes_input_ready(void);
int retropicoes_input_menu_pressed(void);

#ifdef __cplusplus
}
#endif

#endif
