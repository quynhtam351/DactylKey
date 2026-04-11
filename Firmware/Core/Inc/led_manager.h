#ifndef __LED_MANAGER_H
#define __LED_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "keyboard_config.h"

/*
 * LED Manager
 * ═══════════
 *
 * Physical LED assignment:
 *
 *   PC13 (STATUS) — Active LOW — Both halves
 *     Managed by SplitComm (connection status) + main (USB status).
 *     OFF   : Connected, no issues
 *     2Hz   : Disconnected (split cable unplugged / other half reset)
 *     4Hz   : Connected but issue (Master: USB not enumerated)
 *
 *   PB12 (LAYER) — Active LOW — Master only
 *     OFF : Default layer only (layer_state == 0x01)
 *     ON  : Any non-default layer is active
 *
 *   PB13 (CAPS) — Active LOW — Master only
 *     OFF : Caps Lock not active
 *     ON  : Caps Lock active (from USB HID Output report)
 *
 * Slave side: only PC13 is used (connection status blink).
 * PB12 and PB13 remain OFF on Slave.
 */

/* ── HID LED bits (from USB Output report) ────────────────────── */

#define HID_LED_NUM_LOCK        (1U << 0)
#define HID_LED_CAPS_LOCK       (1U << 1)
#define HID_LED_SCROLL_LOCK     (1U << 2)
#define HID_LED_COMPOSE         (1U << 3)
#define HID_LED_KANA            (1U << 4)

/* ── Status LED patterns ──────────────────────────────────────── */

typedef enum {
    LED_STATUS_OFF          = 0,  /* Solid OFF: connected, all OK      */
    LED_STATUS_BLINK_2HZ    = 1,  /* 2Hz (250ms): disconnected         */
    LED_STATUS_BLINK_4HZ    = 2,  /* 4Hz (125ms): connected with issue */
} LedStatusPattern_t;

/* ── Public API ───────────────────────────────────────────────── */

void    LedManager_Init(void);

/* Called by SplitComm / main to set status LED behavior */
void    LedManager_SetStatusPattern(LedStatusPattern_t pattern);

/* Called from usbd_hid_custom.c EP0_RxReady (Master only) */
void    LedManager_SetHIDLeds(uint8_t led_bits);
uint8_t LedManager_GetHIDLeds(void);

/* Called from main loop after KeyProcessor (Master only) */
void    LedManager_SetLayerState(uint32_t layer_state);

/* Call once per main loop iteration */
void    LedManager_Update(void);

#ifdef __cplusplus
}
#endif

#endif
