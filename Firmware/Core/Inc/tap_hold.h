#ifndef __TAP_HOLD_H
#define __TAP_HOLD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "keycode_defs.h"
#include "keyboard_config.h"
#include "matrix_driver.h"

#define MAX_TAP_HOLD_KEYS       32U
#define TAPPING_TERM_MS         200U

typedef enum {
    TH_HOLD_BASIC  = 0U,
    TH_HOLD_LAYER  = 1U,
} TapHoldType_t;

typedef struct {
    keycode_t       tap_kc;
    keycode_t       hold_kc;
} TapHoldEntry_t;

typedef enum {
    TH_STATE_IDLE    = 0,
    TH_STATE_PENDING,
    TH_STATE_TAP_ACTIVE,
    TH_STATE_HOLD_ACTIVE,
} TapHoldState_t;

typedef struct {
    TapHoldState_t  state;
    uint8_t         key_index;
    uint8_t         th_id;
    bool            is_remote;
    uint32_t        press_tick;
} TapHoldPending_t;

void TapHold_Init(void);

bool TapHold_SetEntry(uint8_t th_id, keycode_t tap_kc, keycode_t hold_kc);
bool TapHold_GetEntry(uint8_t th_id, TapHoldEntry_t *entry_out);

bool TapHold_HandlePress(uint8_t key_index, uint8_t th_id, bool is_remote);
bool TapHold_HandleRelease(uint8_t key_index);

void TapHold_NotifyOtherKeyPress(void);

void TapHold_Process(void);

bool TapHold_IsPending(uint8_t key_index);

/* M2 fix: query key_index hiện tại đang được xử lý bởi tap-hold */
uint8_t TapHold_GetPendingKeyIndex(void);

typedef void (*TapHoldFireCb_t)(keycode_t kc, KeyState_t state, bool is_remote);
void TapHold_RegisterCallback(TapHoldFireCb_t cb);

#ifdef __cplusplus
}
#endif

#endif
