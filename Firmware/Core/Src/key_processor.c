#include "key_processor.h"
#include "flash_keymap.h"
#include "macro_engine.h"
#include "tap_hold.h"
#include "auto_shift.h"
#include "media_keys.h"
#include "default_keymap.h"
#include <string.h>

static KeyProcessorState_t s_state;

static const keycode_t *s_keymap_left[MAX_LAYERS];
static const keycode_t *s_keymap_right[MAX_LAYERS];

static keycode_t s_active_local[MATRIX_ROWS * MATRIX_COLS];
static keycode_t s_active_remote[MATRIX_ROWS * MATRIX_COLS];
static uint8_t   s_overflow_count;

static uint8_t s_system_key_bitmap;
static bool    s_system_key_changed;

#define OVERFLOW_POOL_SIZE  8U
static uint8_t s_overflow_pool[OVERFLOW_POOL_SIZE];
static uint8_t s_overflow_pool_count;

static uint8_t s_num_active_layers;

#define TH_RESOLVE_MAP_SIZE   (MATRIX_ROWS * MATRIX_COLS)
static keycode_t s_th_resolved_kc[TH_RESOLVE_MAP_SIZE];

typedef enum {
    TT_IDLE = 0,
    TT_PRESSED,
    TT_MOMENTARY,
    TT_WAIT_2ND,
    TT_TOGGLED,
    TT_TOGGLE_ACTIVE,
} TT_State_t;

typedef struct {
    TT_State_t  state;
    uint8_t     target_layer;
    uint32_t    press_tick;
    uint32_t    release_tick;
    uint8_t     key_index;
} TT_Context_t;

#define TT_MAX_CONTEXTS     4U
#define TT_TAPPING_TERM_MS  200U

static TT_Context_t s_tt[TT_MAX_CONTEXTS];

static TT_Context_t *_TT_FindByLayer(uint8_t layer);
static TT_Context_t *_TT_FindByKeyIndex(uint8_t key_index);
static TT_Context_t *_TT_Alloc(void);
static void          _TT_Free(TT_Context_t *ctx);
static void          _TT_LayerOn(uint8_t layer);
static void          _TT_LayerOff(uint8_t layer);

static keycode_t _ResolveKeycodeLeft(uint8_t row, uint8_t col);
static keycode_t _ResolveKeycodeRight(uint8_t row, uint8_t col);
static void      _ProcessKeycode(keycode_t kc, KeyState_t state, bool is_remote,
                                  uint8_t key_index);
static void      _HandleBasicKey(keycode_t kc, KeyState_t state);
static void      _HandleLayerAction(keycode_t kc, KeyState_t state, uint8_t key_index);
static void      _HandleMacro(keycode_t kc, KeyState_t state);
static void      _HandleSystemKey(keycode_t kc, KeyState_t state);
static void      _HandleTapHold(keycode_t kc, KeyState_t state,
                                 uint8_t key_index, bool is_remote);
static void      _AddPressedKey(uint8_t hid_code);
static void      _RemovePressedKey(uint8_t hid_code);
static void      _UpdateLayerCache(void);
static void      _UpdateActiveLayer(void);
static void      _TapHoldCallback(keycode_t kc, KeyState_t state, bool is_remote);
static void      _AutoShiftCallback(keycode_t kc, KeyState_t state,
                                     bool shifted, bool is_remote);

static inline uint8_t _SystemUsageToBitmap(uint8_t usage)
{
    if (usage >= 0x81U && usage <= 0x83U) {
        return (uint8_t)(1U << (usage - 0x81U));
    }
    return 0U;
}

