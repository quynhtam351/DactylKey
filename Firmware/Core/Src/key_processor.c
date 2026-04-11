#include "key_processor.h"
#include "default_keymap.h"
#include <string.h>

/*
 * Key Processor
 * ═════════════
 * Translates physical KeyEvents into HID state (modifiers + keycodes).
 *
 * 6KRO Overflow Handling:
 * When more than 6 non-modifier keys are held simultaneously, the HID
 * spec recommends sending ErrorRollOver (0x01) in all 6 keycode slots.
 * This signals the host that a rollover condition exists, rather than
 * silently dropping keys. The rollover_overflow flag is set/cleared
 * accordingly and checked by hid_reporter.c.
 *
 * Active Key Tracking (s_active_local / s_active_remote):
 * When a key is pressed, we resolve its keycode from the current layer
 * state and store it. On release, we use the STORED keycode (not a
 * fresh resolve). This ensures correct release even if the layer changes
 * between press and release.
 *
 * Layer State:
 * layer_state is a bitmask. Bit 0 (Layer 0) is always kept set.
 * The highest active layer that has a non-transparent keycode wins.
 */

static KeyProcessorState_t s_state;

static const keycode_t *s_keymap_left[MAX_LAYERS];
static const keycode_t *s_keymap_right[MAX_LAYERS];

static keycode_t s_active_local[MATRIX_ROWS * MATRIX_COLS];
static keycode_t s_active_remote[MATRIX_ROWS * MATRIX_COLS];

/*
 * Overflow tracking: count keys pressed beyond the 6KRO limit.
 * We need this to correctly handle release events when overflow > 0.
 */
static uint8_t s_overflow_count;

static keycode_t _ResolveKeycodeLeft(uint8_t row, uint8_t col);
static keycode_t _ResolveKeycodeRight(uint8_t row, uint8_t col);
static void      _HandleBasicKey(keycode_t kc, KeyState_t state);
static void      _HandleLayerAction(keycode_t kc, KeyState_t state);
static void      _AddPressedKey(uint8_t hid_code);
static void      _RemovePressedKey(uint8_t hid_code);
static void      _UpdateOverflowFlag(void);

/* ─────────────────────────────────────────────────────────────── */

void KeyProcessor_Init(void)
{
    memset(&s_state, 0, sizeof(KeyProcessorState_t));
    memset(s_active_local,  0, sizeof(s_active_local));
    memset(s_active_remote, 0, sizeof(s_active_remote));

    s_overflow_count = 0;

    s_keymap_left[0] = &KEYMAP_LEFT_LAYER0[0][0];
    s_keymap_left[1] = &KEYMAP_LEFT_LAYER1[0][0];
    for (int i = 2; i < MAX_LAYERS; i++) {
        s_keymap_left[i] = NULL;
    }

    s_keymap_right[0] = &KEYMAP_RIGHT_LAYER0[0][0];
    s_keymap_right[1] = &KEYMAP_RIGHT_LAYER1[0][0];
    for (int i = 2; i < MAX_LAYERS; i++) {
        s_keymap_right[i] = NULL;
    }

    s_state.active_layer = 0;
    s_state.layer_state  = 0x01;  /* Layer 0 always active */
}

/* ─────────────────────────────────────────────────────────────── */

void KeyProcessor_HandleLocalEvent(const KeyEvent_t *event)
{
    if (event == NULL) return;
    if (event->row >= MATRIX_ROWS || event->col >= MATRIX_COLS) return;

    uint8_t key_index = event->row * MATRIX_COLS + event->col;
    keycode_t kc;

    if (event->state == KEY_STATE_PRESSED) {
        kc = _ResolveKeycodeLeft(event->row, event->col);
        s_active_local[key_index] = kc;
    } else {
        kc = s_active_local[key_index];
        s_active_local[key_index] = KC_NO;
    }

    if (kc == KC_NO || kc == KC_TRANSPARENT) return;

    switch (KC_TYPE(kc)) {
    case KC_TYPE_BASIC:
        _HandleBasicKey(kc, event->state);
        break;
    case KC_TYPE_LAYER:
        _HandleLayerAction(kc, event->state);
        break;
    default:
        break;
    }
}

