#include "hid_reporter.h"
#include "key_processor.h"
#include <string.h>

static HID_KeyboardReport_t s_last_report;
static HID_KeyboardReport_t s_current_report;

void HIDReporter_Init(void)
{
    memset(&s_last_report, 0, sizeof(HID_KeyboardReport_t));
    memset(&s_current_report, 0, sizeof(HID_KeyboardReport_t));

    s_last_report.report_id    = HID_REPORT_ID_KEYBOARD;
    s_current_report.report_id = HID_REPORT_ID_KEYBOARD;
}

void HIDReporter_SendIfChanged(USBD_HandleTypeDef *pdev)
{
    if (!KeyProcessor_HasChanged()) return;

    const KeyProcessorState_t *state = KeyProcessor_GetState();

    /* Build new report */
    s_current_report.report_id = HID_REPORT_ID_KEYBOARD;
    s_current_report.modifiers = state->modifiers;
    s_current_report.reserved  = 0x00;

    memset(s_current_report.keycodes, 0, 6);
    uint8_t count = (state->pressed_count > 6) ? 6 : state->pressed_count;
    memcpy(s_current_report.keycodes, state->pressed_keys, count);

    /* Only send if actually different from last sent report */
    if (memcmp(&s_current_report, &s_last_report,
               sizeof(HID_KeyboardReport_t)) != 0)
    {
        USBD_StatusTypeDef status =
            USBD_HID_SendKeyboardReport(pdev, &s_current_report);

        if (status == USBD_OK) {
            memcpy(&s_last_report, &s_current_report,
                   sizeof(HID_KeyboardReport_t));
        }
    }

    KeyProcessor_ClearChanged();
}