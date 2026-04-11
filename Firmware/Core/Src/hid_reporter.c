#include "hid_reporter.h"
#include "key_processor.h"
#include <string.h>

/*
 * HID Reporter
 * ════════════
 * Builds and sends USB HID Keyboard reports when the key state changes.
 *
 * 6KRO Rollover:
 * When KeyProcessor signals rollover_overflow, we fill all 6 keycode
 * slots with ErrorRollOver (0x01) per USB HID spec section 8.
 * This tells the host a rollover has occurred rather than silently
 * dropping keys. Modifiers are still reported correctly.
 *
 * Boot Protocol vs Report Protocol:
 * This is handled in usbd_hid_custom.c's SendKeyboardReport, which
 * checks the current HID protocol and adjusts the report accordingly.
 *
 * Change detection:
 * We compare the full report struct (memcmp) before sending.
 * This avoids redundant USB transactions when the state hasn't
 * actually changed (e.g., modifier only events that resolve to
 * same modifiers due to dual registration).
 */

/* HID ErrorRollOver code per USB HID Usage Tables spec */
#define HID_ERROR_ROLLOVER      0x01U

static HID_KeyboardReport_t s_last_report;
static HID_KeyboardReport_t s_current_report;

void HIDReporter_Init(void)
{
    memset(&s_last_report,    0, sizeof(HID_KeyboardReport_t));
    memset(&s_current_report, 0, sizeof(HID_KeyboardReport_t));

    s_last_report.report_id    = HID_REPORT_ID_KEYBOARD;
    s_current_report.report_id = HID_REPORT_ID_KEYBOARD;
}

void HIDReporter_SendIfChanged(USBD_HandleTypeDef *pdev)
{
    if (!KeyProcessor_HasChanged()) return;

    const KeyProcessorState_t *state = KeyProcessor_GetState();

    s_current_report.report_id = HID_REPORT_ID_KEYBOARD;
    s_current_report.modifiers = state->modifiers;
    s_current_report.reserved  = 0x00U;

    if (state->rollover_overflow) {
        /*
         * 6KRO overflow: signal ErrorRollOver to the host.
         * All 6 keycode slots are set to 0x01 (ErrorRollOver).
         * Modifiers are still sent correctly — they are separate
         * from the keycode array and not affected by rollover.
         */
        memset(s_current_report.keycodes, HID_ERROR_ROLLOVER, 6);
    } else {
        /*
         * Normal case: copy up to 6 pressed keycodes.
         * Zero-pad any unused slots.
         */
        memset(s_current_report.keycodes, 0, 6);
        uint8_t count = (state->pressed_count > 6) ? 6 : state->pressed_count;
        memcpy(s_current_report.keycodes, state->pressed_keys, count);
    }

    if (memcmp(&s_current_report, &s_last_report,
               sizeof(HID_KeyboardReport_t)) != 0)
    {
        USBD_StatusTypeDef status =
            USBD_HID_SendKeyboardReport(pdev, &s_current_report);

        if (status == USBD_OK) {
            memcpy(&s_last_report, &s_current_report,
                   sizeof(HID_KeyboardReport_t));
        }
        /*
         * If status == USBD_BUSY, we do NOT update last_report.
         * The changed flag is still cleared below, but on the next
         * poll cycle, HasChanged() will return false. However, the
         * report will be resent on the next key event.
         *
         * For strict reliability, consider NOT clearing changed on BUSY.
         * This would cause a retry on the next loop iteration.
         * Trade-off: potential duplicate send vs. possible dropped report.
         * Current choice: clear always to avoid latency accumulation.
         */
    }

    KeyProcessor_ClearChanged();
}
