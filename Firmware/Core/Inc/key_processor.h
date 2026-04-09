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

/* Current state of all pressed keys for HID report building */
typedef struct {
    uint8_t     modifiers;                  /* Modifier bitmask */
    uint8_t     pressed_keys[6];            /* Up to 6 keycodes (6KRO) */
    uint8_t     pressed_count;              /* Number of keys currently in array */
    bool        changed;                    /* Report needs sending */
    uint8_t     active_layer;               /* Current active layer */
    uint32_t    layer_state;                /* Bitmask of active layers */
} KeyProcessorState_t;

void KeyProcessor_Init(void);

void KeyProcessor_HandleLocalEvent(const KeyEvent_t *event);
/**
 * Process a key event from matrix scan
 * Resolves keycode from keymap, updates internal state
 */
void KeyProcessor_HandleRemoteEvent(const KeyEvent_t *event);

/**
 * Get current modifier state
 */
uint8_t KeyProcessor_GetModifiers(void);

/**
 * Get currently pressed keycodes (max 6)
 * Returns number of keys copied to buffer
 */
uint8_t KeyProcessor_GetPressedKeys(uint8_t *keys_out, uint8_t max_keys);

/**
 * Check if report state has changed since last query
 */
bool KeyProcessor_HasChanged(void);

/**
 * Clear the changed flag
 */
void KeyProcessor_ClearChanged(void);

/**
 * Get pointer to internal state (for HID reporter)
 */
const KeyProcessorState_t* KeyProcessor_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_PROCESSOR_H */
