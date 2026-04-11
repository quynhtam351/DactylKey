#include "led_manager.h"
#include "stm32f4xx_hal.h"
#include "keyboard_config.h"
#include <string.h>

/*
 * LED Manager Implementation
 * ══════════════════════════
 *
 * All LEDs are active LOW:
 *   GPIO_PIN_RESET = LED ON
 *   GPIO_PIN_SET   = LED OFF
 *
 * PC13 (STATUS): both halves, blink driven by pattern + tick counter.
 * PB12 (LAYER):  master only, combinational (no blink).
 * PB13 (CAPS):   master only, combinational (no blink).
 *
 * Pattern periods:
 *   2Hz → toggle every 250ms  (500ms full period)
 *   4Hz → toggle every 125ms  (250ms full period)
 */

typedef struct {
    LedStatusPattern_t  status_pattern;
    uint32_t            blink_tick;
    uint8_t             blink_phase;

    uint8_t             hid_leds;       /* raw bitmask from host    */
    uint32_t            layer_state;    /* from KeyProcessor        */
} LedManagerState_t;

static LedManagerState_t s_led;

/* ── Helper ───────────────────────────────────────────────────── */

static inline void _SetLED(GPIO_TypeDef *port, uint16_t pin, bool on)
{
    /* Active LOW: ON = RESET, OFF = SET */
    HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* ═══════════════════════════════════════════════════════════════ */

void LedManager_Init(void)
{
    memset(&s_led, 0, sizeof(LedManagerState_t));

    s_led.status_pattern = LED_STATUS_OFF;
    s_led.blink_tick     = HAL_GetTick();
    s_led.blink_phase    = 0U;

    /* All LEDs off at startup */
    _SetLED(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, false);
    _SetLED(LED_LAYER_GPIO_PORT,  LED_LAYER1_PIN, false);
    _SetLED(LED_LAYER_GPIO_PORT,  LED_LAYER2_PIN, false);
}

void LedManager_SetStatusPattern(LedStatusPattern_t pattern)
{
    if (s_led.status_pattern == pattern) return;

    s_led.status_pattern = pattern;
    /* Reset blink phase so new pattern starts cleanly */
    s_led.blink_tick  = HAL_GetTick();
    s_led.blink_phase = 0U;
}

void LedManager_SetHIDLeds(uint8_t led_bits)
{
    s_led.hid_leds = led_bits & 0x1FU; /* mask to 5 valid bits */
}

uint8_t LedManager_GetHIDLeds(void)
{
    return s_led.hid_leds;
}

void LedManager_SetLayerState(uint32_t layer_state)
{
    s_led.layer_state = layer_state;
}

/* ═══════════════════════════════════════════════════════════════ */
/*                        LED UPDATE                               */
/* ═══════════════════════════════════════════════════════════════ */

void LedManager_Update(void)
{
    uint32_t now = HAL_GetTick();

    /* ──────────────────────────────────────────────────────────
     *  PC13 (STATUS) — both halves
     * ────────────────────────────────────────────────────────── */

    switch (s_led.status_pattern) {

    case LED_STATUS_OFF:
        /* Connected, no issues → solid OFF */
        _SetLED(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, false);
        break;

    case LED_STATUS_BLINK_2HZ:
        /* Disconnected → toggle every 250ms */
        if ((now - s_led.blink_tick) >= 250U) {
            s_led.blink_tick   = now;
            s_led.blink_phase ^= 1U;
        }
        _SetLED(LED_STATUS_GPIO_PORT, LED_STATUS_PIN,
                (s_led.blink_phase & 1U) != 0U);
        break;

    case LED_STATUS_BLINK_4HZ:
        /* Connected with issue → toggle every 125ms */
        if ((now - s_led.blink_tick) >= 125U) {
            s_led.blink_tick   = now;
            s_led.blink_phase ^= 1U;
        }
        _SetLED(LED_STATUS_GPIO_PORT, LED_STATUS_PIN,
                (s_led.blink_phase & 1U) != 0U);
        break;

    default:
        _SetLED(LED_STATUS_GPIO_PORT, LED_STATUS_PIN, false);
        break;
    }

    /* ──────────────────────────────────────────────────────────
     *  PB12 (LAYER) — Master only
     *  ON when any layer above Layer 0 is active.
     *  layer_state == 0x00000001 means only Layer 0 → OFF.
     *  Any other bit set → ON.
     * ────────────────────────────────────────────────────────── */

    if (IS_MASTER()) {
        bool non_default = (s_led.layer_state & ~(0x01UL)) != 0U;
        _SetLED(LED_LAYER_GPIO_PORT, LED_LAYER1_PIN, non_default);

        /* ──────────────────────────────────────────────────────
         *  PB13 (CAPS LOCK) — Master only
         *  ON when host reports Caps Lock active.
         * ────────────────────────────────────────────────────── */

        bool caps = (s_led.hid_leds & HID_LED_CAPS_LOCK) != 0U;
        _SetLED(LED_LAYER_GPIO_PORT, LED_LAYER2_PIN, caps);
    }
    /* Slave: PB12 and PB13 remain OFF (set in LedManager_Init,
     * never written again since IS_MASTER() is false) */
}