void KeyProcessor_Init(void)
{
    memset(&s_state, 0, sizeof(KeyProcessorState_t));
    memset(s_active_local,  0, sizeof(s_active_local));
    memset(s_active_remote, 0, sizeof(s_active_remote));
    memset(s_overflow_pool, 0, sizeof(s_overflow_pool));
    memset(s_th_resolved_kc, 0, sizeof(s_th_resolved_kc));
    memset(s_tt, 0, sizeof(s_tt));

    s_overflow_count      = 0;
    s_overflow_pool_count = 0;
    s_system_key_bitmap   = 0;
    s_system_key_changed  = false;

    for (int i = 0; i < MAX_LAYERS; i++) {
        s_keymap_left[i]  = NULL;
        s_keymap_right[i] = NULL;
    }

    for (uint8_t layer = 0U; layer < NUM_KEYMAP_LAYERS; layer++) {
        s_keymap_left[layer]  = FlashKeymap_GetLeft(layer);
        s_keymap_right[layer] = FlashKeymap_GetRight(layer);
    }

    s_state.active_layer = 0;
    s_state.layer_state  = 0x01U;

    _UpdateLayerCache();

    MacroEngine_Init();
    TapHold_Init();
    TapHold_RegisterCallback(_TapHoldCallback);
    AutoShift_Init();
    AutoShift_RegisterCallback(_AutoShiftCallback);
    MediaKeys_Init();
}

void KeyProcessor_HandleLocalEvent(const KeyEvent_t *event)
{
    if (event == NULL) return;
    if (event->row >= MATRIX_ROWS || event->col >= MATRIX_COLS) return;

    uint8_t   key_index = event->row * MATRIX_COLS + event->col;
    keycode_t kc;

    if (event->state == KEY_STATE_PRESSED) {

        if (!AutoShift_IsPending(key_index)) {
            AutoShift_NotifyOtherKeyPress();
        }
        if (!TapHold_IsPending(key_index)) {
            TapHold_NotifyOtherKeyPress();
        }

        kc = _ResolveKeycodeLeft(event->row, event->col);
        s_active_local[key_index] = kc;

        if (kc == KC_NO || kc == KC_TRANSPARENT) return;

        if (KC_TYPE(kc) == KC_TYPE_TAP_HOLD) {
            _ProcessKeycode(kc, KEY_STATE_PRESSED, false, key_index);
            return;
        }

        if (AutoShift_HandlePress(key_index, kc, false)) {
            return;
        }

        _ProcessKeycode(kc, KEY_STATE_PRESSED, false, key_index);

    } else {
        kc = s_active_local[key_index];
        s_active_local[key_index] = KC_NO;

        if (kc == KC_NO || kc == KC_TRANSPARENT) return;

        if (KC_TYPE(kc) == KC_TYPE_TAP_HOLD) {
            if (TapHold_HandleRelease(key_index)) return;
            if (key_index < TH_RESOLVE_MAP_SIZE &&
                s_th_resolved_kc[key_index] != KC_NO) {
                keycode_t resolved = s_th_resolved_kc[key_index];
                s_th_resolved_kc[key_index] = KC_NO;
                _ProcessKeycode(resolved, KEY_STATE_RELEASED, false, key_index);
            }
            return;
        }

        if (AutoShift_HandleRelease(key_index)) {
            return;
        }

        _ProcessKeycode(kc, KEY_STATE_RELEASED, false, key_index);
    }
}

