#ifndef __KEYBOARD_CONFIG_H
#define __KEYBOARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================
 * KEYBOARD IDENTITY
 *============================================================*/
#define KEYBOARD_NAME           "Dactyl Ergonomic Keyboard"
#define KEYBOARD_VERSION        0x0100
#define FIRMWARE_VERSION        0x0100

/*============================================================
 * KEYBOARD ROLE CONFIGURATION
 *============================================================*/
#define KEYBOARD_ROLE_MASTER    0
#define KEYBOARD_ROLE_SLAVE     1
#define KEYBOARD_ROLE           KEYBOARD_ROLE_MASTER

/*============================================================
 * MATRIX DIMENSIONS
 *============================================================*/
#define MATRIX_ROWS             4
#define MATRIX_COLS             4
#define MATRIX_SIZE             (MATRIX_ROWS * MATRIX_COLS)

#define KEYS_PER_HALF           (MATRIX_ROWS * MATRIX_COLS)
#define TOTAL_KEYS              (KEYS_PER_HALF * 2)

/*============================================================
 * 🔧 PIN CONFIGURATION - EDIT HERE TO CHANGE PINS
 *============================================================*/

/* ----- ROW PINS (Output, directly on GPIO port) ----- */
#define ROW_GPIO_PORT           GPIOA

/* Định nghĩa từng pin ROW - Chỉ cần sửa ở đây */
#define ROW0_PIN                GPIO_PIN_0
#define ROW1_PIN                GPIO_PIN_1
#define ROW2_PIN                GPIO_PIN_2
#define ROW3_PIN                GPIO_PIN_3

/* Mảng ROW pins để sử dụng trong code */
#define ROW_PINS_ARRAY          { ROW0_PIN, ROW1_PIN, ROW2_PIN, ROW3_PIN }

/* Mask tất cả ROW pins để init cùng lúc */
#define ROW_ALL_PINS            (ROW0_PIN | ROW1_PIN | ROW2_PIN | ROW3_PIN)


/* ----- COLUMN PINS (Input với Pull-up) ----- */
#define COL_GPIO_PORT           GPIOB

/* Định nghĩa từng pin COL - Chỉ cần sửa ở đây */
#define COL0_PIN                GPIO_PIN_0
#define COL1_PIN                GPIO_PIN_1
#define COL2_PIN                GPIO_PIN_6
#define COL3_PIN                GPIO_PIN_5

/* Mảng COL pins để sử dụng trong code */
#define COL_PINS_ARRAY          { COL0_PIN, COL1_PIN, COL2_PIN, COL3_PIN }

/* Mask tất cả COL pins để init và đọc cùng lúc */
#define COL_ALL_PINS            (COL0_PIN | COL1_PIN | COL2_PIN | COL3_PIN)

/* Offset của COL pins trong thanh ghi IDR (nếu không bắt đầu từ bit 0) */
#define COL_PIN_OFFSET          0


/* ----- LED PINS ----- */
#define LED_STATUS_GPIO_PORT    GPIOC
#define LED_STATUS_PIN          GPIO_PIN_13

#define LED_LAYER_GPIO_PORT     GPIOB
#define LED_LAYER1_PIN          GPIO_PIN_12
#define LED_LAYER2_PIN          GPIO_PIN_13

#define LED_ALL_PINS            (LED_LAYER1_PIN | LED_LAYER2_PIN)


/*============================================================
 * TIMING CONFIGURATION
 *============================================================*/
#define DEBOUNCE_TIME_MS        5
#define MATRIX_SCAN_INTERVAL_MS 1
#define ROW_SETTLE_US           2

/*============================================================
 * LAYER CONFIGURATION
 *============================================================*/
#define MAX_LAYERS              16
#define DEFAULT_LAYER           0

/*============================================================
 * EVENT QUEUE
 *============================================================*/
#define KEY_EVENT_QUEUE_SIZE    32

/*============================================================
 * SPLIT COMMUNICATION
 *============================================================*/
#define SPLIT_UART_BAUDRATE     1000000
#define SPLIT_TX_BUFFER_SIZE    256
#define SPLIT_RX_BUFFER_SIZE    512
#define SPLIT_HEARTBEAT_MS      50
#define SPLIT_TIMEOUT_MS        100

/*============================================================
 * MACRO CONFIGURATION
 *============================================================*/
#define MAX_MACROS              32
#define MAX_MACRO_STEPS         64

/*============================================================
 * FLASH STORAGE
 *============================================================*/
#define KEYMAP_FLASH_ADDR       0x08020000
#define KEYMAP_MAGIC_NUMBER     0xDACF1234
#define KEYMAP_VERSION          1

#ifdef __cplusplus
}
#endif

#endif /* __KEYBOARD_CONFIG_H */
