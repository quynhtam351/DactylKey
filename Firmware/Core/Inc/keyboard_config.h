#ifndef __KEYBOARD_CONFIG_H
#define __KEYBOARD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ── Identity ─────────────────────────────────────────────────── */

#define KEYBOARD_NAME           "Dactyl Ergonomic Keyboard"
#define KEYBOARD_VERSION        0x0100
#define FIRMWARE_VERSION        0x0100

/* ── Role constants ───────────────────────────────────────────── */

#define KEYBOARD_ROLE_MASTER    0U
#define KEYBOARD_ROLE_SLAVE     1U
#define KEYBOARD_ROLE_UNKNOWN   0xFFU

/*
 * Runtime role detection.
 *
 * g_keyboard_role is set once during startup by Role_Detect()
 * based on USB VBUS presence. After Role_Detect() returns,
 * this variable is read-only for the rest of the firmware.
 *
 * Do NOT use this in ISR context without understanding that it
 * is written only once before any ISR is enabled.
 *
 * Fallback: if VBUS detection is inconclusive, the role defaults
 * to KEYBOARD_ROLE_FALLBACK defined below.
 */
extern uint8_t g_keyboard_role;

/*
 * Compile-time fallback role used when VBUS detection fails
 * (e.g., USB OTG PHY not responding, clock issue).
 * Change this to KEYBOARD_ROLE_SLAVE to make the other half
 * the default fallback.
 */
#define KEYBOARD_ROLE_FALLBACK  KEYBOARD_ROLE_MASTER

/* ── Convenience macros (runtime checks) ─────────────────────── */

#define IS_MASTER()     (g_keyboard_role == KEYBOARD_ROLE_MASTER)
#define IS_SLAVE()      (g_keyboard_role == KEYBOARD_ROLE_SLAVE)

/* ── Matrix dimensions ────────────────────────────────────────── */

#define MATRIX_ROWS             4
#define MATRIX_COLS             4
#define MATRIX_SIZE             (MATRIX_ROWS * MATRIX_COLS)

#define KEYS_PER_HALF           (MATRIX_ROWS * MATRIX_COLS)
#define TOTAL_KEYS              (KEYS_PER_HALF * 2)

/* ── Row GPIO ─────────────────────────────────────────────────── */

#define ROW_GPIO_PORT           GPIOA

#define ROW0_PIN                GPIO_PIN_0
#define ROW1_PIN                GPIO_PIN_1
#define ROW2_PIN                GPIO_PIN_2
#define ROW3_PIN                GPIO_PIN_3

#define ROW_PINS_ARRAY          { ROW0_PIN, ROW1_PIN, ROW2_PIN, ROW3_PIN }
#define ROW_ALL_PINS            (ROW0_PIN | ROW1_PIN | ROW2_PIN | ROW3_PIN)

/* ── Column GPIO ──────────────────────────────────────────────── */

#define COL_GPIO_PORT           GPIOB

#define COL0_PIN                GPIO_PIN_0
#define COL1_PIN                GPIO_PIN_1
#define COL2_PIN                GPIO_PIN_6
#define COL3_PIN                GPIO_PIN_5

#define COL_PINS_ARRAY          { COL0_PIN, COL1_PIN, COL2_PIN, COL3_PIN }
#define COL_ALL_PINS            (COL0_PIN | COL1_PIN | COL2_PIN | COL3_PIN)

/* ── LED GPIO ─────────────────────────────────────────────────── */

#define LED_STATUS_GPIO_PORT    GPIOC
#define LED_STATUS_PIN          GPIO_PIN_13

#define LED_LAYER_GPIO_PORT     GPIOB
#define LED_LAYER1_PIN          GPIO_PIN_12
#define LED_LAYER2_PIN          GPIO_PIN_13

#define LED_ALL_PINS            (LED_LAYER1_PIN | LED_LAYER2_PIN)

/* ── Timing ───────────────────────────────────────────────────── */

#define DEBOUNCE_TIME_MS        5
#define MATRIX_SCAN_INTERVAL_MS 1
#define ROW_SETTLE_US           2

/* ── Layer system ─────────────────────────────────────────────── */

#define MAX_LAYERS              16
#define DEFAULT_LAYER           0

/* ── Event queue ──────────────────────────────────────────────── */

#define KEY_EVENT_QUEUE_SIZE    32

/* ── Split communication ──────────────────────────────────────── */

#define SPLIT_UART_BAUDRATE     1000000
#define SPLIT_TX_BUFFER_SIZE    256
#define SPLIT_RX_BUFFER_SIZE    512
#define SPLIT_HEARTBEAT_MS      50
#define SPLIT_TIMEOUT_MS        100

/* ── Macro system (reserved) ──────────────────────────────────── */

#define MAX_MACROS              32
#define MAX_MACRO_STEPS         64

/* ── Flash keymap storage (reserved) ─────────────────────────── */

#define KEYMAP_FLASH_ADDR       0x08020000
#define KEYMAP_MAGIC_NUMBER     0xDACF1234
#define KEYMAP_VERSION          1

/* ── VBUS detection ───────────────────────────────────────────── */

/*
 * Milliseconds to wait after enabling USB OTG clock before
 * reading the VBUS comparator. The OTG PHY needs time to power
 * up its internal comparators.
 */
#define VBUS_DETECT_SETTLE_MS   10U

/*
 * Number of consecutive BSVLD readings required to confirm VBUS.
 * Sampled every 1ms. Total detection time = SETTLE + SAMPLES ms.
 * This prevents false detection from transient spikes.
 */
#define VBUS_DETECT_SAMPLES     3U

#ifdef __cplusplus
}
#endif

#endif