void KeyProcessor_HandleRemoteEvent(const KeyEvent_t *event)
{
    if (event == NULL) return;
    if (event->row >= MATRIX_ROWS || event->col >= MATRIX_COLS) return;

    uint8_t   key_index = event->row * MATRIX_COLS + event->col;
    keycode_t kc;

    if (event->state == KEY_STATE_PRESSED) {

        if (!AutoShift_IsPending(key_index)) {
            AutoShift_NotifyOtherKeyPress();
        }
        if (!TapHold_IsPending(key_index)) {
            TapHold_NotifyOtherKeyPress();
        }

        kc = _ResolveKeycodeRight(event->row, event->col);
        s_active_remote[key_index] = kc;

        if (kc == KC_NO || kc == KC_TRANSPARENT) return;

        if (KC_TYPE(kc) == KC_TYPE_TAP_HOLD) {
            _ProcessKeycode(kc, KEY_STATE_PRESSED, true, key_index);
            return;
        }

        if (AutoShift_HandlePress(key_index, kc, true)) {
            return;
        }

        _ProcessKeycode(kc, KEY_STATE_PRESSED, true, key_index);

    } else {
        kc = s_active_remote[key_index];
        s_active_remote[key_index] = KC_NO;

        if (kc == KC_NO || kc == KC_TRANSPARENT) return;

        if (KC_TYPE(kc) == KC_TYPE_TAP_HOLD) {
            if (TapHold_HandleRelease(key_index)) return;
            if (key_index < TH_RESOLVE_MAP_SIZE &&
                s_th_resolved_kc[key_index] != KC_NO) {
                keycode_t resolved = s_th_resolved_kc[key_index];
                s_th_resolved_kc[key_index] = KC_NO;
                _ProcessKeycode(resolved, KEY_STATE_RELEASED, true, key_index);
            }
            return;
        }

        if (AutoShift_HandleRelease(key_index)) {
            return;
        }

        _ProcessKeycode(kc, KEY_STATE_RELEASED, true, key_index);
    }
}

void KeyProcessor_InjectKeycode(keycode_t kc, KeyState_t state)
{
    if (kc == KC_NO || kc == KC_TRANSPARENT) return;
    _ProcessKeycode(kc, state, false, 0xFFU);
}

uint8_t KeyProcessor_GetModifiers(void) { return s_state.modifiers; }

uint8_t KeyProcessor_GetPressedKeys(uint8_t *keys_out, uint8_t max_keys)
{
    uint8_t count = (s_state.pressed_count < max_keys)
                    ? s_state.pressed_count : max_keys;
    memcpy(keys_out, s_state.pressed_keys, count);
    return count;
}

bool KeyProcessor_HasChanged(void)   { return s_state.changed; }
void KeyProcessor_ClearChanged(void) { s_state.changed = false; }

const KeyProcessorState_t *KeyProcessor_GetState(void) { return &s_state; }

uint8_t KeyProcessor_GetSystemKey(void)          { return s_system_key_bitmap; }
bool    KeyProcessor_SystemKeyChanged(void)      { return s_system_key_changed; }
void    KeyProcessor_ClearSystemKeyChanged(void) { s_system_key_changed = false; }

void KeyProcessor_TT_Process(void)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0U; i < TT_MAX_CONTEXTS; i++) {
        TT_Context_t *ctx = &s_tt[i];

        switch (ctx->state) {
        case TT_PRESSED:
            if ((now - ctx->press_tick) >= TT_TAPPING_TERM_MS) {
                ctx->state = TT_MOMENTARY;
            }
            break;

        case TT_WAIT_2ND:
            if ((now - ctx->release_tick) >= TT_TAPPING_TERM_MS) {
                _TT_LayerOff(ctx->target_layer);
                _TT_Free(ctx);
            }
            break;

        default:
            break;
        }
    }
}

static void _UpdateLayerCache(void)
{
    s_num_active_layers = 0U;
    for (uint8_t i = 0U; i < MAX_LAYERS; i++) {
        if (s_keymap_left[i] != NULL || s_keymap_right[i] != NULL) {
            s_num_active_layers = i + 1U;
        }
    }
    if (s_num_active_layers == 0U) s_num_active_layers = 1U;
}

static void _UpdateActiveLayer(void)
{
    s_state.active_layer = 0U;
    for (int layer = (int)s_num_active_layers - 1; layer > 0; layer--) {
        if (s_state.layer_state & (1UL << layer)) {
            s_state.active_layer = (uint8_t)layer;
            break;
        }
    }
}

static keycode_t _ResolveKeycodeLeft(uint8_t row, uint8_t col)
{
    for (int layer = (int)s_num_active_layers - 1; layer >= 0; layer--) {
        if (!(s_state.layer_state & (1UL << layer))) continue;
        if (s_keymap_left[layer] == NULL) continue;
        keycode_t kc = s_keymap_left[layer][row * MATRIX_COLS + col];
        if (kc != KC_TRANSPARENT) return kc;
    }
    return KC_NO;
}

