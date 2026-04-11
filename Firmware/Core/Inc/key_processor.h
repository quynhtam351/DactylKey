#ifndef __KEY_PROCESSOR_H
#define __KEY_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "matrix_driver.h"
#include "keycode_defs.h"
#include "keyboard_config.h"

typedef struct {
    uint8_t     modifiers;
    uint8_t     pressed_keys[6];
    uint8_t     pressed_count;
    bool        changed;
    uint8_t     active_layer;
    uint32_t    layer_state;
    bool        rollover_overflow;
} KeyProcessorState_t;

void KeyProcessor_Init(void);

void KeyProcessor_HandleLocalEvent(const KeyEvent_t *event);
void KeyProcessor_HandleRemoteEvent(const KeyEvent_t *event);
void KeyProcessor_InjectKeycode(keycode_t kc, KeyState_t state);

uint8_t KeyProcessor_GetModifiers(void);
uint8_t KeyProcessor_GetPressedKeys(uint8_t *keys_out, uint8_t max_keys);

bool KeyProcessor_HasChanged(void);
void KeyProcessor_ClearChanged(void);

uint8_t KeyProcessor_GetSystemKey(void);
bool    KeyProcessor_SystemKeyChanged(void);
void    KeyProcessor_ClearSystemKeyChanged(void);

const KeyProcessorState_t *KeyProcessor_GetState(void);

void KeyProcessor_TT_Process(void);

#ifdef __cplusplus
}
#endif

#endif
