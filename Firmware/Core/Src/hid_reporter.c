#include "hid_reporter.h"
#include "key_processor.h"
#include "media_keys.h"
#include "usbd_hid_custom.h"
#include <string.h>

#define HID_ERROR_ROLLOVER  0x01U

static HID_KeyboardReport_t s_last_kb;
static HID_KeyboardReport_t s_curr_kb;

typedef struct {
    uint8_t report_id;
    uint8_t usage;
} __attribute__((packed)) HID_SystemReport_t;

static HID_SystemReport_t s_last_sys;
static HID_SystemReport_t s_curr_sys;

static HID_ConsumerReport_t s_last_cons;
static HID_ConsumerReport_t s_curr_cons;

void HIDReporter_Init(void)
{
    memset(&s_last_kb, 0, sizeof(s_last_kb));
    memset(&s_curr_kb, 0, sizeof(s_curr_kb));
    s_last_kb.report_id = HID_REPORT_ID_KEYBOARD;
    s_curr_kb.report_id = HID_REPORT_ID_KEYBOARD;

    memset(&s_last_sys, 0, sizeof(s_last_sys));
    memset(&s_curr_sys, 0, sizeof(s_curr_sys));
    s_last_sys.report_id = HID_REPORT_ID_SYSTEM;
    s_curr_sys.report_id = HID_REPORT_ID_SYSTEM;

    memset(&s_last_cons, 0, sizeof(s_last_cons));
    memset(&s_curr_cons, 0, sizeof(s_curr_cons));
    s_last_cons.report_id = HID_REPORT_ID_CONSUMER;
    s_curr_cons.report_id = HID_REPORT_ID_CONSUMER;
}

static void _SendKeyboardReport(USBD_HandleTypeDef *pdev)
{
    if (!KeyProcessor_HasChanged()) return;

    if (!USBD_HID_KeyboardReady(pdev)) return;

    const KeyProcessorState_t *state = KeyProcessor_GetState();

    s_curr_kb.report_id = HID_REPORT_ID_KEYBOARD;
    s_curr_kb.modifiers = state->modifiers;
    s_curr_kb.reserved  = 0x00U;

    if (state->rollover_overflow) {
        memset(s_curr_kb.keycodes, HID_ERROR_ROLLOVER, 6U);
    } else {
        memset(s_curr_kb.keycodes, 0U, 6U);
        uint8_t count = (state->pressed_count > 6U) ? 6U : state->pressed_count;
        memcpy(s_curr_kb.keycodes, state->pressed_keys, count);
    }

    if (memcmp(&s_curr_kb, &s_last_kb, sizeof(HID_KeyboardReport_t)) == 0) {
        KeyProcessor_ClearChanged();
        return;
    }

    USBD_StatusTypeDef st = USBD_HID_SendKeyboardReport(pdev, &s_curr_kb);
    if (st == USBD_OK) {
        memcpy(&s_last_kb, &s_curr_kb, sizeof(HID_KeyboardReport_t));
        KeyProcessor_ClearChanged();
    }
}

static void _SendSystemReport(USBD_HandleTypeDef *pdev)
{
    if (!KeyProcessor_SystemKeyChanged()) return;

    if (!USBD_HID_KeyboardReady(pdev)) return;

    s_curr_sys.report_id = HID_REPORT_ID_SYSTEM;
    s_curr_sys.usage     = KeyProcessor_GetSystemKey();

    if (memcmp(&s_curr_sys, &s_last_sys, sizeof(HID_SystemReport_t)) == 0) {
        KeyProcessor_ClearSystemKeyChanged();
        return;
    }

    USBD_StatusTypeDef st = USBD_HID_SendSystemReport(pdev, s_curr_sys.usage);
    if (st == USBD_OK) {
        memcpy(&s_last_sys, &s_curr_sys, sizeof(HID_SystemReport_t));
        KeyProcessor_ClearSystemKeyChanged();
    }
}

void HIDReporter_SendMediaIfChanged(USBD_HandleTypeDef *pdev)
{
    if (!MediaKeys_HasChanged()) return;

    if (!USBD_HID_KeyboardReady(pdev)) return;

    const MediaKeyState_t *ms = MediaKeys_GetState();

    s_curr_cons.report_id = HID_REPORT_ID_CONSUMER;
    s_curr_cons.usage_id  = (ms->count > 0U) ? ms->usages[0] : 0U;

    if (memcmp(&s_curr_cons, &s_last_cons, sizeof(HID_ConsumerReport_t)) == 0) {
        MediaKeys_ClearChanged();
        return;
    }

    USBD_StatusTypeDef st = USBD_HID_SendConsumerReport(pdev, s_curr_cons.usage_id);
    if (st == USBD_OK) {
        memcpy(&s_last_cons, &s_curr_cons, sizeof(HID_ConsumerReport_t));
        MediaKeys_ClearChanged();
    }
}

void HIDReporter_SendIfChanged(USBD_HandleTypeDef *pdev)
{
    _SendKeyboardReport(pdev);
    _SendSystemReport(pdev);
    HIDReporter_SendMediaIfChanged(pdev);
}
