#ifndef __MATRIX_DRIVER_H
#define __MATRIX_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "keyboard_config.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KEY_STATE_RELEASED = 0,
    KEY_STATE_PRESSED  = 1
} KeyState_t;

typedef struct {
    uint8_t     key_index;
    uint8_t     row;
    uint8_t     col;
    KeyState_t  state;
    uint32_t    timestamp;
} KeyEvent_t;

typedef enum {
    DEBOUNCE_STABLE     = 0,
    DEBOUNCE_DEBOUNCING = 1
} DebounceStatus_t;

typedef struct {
    DebounceStatus_t status;
    uint8_t          timer;
    bool             stable_state;    /* last confirmed stable reading */
    bool             pending_state;   /* state that triggered debounce */
} DebounceState_t;

typedef struct {

    uint8_t raw[MATRIX_ROWS];

    uint8_t debounced[MATRIX_ROWS];

    DebounceState_t debounce[MATRIX_ROWS][MATRIX_COLS];

    bool changed;

    uint32_t scan_count;
} MatrixState_t;

typedef struct {
    KeyEvent_t  buffer[KEY_EVENT_QUEUE_SIZE];
    uint8_t     head;
    uint8_t     tail;
    uint8_t     count;
} KeyEventQueue_t;

void Matrix_Init(void);

void Matrix_Scan(void);

void Matrix_DebounceTask(void);

bool Matrix_GetEvent(KeyEvent_t *event);

bool Matrix_HasEvent(void);

const MatrixState_t* Matrix_GetState(void);

KeyState_t Matrix_GetKeyState(uint8_t row, uint8_t col);

uint8_t Matrix_ToKeyIndex(uint8_t row, uint8_t col);

void Matrix_Reset(void);

#ifdef __cplusplus
}
#endif

#endif