static keycode_t _ResolveKeycodeRight(uint8_t row, uint8_t col)
{
    for (int layer = (int)s_num_active_layers - 1; layer >= 0; layer--) {
        if (!(s_state.layer_state & (1UL << layer))) continue;
        if (s_keymap_right[layer] == NULL) continue;
        keycode_t kc = s_keymap_right[layer][row * MATRIX_COLS + col];
        if (kc != KC_TRANSPARENT) return kc;
    }
    return KC_NO;
}

static void _ProcessKeycode(keycode_t kc, KeyState_t state,
                             bool is_remote, uint8_t key_index)
{
    uint8_t type = KC_TYPE(kc);

    switch (type) {
    case KC_TYPE_BASIC:
        _HandleBasicKey(kc, state);
        break;
    case KC_TYPE_LAYER:
        _HandleLayerAction(kc, state, key_index);
        break;
    case KC_TYPE_MACRO:
        _HandleMacro(kc, state);
        break;
    case KC_TYPE_SYSTEM:
        _HandleSystemKey(kc, state);
        break;
    case KC_TYPE_TAP_HOLD:
        _HandleTapHold(kc, state, key_index, is_remote);
        break;
    case KC_TYPE_MEDIA:
        MediaKeys_HandleKeycode(kc, state);
        s_state.changed = true;
        break;
    default:
        break;
    }
}

static void _HandleBasicKey(keycode_t kc, KeyState_t state)
{
    uint16_t hid_code = KC_DATA(kc);

    if (IS_MODIFIER(kc)) {
        uint8_t mod_bit = MODIFIER_BIT(kc);
        if (state == KEY_STATE_PRESSED)  s_state.modifiers |= mod_bit;
        else                             s_state.modifiers &= ~mod_bit;
        s_state.changed = true;
    } else {
        if (state == KEY_STATE_PRESSED)  _AddPressedKey((uint8_t)hid_code);
        else                             _RemovePressedKey((uint8_t)hid_code);
    }
}

static void _HandleLayerAction(keycode_t kc, KeyState_t state, uint8_t key_index)
{
    uint8_t action       = LAYER_ACTION(kc);
    uint8_t target_layer = LAYER_TARGET(kc);
    if (target_layer >= MAX_LAYERS) return;

    switch (action) {

    case LAYER_ACTION_MO:
        if (state == KEY_STATE_PRESSED)
            s_state.layer_state |= (1UL << target_layer);
        else
            s_state.layer_state &= ~(1UL << target_layer);
        s_state.layer_state |= 0x01U;
        break;

    case LAYER_ACTION_TG:
        if (state == KEY_STATE_PRESSED) {
            s_state.layer_state ^= (1UL << target_layer);
            s_state.layer_state |= 0x01U;
        }
        break;

    case LAYER_ACTION_DF:
        if (state == KEY_STATE_PRESSED)
            s_state.layer_state = (1UL << target_layer);
        break;

    case LAYER_ACTION_TT:
    {
        if (state == KEY_STATE_PRESSED) {

            TT_Context_t *existing = _TT_FindByLayer(target_layer);

            if (existing != NULL && existing->state == TT_TOGGLE_ACTIVE) {
                _TT_LayerOff(target_layer);
                _TT_Free(existing);
                break;
            }

            if (existing != NULL && existing->state == TT_WAIT_2ND) {
                existing->state     = TT_TOGGLED;
                existing->key_index = key_index;
                break;
            }

            TT_Context_t *ctx = _TT_Alloc();
            if (ctx == NULL) {
                s_state.layer_state |= (1UL << target_layer);
                s_state.layer_state |= 0x01U;
                break;
            }

            ctx->state        = TT_PRESSED;
            ctx->target_layer = target_layer;
            ctx->press_tick   = HAL_GetTick();
            ctx->key_index    = key_index;

            _TT_LayerOn(target_layer);

        } else {

            TT_Context_t *ctx = _TT_FindByKeyIndex(key_index);
            if (ctx == NULL) {
                s_state.layer_state &= ~(1UL << target_layer);
                s_state.layer_state |= 0x01U;
                break;
            }

            switch (ctx->state) {

            case TT_PRESSED:
            {
                uint32_t held = HAL_GetTick() - ctx->press_tick;
                if (held < TT_TAPPING_TERM_MS) {
                    ctx->state        = TT_WAIT_2ND;
                    ctx->release_tick = HAL_GetTick();
                } else {
                    _TT_LayerOff(target_layer);
                    _TT_Free(ctx);
                }
                break;
            }

            case TT_MOMENTARY:
                _TT_LayerOff(target_layer);
                _TT_Free(ctx);
                break;

            case TT_TOGGLED:
                ctx->state = TT_TOGGLE_ACTIVE;
                break;

            case TT_TOGGLE_ACTIVE:
                break;

            case TT_WAIT_2ND:
                break;

            default:
                _TT_Free(ctx);
                break;
            }
        }
        break;
    }

    default:
        break;
    }

    _UpdateActiveLayer();
}