void KeyProcessor_HandleRemoteEvent(const KeyEvent_t *event)
{
    if (event == NULL) return;
    if (event->row >= MATRIX_ROWS || event->col >= MATRIX_COLS) return;

    uint8_t key_index = event->row * MATRIX_COLS + event->col;
    keycode_t kc;

    if (event->state == KEY_STATE_PRESSED) {
        kc = _ResolveKeycodeRight(event->row, event->col);
        s_active_remote[key_index] = kc;
    } else {
        kc = s_active_remote[key_index];
        s_active_remote[key_index] = KC_NO;
    }

    if (kc == KC_NO || kc == KC_TRANSPARENT) return;

    switch (KC_TYPE(kc)) {
    case KC_TYPE_BASIC:
        _HandleBasicKey(kc, event->state);
        break;
    case KC_TYPE_LAYER:
        _HandleLayerAction(kc, event->state);
        break;
    default:
        break;
    }
}

/* ─────────────────────────────────────────────────────────────── */

uint8_t KeyProcessor_GetModifiers(void)
{
    return s_state.modifiers;
}

uint8_t KeyProcessor_GetPressedKeys(uint8_t *keys_out, uint8_t max_keys)
{
    uint8_t count = (s_state.pressed_count < max_keys)
                    ? s_state.pressed_count : max_keys;
    memcpy(keys_out, s_state.pressed_keys, count);
    return count;
}

bool KeyProcessor_HasChanged(void)
{
    return s_state.changed;
}

void KeyProcessor_ClearChanged(void)
{
    s_state.changed = false;
}

const KeyProcessorState_t* KeyProcessor_GetState(void)
{
    return &s_state;
}

/* ─────────────────────────────────────────────────────────────── */

static keycode_t _ResolveKeycodeLeft(uint8_t row, uint8_t col)
{
    for (int layer = MAX_LAYERS - 1; layer >= 0; layer--) {
        if (!(s_state.layer_state & (1UL << layer))) continue;
        if (s_keymap_left[layer] == NULL) continue;

        keycode_t kc = s_keymap_left[layer][row * MATRIX_COLS + col];
        if (kc != KC_TRANSPARENT) {
            return kc;
        }
    }
    return KC_NO;
}

static keycode_t _ResolveKeycodeRight(uint8_t row, uint8_t col)
{
    for (int layer = MAX_LAYERS - 1; layer >= 0; layer--) {
        if (!(s_state.layer_state & (1UL << layer))) continue;
        if (s_keymap_right[layer] == NULL) continue;

        keycode_t kc = s_keymap_right[layer][row * MATRIX_COLS + col];
        if (kc != KC_TRANSPARENT) {
            return kc;
        }
    }
    return KC_NO;
}

/* ─────────────────────────────────────────────────────────────── */

static void _HandleBasicKey(keycode_t kc, KeyState_t state)
{
    uint16_t hid_code = KC_DATA(kc);

    if (IS_MODIFIER(kc)) {
        uint8_t mod_bit = MODIFIER_BIT(kc);
        if (state == KEY_STATE_PRESSED) {
            s_state.modifiers |= mod_bit;
        } else {
            s_state.modifiers &= ~mod_bit;
        }
        s_state.changed = true;

    } else {
        if (state == KEY_STATE_PRESSED) {
            _AddPressedKey((uint8_t)hid_code);
        } else {
            _RemovePressedKey((uint8_t)hid_code);
        }
    }
}

