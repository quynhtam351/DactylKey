#include "key_processor.h"
#include "default_keymap.h"
#include <string.h>

static KeyProcessorState_t s_state;

// Keymap pointers: LEFT (master/local) và RIGHT (slave/remote)
static const keycode_t *s_keymap_left[MAX_LAYERS];
static const keycode_t *s_keymap_right[MAX_LAYERS];

// Active keycodes cho local và remote
static keycode_t s_active_local[MATRIX_ROWS * MATRIX_COLS];
static keycode_t s_active_remote[MATRIX_ROWS * MATRIX_COLS];

// Private functions
static keycode_t _ResolveKeycodeLeft(uint8_t row, uint8_t col);
static keycode_t _ResolveKeycodeRight(uint8_t row, uint8_t col);
static void      _HandleBasicKey(keycode_t kc, KeyState_t state);
static void      _HandleLayerAction(keycode_t kc, KeyState_t state);
static void      _AddPressedKey(uint8_t hid_code);
static void      _RemovePressedKey(uint8_t hid_code);

void KeyProcessor_Init(void)
{
    memset(&s_state, 0, sizeof(KeyProcessorState_t));
    memset(s_active_local, 0, sizeof(s_active_local));
    memset(s_active_remote, 0, sizeof(s_active_remote));

    // LEFT keymap (master/local)
    s_keymap_left[0] = &KEYMAP_LEFT_LAYER0[0][0];
    s_keymap_left[1] = &KEYMAP_LEFT_LAYER1[0][0];
    for (int i = 2; i < MAX_LAYERS; i++) {
        s_keymap_left[i] = NULL;
    }

    // RIGHT keymap (slave/remote)
    s_keymap_right[0] = &KEYMAP_RIGHT_LAYER0[0][0];
    s_keymap_right[1] = &KEYMAP_RIGHT_LAYER1[0][0];
    for (int i = 2; i < MAX_LAYERS; i++) {
        s_keymap_right[i] = NULL;
    }

    s_state.active_layer = 0;
    s_state.layer_state  = 0x01;
}

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

    uint8_t type = KC_TYPE(kc);

    switch (type) {
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

    uint8_t type = KC_TYPE(kc);

    switch (type) {
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
    uint8_t action = (KC_DATA(kc) >> 8) & 0x0F;
    uint8_t target_layer = (KC_DATA(kc) >> 4) & 0x0F;

    if (target_layer >= MAX_LAYERS) return;

    switch (action) {
    case 0x0:  // MO - Momentary
        if (state == KEY_STATE_PRESSED) {
            s_state.layer_state |= (1UL << target_layer);
        } else {
            s_state.layer_state &= ~(1UL << target_layer);
        }
        s_state.layer_state |= 0x01;
        break;

    case 0x1:  // TG - Toggle
        if (state == KEY_STATE_PRESSED) {
            s_state.layer_state ^= (1UL << target_layer);
            s_state.layer_state |= 0x01;
        }
        break;

    case 0x2:  // DF - Default layer
        if (state == KEY_STATE_PRESSED) {
            s_state.layer_state = (1UL << target_layer);
        }
        break;

    default:
        break;
    }
}

static void _AddPressedKey(uint8_t hid_code)
{
    for (uint8_t i = 0; i < s_state.pressed_count; i++) {
        if (s_state.pressed_keys[i] == hid_code) return;
    }

    if (s_state.pressed_count < 6) {
        s_state.pressed_keys[s_state.pressed_count] = hid_code;
        s_state.pressed_count++;
        s_state.changed = true;
    }
}

static void _RemovePressedKey(uint8_t hid_code)
{
    for (uint8_t i = 0; i < s_state.pressed_count; i++) {
        if (s_state.pressed_keys[i] == hid_code) {
            for (uint8_t j = i; j < s_state.pressed_count - 1; j++) {
                s_state.pressed_keys[j] = s_state.pressed_keys[j + 1];
            }
            s_state.pressed_count--;
            s_state.pressed_keys[s_state.pressed_count] = 0;
            s_state.changed = true;
            return;
        }
    }
}