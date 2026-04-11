#include "auto_shift.h"
#include "stm32f4xx_hal.h"
#include "keycode_defs.h"
#include <string.h>

static AutoShiftConfig_t  s_cfg;
static AutoShiftSlot_t    s_slots[AUTO_SHIFT_MAX_PENDING];
static AutoShiftFireCb_t  s_callback = NULL;

static AutoShiftSlot_t *_FindSlot(uint8_t key_index);
static AutoShiftSlot_t *_AllocSlot(void);
static void             _FreeSlot(AutoShiftSlot_t *slot);
static void             _ResolveSlot(AutoShiftSlot_t *slot, bool as_shifted);

void AutoShift_Init(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
        s_slots[i].state = AS_IDLE;
    }
    s_callback = NULL;

    s_cfg.enabled       = true;
    s_cfg.term_ms       = AUTO_SHIFT_TERM_MS;
    s_cfg.shift_alpha   = true;
    s_cfg.shift_numeric = true;
    s_cfg.shift_special = true;
}

void AutoShift_SetEnabled(bool enabled)
{
    s_cfg.enabled = enabled;

    if (!enabled) {
        for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
            if (s_slots[i].state == AS_PENDING) {
                _ResolveSlot(&s_slots[i], false);
            }
        }
    }
}

bool AutoShift_IsEnabled(void)
{
    return s_cfg.enabled;
}

void AutoShift_SetTerm(uint32_t term_ms)
{
    if (term_ms > 0U && term_ms <= 1000U) {
        s_cfg.term_ms = term_ms;
    }
}

void AutoShift_Configure(const AutoShiftConfig_t *cfg)
{
    if (cfg == NULL) return;
    s_cfg = *cfg;
    if (s_cfg.term_ms == 0U || s_cfg.term_ms > 1000U) {
        s_cfg.term_ms = AUTO_SHIFT_TERM_MS;
    }
}

void AutoShift_GetConfig(AutoShiftConfig_t *cfg_out)
{
    if (cfg_out == NULL) return;
    *cfg_out = s_cfg;
}

bool AutoShift_IsAutoShiftable(keycode_t kc)
{
    if (!s_cfg.enabled) return false;
    if (KC_TYPE(kc) != KC_TYPE_BASIC) return false;

    uint16_t data = KC_DATA(kc);

    if (IS_MODIFIER(kc)) return false;

    if (s_cfg.shift_alpha && data >= 0x04U && data <= 0x1DU) return true;

    if (s_cfg.shift_numeric && data >= 0x1EU && data <= 0x27U) return true;

    if (s_cfg.shift_special) {
        switch (data) {
        case 0x002DU:  /* KC_MINUS */
        case 0x002EU:  /* KC_EQUAL */
        case 0x002FU:  /* KC_LBRACKET */
        case 0x0030U:  /* KC_RBRACKET */
        case 0x0031U:  /* KC_BACKSLASH */
        case 0x0033U:  /* KC_SEMICOLON */
        case 0x0034U:  /* KC_QUOTE */
        case 0x0035U:  /* KC_GRAVE */
        case 0x0036U:  /* KC_COMMA */
        case 0x0037U:  /* KC_DOT */
        case 0x0038U:  /* KC_SLASH */
            return true;
        default:
            break;
        }
    }

    return false;
}

bool AutoShift_HandlePress(uint8_t key_index, keycode_t kc, bool is_remote)
{
    if (!AutoShift_IsAutoShiftable(kc)) return false;

    /* Resolve tất cả pending slots khác ngay khi có phím mới */
    for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
        if (s_slots[i].state == AS_PENDING &&
            s_slots[i].key_index != key_index) {
            _ResolveSlot(&s_slots[i], false);
        }
    }

    AutoShiftSlot_t *slot = _AllocSlot();
    if (slot == NULL) {
        return false;
    }

    slot->state      = AS_PENDING;
    slot->key_index  = key_index;
    slot->kc         = kc;
    slot->is_remote  = is_remote;
    slot->press_tick = HAL_GetTick();

    return true;
}

bool AutoShift_HandleRelease(uint8_t key_index)
{
    AutoShiftSlot_t *slot = _FindSlot(key_index);
    if (slot == NULL || slot->state == AS_IDLE) return false;

    /* Lưu thông tin trước khi free slot */
    keycode_t saved_kc        = slot->kc;
    bool      saved_is_remote = slot->is_remote;
    AutoShiftKeyState_t saved_state = slot->state;

    switch (saved_state) {
    case AS_PENDING:
        /*
         * Nhả trước timeout → tap thường.
         * _ResolveSlot gửi PRESSED callback, rồi ta gửi RELEASED.
         */
        _ResolveSlot(slot, false);
        _FreeSlot(slot);
        if (s_callback != NULL) {
            s_callback(saved_kc, KEY_STATE_RELEASED, false, saved_is_remote);
        }
        break;

    case AS_SHIFTED:
        _FreeSlot(slot);
        if (s_callback != NULL) {
            s_callback(saved_kc, KEY_STATE_RELEASED, true, saved_is_remote);
        }
        break;

    case AS_NORMAL:
        _FreeSlot(slot);
        if (s_callback != NULL) {
            s_callback(saved_kc, KEY_STATE_RELEASED, false, saved_is_remote);
        }
        break;

    default:
        _FreeSlot(slot);
        break;
    }

    return true;
}

void AutoShift_Process(void)
{
    if (!s_cfg.enabled) return;

    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
        AutoShiftSlot_t *slot = &s_slots[i];
        if (slot->state != AS_PENDING) continue;

        if ((now - slot->press_tick) >= s_cfg.term_ms) {
            _ResolveSlot(slot, true);
        }
    }
}

void AutoShift_NotifyOtherKeyPress(void)
{
    for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
        if (s_slots[i].state == AS_PENDING) {
            _ResolveSlot(&s_slots[i], false);
        }
    }
}

bool AutoShift_IsPending(uint8_t key_index)
{
    AutoShiftSlot_t *slot = _FindSlot(key_index);
    return (slot != NULL && slot->state == AS_PENDING);
}

void AutoShift_RegisterCallback(AutoShiftFireCb_t cb)
{
    s_callback = cb;
}

uint8_t AutoShift_GetShiftedCount(void)
{
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
        if (s_slots[i].state == AS_SHIFTED) {
            count++;
        }
    }
    return count;
}

static AutoShiftSlot_t *_FindSlot(uint8_t key_index)
{
    for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
        if (s_slots[i].state != AS_IDLE &&
            s_slots[i].key_index == key_index) {
            return &s_slots[i];
        }
    }
    return NULL;
}

static AutoShiftSlot_t *_AllocSlot(void)
{
    for (uint8_t i = 0U; i < AUTO_SHIFT_MAX_PENDING; i++) {
        if (s_slots[i].state == AS_IDLE) {
            return &s_slots[i];
        }
    }
    return NULL;
}

static void _FreeSlot(AutoShiftSlot_t *slot)
{
    if (slot != NULL) {
        memset(slot, 0, sizeof(AutoShiftSlot_t));
        slot->state = AS_IDLE;
    }
}

static void _ResolveSlot(AutoShiftSlot_t *slot, bool as_shifted)
{
    if (slot->state != AS_PENDING) return;

    slot->state = as_shifted ? AS_SHIFTED : AS_NORMAL;

    if (s_callback != NULL) {
        s_callback(slot->kc, KEY_STATE_PRESSED,
                   as_shifted, slot->is_remote);
    }
}
