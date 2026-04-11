#include "tap_hold.h"
#include "stm32f4xx_hal.h"
#include <string.h>

static TapHoldEntry_t s_table[MAX_TAP_HOLD_KEYS];
static TapHoldPending_t s_pending;
static TapHoldFireCb_t s_callback = NULL;

static void _Resolve(bool as_tap);

void TapHold_Init(void)
{
    memset(s_table, 0, sizeof(s_table));
    memset(&s_pending, 0, sizeof(s_pending));
    s_pending.state = TH_STATE_IDLE;
    s_callback = NULL;
}

bool TapHold_SetEntry(uint8_t th_id, keycode_t tap_kc, keycode_t hold_kc)
{
    if (th_id >= MAX_TAP_HOLD_KEYS) return false;
    s_table[th_id].tap_kc  = tap_kc;
    s_table[th_id].hold_kc = hold_kc;
    return true;
}

bool TapHold_GetEntry(uint8_t th_id, TapHoldEntry_t *entry_out)
{
    if (th_id >= MAX_TAP_HOLD_KEYS || entry_out == NULL) return false;
    *entry_out = s_table[th_id];
    return true;
}

void TapHold_RegisterCallback(TapHoldFireCb_t cb)
{
    s_callback = cb;
}

bool TapHold_HandlePress(uint8_t key_index, uint8_t th_id, bool is_remote)
{
    if (th_id >= MAX_TAP_HOLD_KEYS) return false;

    if (s_pending.state == TH_STATE_PENDING) {
        _Resolve(false);
    }

    s_pending.state      = TH_STATE_PENDING;
    s_pending.key_index  = key_index;
    s_pending.th_id      = th_id;
    s_pending.is_remote  = is_remote;
    s_pending.press_tick = HAL_GetTick();

    return true;
}

bool TapHold_HandleRelease(uint8_t key_index)
{
    if (s_pending.state == TH_STATE_IDLE) return false;
    if (s_pending.key_index != key_index) return false;

    if (s_pending.state == TH_STATE_PENDING) {
        _Resolve(true);
        if (s_callback != NULL) {
            s_callback(s_table[s_pending.th_id].tap_kc,
                       KEY_STATE_RELEASED, s_pending.is_remote);
        }
    } else if (s_pending.state == TH_STATE_HOLD_ACTIVE) {
        if (s_callback != NULL) {
            s_callback(s_table[s_pending.th_id].hold_kc,
                       KEY_STATE_RELEASED, s_pending.is_remote);
        }
    } else if (s_pending.state == TH_STATE_TAP_ACTIVE) {
        if (s_callback != NULL) {
            s_callback(s_table[s_pending.th_id].tap_kc,
                       KEY_STATE_RELEASED, s_pending.is_remote);
        }
    }

    s_pending.state = TH_STATE_IDLE;
    return true;
}

void TapHold_NotifyOtherKeyPress(void)
{
    if (s_pending.state == TH_STATE_PENDING) {
        _Resolve(false);
    }
}

void TapHold_Process(void)
{
    if (s_pending.state != TH_STATE_PENDING) return;

    uint32_t elapsed = HAL_GetTick() - s_pending.press_tick;
    if (elapsed >= TAPPING_TERM_MS) {
        _Resolve(false);
    }
}

bool TapHold_IsPending(uint8_t key_index)
{
    return (s_pending.state == TH_STATE_PENDING &&
            s_pending.key_index == key_index);
}

/* M2 fix: expose key_index cho callback để lưu resolved keycode */
uint8_t TapHold_GetPendingKeyIndex(void)
{
    return s_pending.key_index;
}

static void _Resolve(bool as_tap)
{
    if (s_pending.state != TH_STATE_PENDING) return;

    keycode_t kc;
    if (as_tap) {
        kc = s_table[s_pending.th_id].tap_kc;
        s_pending.state = TH_STATE_TAP_ACTIVE;
    } else {
        kc = s_table[s_pending.th_id].hold_kc;
        s_pending.state = TH_STATE_HOLD_ACTIVE;
    }

    if (s_callback != NULL) {
        s_callback(kc, KEY_STATE_PRESSED, s_pending.is_remote);
    }
}
