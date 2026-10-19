#ifndef SPACE_INVADERS_SETTINGS_ADAPTER_H
#define SPACE_INVADERS_SETTINGS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum space_invaders_settings_action {
    SPACE_INVADERS_SETTINGS_RESUME = 0,
    SPACE_INVADERS_SETTINGS_SAVE,
    SPACE_INVADERS_SETTINGS_LOAD,
    SPACE_INVADERS_SETTINGS_RESET,
    SPACE_INVADERS_SETTINGS_ROM_LIST,
    SPACE_INVADERS_SETTINGS_LOADER
} space_invaders_settings_action_t;

space_invaders_settings_action_t space_invaders_settings_run(void);

#ifdef __cplusplus
}
#endif
#endif
