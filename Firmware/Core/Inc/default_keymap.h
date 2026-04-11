#ifndef __DEFAULT_KEYMAP_H
#define __DEFAULT_KEYMAP_H

#include "keycode_defs.h"
#include "keyboard_config.h"

static const keycode_t KEYMAP_LEFT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {

    { KC_GRAVE,  KC_1,      KC_2,      KC_3       },
    { KC_TAB,    KC_Q,      KC_W,      KC_E       },
    { KC_ESC,    KC_A,      KC_S,      KC_D       },
    { KC_LSFT,   KC_Z,      KC_X,      KC_C       },
};

static const keycode_t KEYMAP_LEFT_LAYER1[MATRIX_ROWS][MATRIX_COLS] = {

    { KC_TRNS,   KC_F1,     KC_F2,     KC_F3      },
    { KC_TRNS,   KC_NO,     KC_UP,     KC_NO      },
    { KC_TRNS,   KC_LEFT,   KC_DOWN,   KC_RIGHT   },
    { KC_TRNS,   KC_NO,     KC_NO,     KC_NO      },
};

static const keycode_t KEYMAP_RIGHT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {

    { KC_4,      KC_5,      KC_6,      KC_BSPC    },
    { KC_R,      KC_T,      KC_Y,      KC_U       },
    { KC_F,      KC_G,      KC_H,      KC_ENT     },
    { KC_V,      KC_B,      KC_SPC,    MO(1)      },
};

static const keycode_t KEYMAP_RIGHT_LAYER1[MATRIX_ROWS][MATRIX_COLS] = {

    { KC_F4,          KC_F5,          KC_F6,          KC_DEL          },
    { MC_PREV_TRACK,  MC_PLAY_PAUSE,  MC_NEXT_TRACK,  MC_STOP         },
    { MC_VOLUME_DOWN, MC_MUTE,        MC_VOLUME_UP,   KC_TRNS         },
    { MC_AL_BROWSER,  MC_AL_CALCULATOR, KC_TRNS,      KC_TRNS         },
};

#if(0)

static const keycode_t KEYMAP_LEFT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {

    { KC_GRAVE,KC_1,    KC_2,     KC_3,    KC_4,     KC_5      },
    { KC_TAB,  KC_Q,    KC_W,     KC_E,    KC_R,     KC_T      },
    { KC_ESC,  KC_A,    KC_S,     KC_D,    KC_F,     KC_G      },
    { KC_LSFT, KC_Z,    KC_X,     KC_C,    KC_V,     KC_B      },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },
    { KC_LCTRL, KC_LGUI, KC_LALT,  KC_SPC,  KC_NO,    KC_NO     },
    { MO(1),   KC_ENT,  KC_BSPC,  KC_DEL,  KC_NO,    KC_NO     },
};

static const keycode_t KEYMAP_LEFT_LAYER1[MATRIX_ROWS][MATRIX_COLS] = {
    { KC_TRNS, KC_F1,   KC_F2,    KC_F3,   KC_F4,    KC_F5     },
    { KC_TRNS, KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },
    { KC_TRNS, KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },
    { KC_TRNS, KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },
    { KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_NO,    KC_NO     },
    { KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_NO,    KC_NO     },
};

static const keycode_t KEYMAP_RIGHT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {

    { KC_6,    KC_7,    KC_8,     KC_9,        KC_0,         KC_MINUS     },
    { KC_Y,    KC_U,    KC_I,     KC_O,        KC_P,         KC_BACKSLASH },
    { KC_H,    KC_J,    KC_K,     KC_L,        KC_SEMICOLON, KC_QUOTE     },
    { KC_N,    KC_M,    KC_COMMA, KC_DOT,      KC_SLASH,     KC_RSFT      },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,       KC_NO,        KC_NO        },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,       KC_NO,        KC_NO        },
    { KC_SPC,  KC_RALT, KC_RGUI,  KC_RCTRL,     KC_NO,        KC_NO        },
    { KC_ENT,  KC_BSPC, KC_DEL,   MO(1),       KC_NO,        KC_NO        },
};

static const keycode_t KEYMAP_RIGHT_LAYER1[MATRIX_ROWS][MATRIX_COLS] = {
    { KC_F6,   KC_F7,   KC_F8,    KC_F9,       KC_F10,       KC_F11       },
    { KC_PGUP, KC_HOME, KC_UP,    KC_END,      KC_NO,        KC_F12       },
    { KC_PGDN, KC_LEFT, KC_DOWN,  KC_RIGHT,    KC_NO,        KC_NO        },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,       KC_NO,        KC_TRNS      },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,       KC_NO,        KC_NO        },
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,       KC_NO,        KC_NO        },
    { KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS,     KC_NO,        KC_NO        },
    { KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS,     KC_NO,        KC_NO        },
};
#endif
#endif