static void _HandleLayerAction(keycode_t kc, KeyState_t state)
{
    uint8_t action       = (KC_DATA(kc) >> 8) & 0x0F;
    uint8_t target_layer = (KC_DATA(kc) >> 4) & 0x0F;

    if (target_layer >= MAX_LAYERS) return;

    switch (action) {
    case 0x0: /* MO: momentary */
        if (state == KEY_STATE_PRESSED) {
            s_state.layer_state |= (1UL << target_layer);
        } else {
            s_state.layer_state &= ~(1UL << target_layer);
        }
        /* Layer 0 always active */
        s_state.layer_state |= 0x01;
        break;

    case 0x1: /* TG: toggle */
        if (state == KEY_STATE_PRESSED) {
            s_state.layer_state ^= (1UL << target_layer);
            s_state.layer_state |= 0x01;
        }
        break;

    case 0x2: /* DF: default */
        if (state == KEY_STATE_PRESSED) {
            s_state.layer_state = (1UL << target_layer);
        }
        break;

    default:
        break;
    }
}

/* ─────────────────────────────────────────────────────────────── */

/*
 * _AddPressedKey: add a HID keycode to the pressed list.
 *
 * If fewer than 6 keys are currently pressed, add normally.
 * If already at 6, increment the overflow counter and set the
 * rollover_overflow flag. The HID reporter will send ErrorRollOver.
 */
static void _AddPressedKey(uint8_t hid_code)
{
    /* Duplicate check: do not add a key already in the list */
    for (uint8_t i = 0; i < s_state.pressed_count && i < 6; i++) {
        if (s_state.pressed_keys[i] == hid_code) return;
    }

    if (s_state.pressed_count < 6) {
        s_state.pressed_keys[s_state.pressed_count] = hid_code;
        s_state.pressed_count++;
    } else {
        /*
         * 6KRO limit reached. Track overflow but do not add to
         * the 6-slot array. The array stays intact so we know
         * exactly which 6 keys were first pressed.
         */
        s_overflow_count++;
    }

    _UpdateOverflowFlag();
    s_state.changed = true;
}

/*
 * _RemovePressedKey: remove a HID keycode from the pressed list.
 *
 * Three cases:
 * 1. Key is in the 6-slot array → remove and shift, decrement count.
 *    If overflow_count > 0, we can't restore the overflowed keys
 *    (we didn't store them), so just decrement overflow_count.
 * 2. Key is NOT in the 6-slot array but overflow_count > 0 →
 *    it was an overflowed key, just decrement overflow_count.
 * 3. Key not found anywhere → ignore (spurious release).
 */
static void _RemovePressedKey(uint8_t hid_code)
{
    /* Search in the 6-slot array */
    for (uint8_t i = 0; i < s_state.pressed_count; i++) {
        if (s_state.pressed_keys[i] == hid_code) {
            /* Shift remaining keys left */
            for (uint8_t j = i; j < s_state.pressed_count - 1; j++) {
                s_state.pressed_keys[j] = s_state.pressed_keys[j + 1];
            }
            s_state.pressed_count--;
            s_state.pressed_keys[s_state.pressed_count] = 0;

            /*
             * If there were overflow keys, releasing a tracked key
             * frees a slot but we don't know which overflow key to
             * promote (we never stored them). Decrement overflow count
             * to keep the accounting correct.
             */
            if (s_overflow_count > 0) {
                s_overflow_count--;
            }

            _UpdateOverflowFlag();
            s_state.changed = true;
            return;
        }
    }

    /*
     * Key not in the 6-slot array. If we have overflow, it might
     * have been an overflowed key.
     */
    if (s_overflow_count > 0) {
        s_overflow_count--;
        _UpdateOverflowFlag();
        s_state.changed = true;
    }
}

/*
 * _UpdateOverflowFlag: sync rollover_overflow with s_overflow_count.
 * Also updates active_layer for display purposes.
 */
static void _UpdateOverflowFlag(void)
{
    s_state.rollover_overflow = (s_overflow_count > 0);

    /* Update active_layer: highest active non-zero layer */
    s_state.active_layer = 0;
    for (int layer = MAX_LAYERS - 1; layer > 0; layer--) {
        if (s_state.layer_state & (1UL << layer)) {
            s_state.active_layer = (uint8_t)layer;
            break;
        }
    }
}