static void _HandleMacro(keycode_t kc, KeyState_t state)
{
    if (state != KEY_STATE_PRESSED) return;
    uint8_t macro_id = (uint8_t)(KC_DATA(kc) & 0xFFU);
    MacroEngine_Trigger(macro_id);
}

static void _HandleSystemKey(keycode_t kc, KeyState_t state)
{
    uint8_t usage = (uint8_t)(KC_DATA(kc) & 0xFFU);

    if (state == KEY_STATE_PRESSED) {
        s_system_key_bitmap |= _SystemUsageToBitmap(usage);
    } else {
        s_system_key_bitmap &= ~_SystemUsageToBitmap(usage);
    }

    s_system_key_changed = true;
    s_state.changed      = true;
}

static void _HandleTapHold(keycode_t kc, KeyState_t state,
                            uint8_t key_index, bool is_remote)
{
    (void)is_remote;
    uint8_t th_id = (uint8_t)(KC_DATA(kc) & 0xFFU);
    if (state == KEY_STATE_PRESSED) {
        TapHold_HandlePress(key_index, th_id, is_remote);
    }
}

static void _AddPressedKey(uint8_t hid_code)
{
    for (uint8_t i = 0U; i < s_state.pressed_count; i++) {
        if (s_state.pressed_keys[i] == hid_code) return;
    }
    for (uint8_t i = 0U; i < s_overflow_pool_count; i++) {
        if (s_overflow_pool[i] == hid_code) return;
    }

    if (s_state.pressed_count < 6U) {
        s_state.pressed_keys[s_state.pressed_count++] = hid_code;
    } else {
        if (s_overflow_pool_count < OVERFLOW_POOL_SIZE) {
            s_overflow_pool[s_overflow_pool_count++] = hid_code;
            s_overflow_count++;
        }
    }

    s_state.rollover_overflow = (s_overflow_count > 0U);
    s_state.changed = true;
}

static void _RemovePressedKey(uint8_t hid_code)
{
    for (uint8_t i = 0U; i < s_state.pressed_count; i++) {
        if (s_state.pressed_keys[i] == hid_code) {
            for (uint8_t j = i; j < s_state.pressed_count - 1U; j++) {
                s_state.pressed_keys[j] = s_state.pressed_keys[j + 1U];
            }
            s_state.pressed_count--;
            s_state.pressed_keys[s_state.pressed_count] = 0U;

            if (s_overflow_pool_count > 0U) {
                uint8_t promoted = s_overflow_pool[0];
                for (uint8_t k = 0U; k < s_overflow_pool_count - 1U; k++) {
                    s_overflow_pool[k] = s_overflow_pool[k + 1U];
                }
                s_overflow_pool_count--;
                s_overflow_count--;
                s_state.pressed_keys[s_state.pressed_count++] = promoted;
            }

            s_state.rollover_overflow = (s_overflow_count > 0U);
            s_state.changed = true;
            return;
        }
    }

    for (uint8_t i = 0U; i < s_overflow_pool_count; i++) {
        if (s_overflow_pool[i] == hid_code) {
            for (uint8_t k = i; k < s_overflow_pool_count - 1U; k++) {
                s_overflow_pool[k] = s_overflow_pool[k + 1U];
            }
            s_overflow_pool_count--;
            s_overflow_count--;
            s_state.rollover_overflow = (s_overflow_count > 0U);
            s_state.changed = true;
            return;
        }
    }
}

