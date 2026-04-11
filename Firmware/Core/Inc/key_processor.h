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

    /*
     * rollover_overflow: true khi số phím đang nhấn vượt quá 6.
     * HID reporter sẽ gửi ErrorRollOver (0x01) trên tất cả 6 slots
     * thay vì im lặng drop phím. Cờ này được clear khi pressed_count
     * giảm xuống <= 6.
     */
    bool        rollover_overflow;
} KeyProcessorState_t;

void KeyProcessor_Init(void);

void KeyProcessor_HandleLocalEvent(const KeyEvent_t *event);

void KeyProcessor_HandleRemoteEvent(const KeyEvent_t *event);

uint8_t KeyProcessor_GetModifiers(void);

uint8_t KeyProcessor_GetPressedKeys(uint8_t *keys_out, uint8_t max_keys);

bool KeyProcessor_HasChanged(void);

void KeyProcessor_ClearChanged(void);

const KeyProcessorState_t* KeyProcessor_GetState(void);

#ifdef __cplusplus
}
#endif

#endif
