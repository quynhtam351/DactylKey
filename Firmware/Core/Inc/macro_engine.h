#ifndef __MACRO_ENGINE_H
#define __MACRO_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "keycode_defs.h"
#include "keyboard_config.h"

/*
 * Macro Engine
 * ════════════
 * Supports up to MAX_MACROS (32) macros, each with up to
 * MAX_MACRO_STEPS (64) steps.
 *
 * Macro keycode encoding:
 *   0x3XYZ: X=0 (reserved), YZ = macro_id (0..31)
 *   Example: macro 5 → 0x3005
 *
 * Step types:
 *   MACRO_STEP_PRESS   : press a key
 *   MACRO_STEP_RELEASE : release a key
 *   MACRO_STEP_TAP     : press + release (convenience)
 *   MACRO_STEP_DELAY   : wait N ms (keycode field = ms)
 *   MACRO_STEP_END     : marks end of macro
 */

typedef enum {
    MACRO_STEP_END     = 0x00U,
    MACRO_STEP_PRESS   = 0x01U,
    MACRO_STEP_RELEASE = 0x02U,
    MACRO_STEP_TAP     = 0x03U,
    MACRO_STEP_DELAY   = 0x04U,
} MacroStepType_t;

typedef struct {
    uint8_t     type;       /* MacroStepType_t */
    uint8_t     kc_h;       /* keycode high byte */
    uint8_t     kc_l;       /* keycode low byte  */
    uint8_t     delay_ms;   /* delay after step  */
} __attribute__((packed)) MacroStep_t;

/* Full macro: array of steps (64 max) */
typedef struct {
    MacroStep_t steps[MAX_MACRO_STEPS];
    uint8_t     count;      /* number of valid steps */
} Macro_t;

/* ── Execution state ──────────────────────────────────────────── */

typedef enum {
    MACRO_EXEC_IDLE = 0,
    MACRO_EXEC_RUNNING,
    MACRO_EXEC_DELAY,
} MacroExecState_t;

/* ── Public API ───────────────────────────────────────────────── */

void MacroEngine_Init(void);

/*
 * Called when a macro key is pressed.
 * macro_id: 0..MAX_MACROS-1
 */
void MacroEngine_Trigger(uint8_t macro_id);

/*
 * Called every main loop iteration.
 * Advances execution of the current macro step.
 * Calls back into KeyProcessor to inject key events.
 */
void MacroEngine_Process(void);

/*
 * Returns true if a macro is currently executing.
 */
bool MacroEngine_IsBusy(void);

/*
 * Set a macro from a raw byte buffer (for Raw HID programming).
 * buf: array of MacroStep_t, step_count entries.
 */
bool MacroEngine_SetMacro(uint8_t macro_id, const MacroStep_t *steps, uint8_t step_count);

/*
 * Get macro step data for Raw HID read-back.
 */
bool MacroEngine_GetMacro(uint8_t macro_id, MacroStep_t *steps_out, uint8_t *count_out);

/*
 * Load macros from flash storage buffer.
 */
void MacroEngine_LoadFromFlash(const uint8_t *flash_data, uint16_t size);

/*
 * Serialize macros to buffer for flash storage.
 * Returns number of bytes written.
 */
uint16_t MacroEngine_SerializeToBuffer(uint8_t *buf, uint16_t buf_size);

#ifdef __cplusplus
}
#endif

#endif
