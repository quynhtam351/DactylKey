#ifndef __KEYCODE_DEFS_H
#define __KEYCODE_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Keycode Format (16-bit):
 * ┌─────────────────────────────────────────────┐
 * │ Bits [15:12] = Type                         │
 * │ Bits [11:0]  = Data                         │
 * └─────────────────────────────────────────────┘
 *
 * Type 0x0: Basic HID keycode (0x00-0xFF)
 * Type 0x1: Layer action
 * Type 0x2: Modifier + key combo
 * Type 0xF: Special (TRANSPARENT, NOOP)
 */

typedef uint16_t keycode_t;

/* ─── Type extraction macros ─── */
#define KC_TYPE(kc)             (((kc) >> 12) & 0x0F)
#define KC_DATA(kc)             ((kc) & 0x0FFF)

#define KC_TYPE_BASIC           0x0
#define KC_TYPE_LAYER           0x1
#define KC_TYPE_MOD_KEY         0x2
#define KC_TYPE_MACRO           0x3
#define KC_TYPE_SYSTEM          0x4
#define KC_TYPE_TAP_HOLD        0x5
#define KC_TYPE_SPECIAL         0xF

/* ─── Special keycodes ─── */
#define KC_NO                   0x0000
#define KC_TRANSPARENT          0xF000
#define KC_TRNS                 KC_TRANSPARENT

/* ─── Basic HID Keycodes (USB HID Usage Table) ─── */
/* Letters */
#define KC_A                    0x0004
#define KC_B                    0x0005
#define KC_C                    0x0006
#define KC_D                    0x0007
#define KC_E                    0x0008
#define KC_F                    0x0009
#define KC_G                    0x000A
#define KC_H                    0x000B
#define KC_I                    0x000C
#define KC_J                    0x000D
#define KC_K                    0x000E
#define KC_L                    0x000F
#define KC_M                    0x0010
#define KC_N                    0x0011
#define KC_O                    0x0012
#define KC_P                    0x0013
#define KC_Q                    0x0014
#define KC_R                    0x0015
#define KC_S                    0x0016
#define KC_T                    0x0017
#define KC_U                    0x0018
#define KC_V                    0x0019
#define KC_W                    0x001A
#define KC_X                    0x001B
#define KC_Y                    0x001C
#define KC_Z                    0x001D

/* Numbers */
#define KC_1                    0x001E
#define KC_2                    0x001F
#define KC_3                    0x0020
#define KC_4                    0x0021
#define KC_5                    0x0022
#define KC_6                    0x0023
#define KC_7                    0x0024
#define KC_8                    0x0025
#define KC_9                    0x0026
#define KC_0                    0x0027

/* Control keys */
#define KC_ENTER                0x0028
#define KC_ENT                  KC_ENTER
#define KC_ESCAPE               0x0029
#define KC_ESC                  KC_ESCAPE
#define KC_BACKSPACE            0x002A
#define KC_BSPC                 KC_BACKSPACE
#define KC_TAB                  0x002B
#define KC_SPACE                0x002C
#define KC_SPC                  KC_SPACE

/* Symbols */
#define KC_MINUS                0x002D
#define KC_EQUAL                0x002E
#define KC_LBRACKET             0x002F
#define KC_RBRACKET             0x0030
#define KC_BACKSLASH            0x0031
#define KC_SEMICOLON            0x0033
#define KC_QUOTE                0x0034
#define KC_GRAVE                0x0035
#define KC_COMMA                0x0036
#define KC_DOT                  0x0037
#define KC_SLASH                0x0038

/* Function keys */
#define KC_CAPSLOCK             0x0039
#define KC_F1                   0x003A
#define KC_F2                   0x003B
#define KC_F3                   0x003C
#define KC_F4                   0x003D
#define KC_F5                   0x003E
#define KC_F6                   0x003F
#define KC_F7                   0x0040
#define KC_F8                   0x0041
#define KC_F9                   0x0042
#define KC_F10                  0x0043
#define KC_F11                  0x0044
#define KC_F12                  0x0045

/* Navigation */
#define KC_PRINTSCREEN          0x0046
#define KC_SCROLLLOCK           0x0047
#define KC_PAUSE                0x0048
#define KC_INSERT               0x0049
#define KC_HOME                 0x004A
#define KC_PGUP                 0x004B
#define KC_DELETE               0x004C
#define KC_DEL                  KC_DELETE
#define KC_END                  0x004D
#define KC_PGDN                 0x004E
#define KC_RIGHT                0x004F
#define KC_LEFT                 0x0050
#define KC_DOWN                 0x0051
#define KC_UP                   0x0052

/* Modifiers (HID usage codes) */
#define KC_LCTRL                0x00E0
#define KC_LSHIFT               0x00E1
#define KC_LSFT                 KC_LSHIFT
#define KC_LALT                 0x00E2
#define KC_LGUI                 0x00E3
#define KC_RCTRL                0x00E4
#define KC_RSHIFT               0x00E5
#define KC_RSFT                 KC_RSHIFT
#define KC_RALT                 0x00E6
#define KC_RGUI                 0x00E7

/* ─── Modifier bitmask (for HID report byte 0) ─── */
#define MOD_BIT_LCTRL           0x01
#define MOD_BIT_LSHIFT          0x02
#define MOD_BIT_LALT            0x04
#define MOD_BIT_LGUI            0x08
#define MOD_BIT_RCTRL           0x10
#define MOD_BIT_RSHIFT          0x20
#define MOD_BIT_RALT            0x40
#define MOD_BIT_RGUI            0x80

/* ─── Layer action macros ─── */
/* MO(layer) - Momentary: active while held */
#define MO(layer)               (0x1000 | (0x0 << 8) | ((layer) & 0x0F) << 4)
/* TG(layer) - Toggle: toggle on/off */
#define TG(layer)               (0x1000 | (0x1 << 8) | ((layer) & 0x0F) << 4)
/* DF(layer) - Default: set default layer */
#define DF(layer)               (0x1000 | (0x2 << 8) | ((layer) & 0x0F) << 4)

/* ─── Helper: Is this a modifier keycode? ─── */
#define IS_MODIFIER(kc)         (KC_DATA(kc) >= 0xE0 && KC_DATA(kc) <= 0xE7)
#define MODIFIER_BIT(kc)        (1U << (KC_DATA(kc) - 0xE0))

/* ─── Helper: Is this a basic printable key? ─── */
#define IS_BASIC_KEY(kc)        (KC_TYPE(kc) == KC_TYPE_BASIC && \
                                 KC_DATA(kc) >= 0x04 && KC_DATA(kc) <= 0xDF)

#ifdef __cplusplus
}
#endif

#endif /* __KEYCODE_DEFS_H */