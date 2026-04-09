/* matrix_driver.h */
#ifndef __MATRIX_DRIVER_H
#define __MATRIX_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "keyboard_config.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================
 * PUBLIC TYPES
 * ========================================================= */

/**
 * @brief Trạng thái của một phím
 */
typedef enum {
    KEY_STATE_RELEASED = 0,
    KEY_STATE_PRESSED  = 1
} KeyState_t;

/**
 * @brief Một key event - sinh ra khi phím thay đổi trạng thái
 */
typedef struct {
    uint8_t     key_index;   /* Local key index: 0 đến (MATRIX_SIZE-1) */
    uint8_t     row;         /* Row number: 0 đến (MATRIX_ROWS-1) */
    uint8_t     col;         /* Col number: 0 đến (MATRIX_COLS-1) */
    KeyState_t  state;       /* KEY_STATE_PRESSED hoặc KEY_STATE_RELEASED */
    uint32_t    timestamp;   /* HAL_GetTick() lúc event xảy ra */
} KeyEvent_t;

/**
 * @brief Trạng thái debounce của một phím
 */
typedef enum {
    DEBOUNCE_STABLE     = 0,
    DEBOUNCE_DEBOUNCING = 1
} DebounceStatus_t;

/**
 * @brief Debounce state cho một phím
 */
typedef struct {
    DebounceStatus_t status;      /* Đang stable hay đang debounce */
    uint8_t          timer;       /* Đếm ngược (ms), max = DEBOUNCE_TIME_MS */
    bool             stable_state;/* Trạng thái đã được xác nhận (sau debounce) */
} DebounceState_t;

/**
 * @brief Toàn bộ state của ma trận một nửa bàn phím
 */
typedef struct {
    /* Raw readings từ GPIO - mỗi byte là 1 row, bit tương ứng col */
    uint8_t raw[MATRIX_ROWS];

    /* State sau debounce - đây là state "thật" */
    uint8_t debounced[MATRIX_ROWS];

    /* Debounce state cho từng phím */
    DebounceState_t debounce[MATRIX_ROWS][MATRIX_COLS];

    /* Flag báo có thay đổi state trong lần scan này */
    bool changed;

    /* Số lần scan đã thực hiện (debug) */
    uint32_t scan_count;
} MatrixState_t;

/* =========================================================
 * KEY EVENT QUEUE - Simple ring buffer
 * ========================================================= */
typedef struct {
    KeyEvent_t  buffer[KEY_EVENT_QUEUE_SIZE];
    uint8_t     head;    /* Index để đọc */
    uint8_t     tail;    /* Index để ghi */
    uint8_t     count;   /* Số event đang có trong queue */
} KeyEventQueue_t;

/* =========================================================
 * PUBLIC FUNCTION PROTOTYPES
 * ========================================================= */

/**
 * @brief  Khởi tạo matrix driver
 *         Gọi một lần trong main() sau khi MX_GPIO_Init()
 */
void Matrix_Init(void);

/**
 * @brief  Scan toàn bộ ma trận một lần
 *         Gọi từ TIM2 interrupt handler mỗi 1ms
 *         Thực hiện: GPIO scan → debounce → generate events
 */
void Matrix_Scan(void);

/**
 * @brief  Tick debounce - gọi cùng lúc với Matrix_Scan (mỗi 1ms)
 *         Đếm ngược timer debounce cho từng phím
 */
void Matrix_DebounceTask(void);

/**
 * @brief  Lấy một key event từ queue (non-blocking)
 * @param  event: Con trỏ để nhận event
 * @retval true nếu có event, false nếu queue rỗng
 */
bool Matrix_GetEvent(KeyEvent_t *event);

/**
 * @brief  Kiểm tra queue có event không
 * @retval true nếu có ít nhất 1 event
 */
bool Matrix_HasEvent(void);

/**
 * @brief  Lấy con trỏ đến matrix state hiện tại (read-only)
 *         Dùng để debug hoặc sync toàn bộ state qua UART
 * @retval Con trỏ đến MatrixState_t
 */
const MatrixState_t* Matrix_GetState(void);

/**
 * @brief  Lấy trạng thái của một phím cụ thể
 * @param  row: Row number
 * @param  col: Col number
 * @retval KEY_STATE_PRESSED hoặc KEY_STATE_RELEASED
 */
KeyState_t Matrix_GetKeyState(uint8_t row, uint8_t col);

/**
 * @brief  Convert row/col sang local key index
 * @param  row: Row number (0 đến MATRIX_ROWS-1)
 * @param  col: Col number (0 đến MATRIX_COLS-1)
 * @retval key_index (0 đến MATRIX_SIZE-1)
 */
uint8_t Matrix_ToKeyIndex(uint8_t row, uint8_t col);

/**
 * @brief  Reset toàn bộ matrix state
 *         Dùng khi reconnect hoặc khi cần sync lại
 */
void Matrix_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __MATRIX_DRIVER_H */
