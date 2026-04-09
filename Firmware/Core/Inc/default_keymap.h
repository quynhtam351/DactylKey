#ifndef __DEFAULT_KEYMAP_H
#define __DEFAULT_KEYMAP_H

#include "keycode_defs.h"
#include "keyboard_config.h"


// ============================================================
// LEFT HALF KEYMAP (Master)
// ============================================================
static const keycode_t KEYMAP_LEFT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {
    //   Col0       Col1       Col2       Col3
    { KC_GRAVE,  KC_1,      KC_2,      KC_3       },  // Row 0
    { KC_TAB,    KC_Q,      KC_W,      KC_E       },  // Row 1
    { KC_ESC,    KC_A,      KC_S,      KC_D       },  // Row 2
    { KC_LSFT,   KC_Z,      KC_X,      KC_C       },  // Row 3
};

static const keycode_t KEYMAP_LEFT_LAYER1[MATRIX_ROWS][MATRIX_COLS] = {
    //   Col0       Col1       Col2       Col3
    { KC_TRNS,   KC_F1,     KC_F2,     KC_F3      },  // Row 0
    { KC_TRNS,   KC_NO,     KC_UP,     KC_NO      },  // Row 1
    { KC_TRNS,   KC_LEFT,   KC_DOWN,   KC_RIGHT   },  // Row 2
    { KC_TRNS,   KC_NO,     KC_NO,     KC_NO      },  // Row 3
};

// ============================================================
// RIGHT HALF KEYMAP (Slave)
// ============================================================
static const keycode_t KEYMAP_RIGHT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {
    //   Col0       Col1       Col2       Col3
    { KC_4,      KC_5,      KC_6,      KC_BSPC    },  // Row 0
    { KC_R,      KC_T,      KC_Y,      KC_U       },  // Row 1
    { KC_F,      KC_G,      KC_H,      KC_ENT     },  // Row 2
    { KC_V,      KC_B,      KC_SPC,    MO(1)      },  // Row 3
};

static const keycode_t KEYMAP_RIGHT_LAYER1[MATRIX_ROWS][MATRIX_COLS] = {
    //   Col0       Col1       Col2       Col3
    { KC_F4,     KC_F5,     KC_F6,     KC_DEL     },  // Row 0
    { KC_HOME,   KC_PGUP,   KC_PGDN,   KC_END     },  // Row 1
    { KC_NO,     KC_NO,     KC_NO,     KC_TRNS    },  // Row 2
    { KC_NO,     KC_NO,     KC_TRNS,   KC_TRNS    },  // Row 3
};


#if(0)
// ============================================================
// LEFT HALF KEYMAP (Master)
// ============================================================
static const keycode_t KEYMAP_LEFT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {
    //   Col0     Col1     Col2      Col3     Col4      Col5
    { KC_GRAVE,KC_1,    KC_2,     KC_3,    KC_4,     KC_5      },  // Row 0
    { KC_TAB,  KC_Q,    KC_W,     KC_E,    KC_R,     KC_T      },  // Row 1
    { KC_ESC,  KC_A,    KC_S,     KC_D,    KC_F,     KC_G      },  // Row 2
    { KC_LSFT, KC_Z,    KC_X,     KC_C,    KC_V,     KC_B      },  // Row 3
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },  // Row 4 (unused)
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,   KC_NO,    KC_NO     },  // Row 5 (unused)
    { KC_LCTRL, KC_LGUI, KC_LALT,  KC_SPC,  KC_NO,    KC_NO     },  // Row 6 (thumb)
    { MO(1),   KC_ENT,  KC_BSPC,  KC_DEL,  KC_NO,    KC_NO     },  // Row 7 (thumb)
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

// ============================================================
// RIGHT HALF KEYMAP (Slave)
// ============================================================
static const keycode_t KEYMAP_RIGHT_LAYER0[MATRIX_ROWS][MATRIX_COLS] = {
    //   Col0     Col1     Col2      Col3         Col4          Col5
    { KC_6,    KC_7,    KC_8,     KC_9,        KC_0,         KC_MINUS     },  // Row 0
    { KC_Y,    KC_U,    KC_I,     KC_O,        KC_P,         KC_BACKSLASH },  // Row 1
    { KC_H,    KC_J,    KC_K,     KC_L,        KC_SEMICOLON, KC_QUOTE     },  // Row 2
    { KC_N,    KC_M,    KC_COMMA, KC_DOT,      KC_SLASH,     KC_RSFT      },  // Row 3
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,       KC_NO,        KC_NO        },  // Row 4 (unused)
    { KC_NO,   KC_NO,   KC_NO,    KC_NO,       KC_NO,        KC_NO        },  // Row 5 (unused)
    { KC_SPC,  KC_RALT, KC_RGUI,  KC_RCTRL,     KC_NO,        KC_NO        },  // Row 6 (thumb)
    { KC_ENT,  KC_BSPC, KC_DEL,   MO(1),       KC_NO,        KC_NO        },  // Row 7 (thumb)
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
#endif // #if(0)
#endif
