#ifndef __AUTO_SHIFT_H
#define __AUTO_SHIFT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "keycode_defs.h"
#include "matrix_driver.h"

#ifndef AUTO_SHIFT_TERM_MS
#define AUTO_SHIFT_TERM_MS      175U
#endif

#define AUTO_SHIFT_MAX_PENDING  6U

typedef enum {
    AS_IDLE = 0,
    AS_PENDING,
    AS_SHIFTED,
    AS_NORMAL,
} AutoShiftKeyState_t;

typedef struct {
    AutoShiftKeyState_t state;
    uint8_t             key_index;
    keycode_t           kc;
    bool                is_remote;
    uint32_t            press_tick;
} AutoShiftSlot_t;

typedef struct {
    bool     enabled;
    uint32_t term_ms;
    bool     shift_alpha;
    bool     shift_numeric;
    bool     shift_special;
} AutoShiftConfig_t;

typedef void (*AutoShiftFireCb_t)(keycode_t kc, KeyState_t state,
                                   bool shifted, bool is_remote);

void AutoShift_Init(void);

void AutoShift_SetEnabled(bool enabled);
bool AutoShift_IsEnabled(void);
void AutoShift_SetTerm(uint32_t term_ms);
void AutoShift_Configure(const AutoShiftConfig_t *cfg);
void AutoShift_GetConfig(AutoShiftConfig_t *cfg_out);

bool AutoShift_HandlePress(uint8_t key_index, keycode_t kc, bool is_remote);
bool AutoShift_HandleRelease(uint8_t key_index);

void AutoShift_Process(void);

void AutoShift_NotifyOtherKeyPress(void);

bool AutoShift_IsPending(uint8_t key_index);

void AutoShift_RegisterCallback(AutoShiftFireCb_t cb);

bool AutoShift_IsAutoShiftable(keycode_t kc);

/* Số phím đang ở trạng thái AS_SHIFTED (để track LSHIFT) */
uint8_t AutoShift_GetShiftedCount(void);

#ifdef __cplusplus
}
#endif

#endif