static void _TapHoldCallback(keycode_t kc, KeyState_t state, bool is_remote)
{
    if (state == KEY_STATE_PRESSED) {
        uint8_t ki = TapHold_GetPendingKeyIndex();
        if (ki < TH_RESOLVE_MAP_SIZE) {
            s_th_resolved_kc[ki] = kc;
        }
    } else {
        uint8_t ki = TapHold_GetPendingKeyIndex();
        if (ki < TH_RESOLVE_MAP_SIZE) {
            s_th_resolved_kc[ki] = KC_NO;
        }
    }

    _ProcessKeycode(kc, state, is_remote, 0xFFU);
}

static void _AutoShiftCallback(keycode_t kc, KeyState_t state,
                                bool shifted, bool is_remote)
{
    if (shifted) {
        if (state == KEY_STATE_PRESSED) {
            s_state.modifiers |= MOD_BIT_LSHIFT;
            s_state.changed    = true;
            _HandleBasicKey(kc, KEY_STATE_PRESSED);
        } else {
            _HandleBasicKey(kc, KEY_STATE_RELEASED);

            bool shift_held_elsewhere = false;
            for (uint8_t i = 0U; i < MATRIX_ROWS * MATRIX_COLS; i++) {
                keycode_t active_l = s_active_local[i];
                if (active_l == KC_LSHIFT || active_l == KC_LSFT) {
                    shift_held_elsewhere = true;
                    break;
                }
            }
            if (!shift_held_elsewhere) {
                for (uint8_t i = 0U; i < MATRIX_ROWS * MATRIX_COLS; i++) {
                    keycode_t active_r = s_active_remote[i];
                    if (active_r == KC_LSHIFT || active_r == KC_LSFT) {
                        shift_held_elsewhere = true;
                        break;
                    }
                }
            }
            if (!shift_held_elsewhere) {
                s_state.modifiers &= ~MOD_BIT_LSHIFT;
            }
            s_state.changed = true;
        }
    } else {
        _HandleBasicKey(kc, state);
    }

    (void)is_remote;
}

static TT_Context_t *_TT_FindByLayer(uint8_t layer)
{
    for (uint8_t i = 0U; i < TT_MAX_CONTEXTS; i++) {
        if (s_tt[i].state != TT_IDLE && s_tt[i].target_layer == layer) {
            return &s_tt[i];
        }
    }
    return NULL;
}

static TT_Context_t *_TT_FindByKeyIndex(uint8_t key_index)
{
    for (uint8_t i = 0U; i < TT_MAX_CONTEXTS; i++) {
        if (s_tt[i].state != TT_IDLE && s_tt[i].key_index == key_index) {
            return &s_tt[i];
        }
    }
    return NULL;
}

static TT_Context_t *_TT_Alloc(void)
{
    for (uint8_t i = 0U; i < TT_MAX_CONTEXTS; i++) {
        if (s_tt[i].state == TT_IDLE) {
            return &s_tt[i];
        }
    }
    return NULL;
}

static void _TT_Free(TT_Context_t *ctx)
{
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(TT_Context_t));
    }
}

static void _TT_LayerOn(uint8_t layer)
{
    s_state.layer_state |= (1UL << layer);
    s_state.layer_state |= 0x01U;
    _UpdateActiveLayer();
}

static void _TT_LayerOff(uint8_t layer)
{
    s_state.layer_state &= ~(1UL << layer);
    s_state.layer_state |= 0x01U;
    _UpdateActiveLayer();
}
