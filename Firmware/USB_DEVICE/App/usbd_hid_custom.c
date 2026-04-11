#include "usbd_hid_custom.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include "led_manager.h"
#include <string.h>

/*
 * Phase 3 additions:
 *   - SET_REPORT handler for LED Output report (Caps Lock etc.)
 *   - EP0_RxReady callback to process LED data
 *   - GET_REPORT handler for Output report
 *   - suspended flag for USB suspend/resume/remote wakeup
 */

static const uint8_t HID_KB_ReportDesc[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
    0x85, HID_REPORT_ID_KEYBOARD,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05,
    0x95, 0x05, 0x75, 0x01, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x03,
    0x05, 0x07, 0x19, 0x00, 0x29, 0xE7,
    0x15, 0x00, 0x26, 0xE7, 0x00,
    0x95, 0x06, 0x75, 0x08, 0x81, 0x00,
    0xC0,
};

static const uint8_t HID_Raw_ReportDesc[] = {
    0x06, 0x60, 0xFF, 0x09, 0x61, 0xA1, 0x01,
    0x09, 0x62, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, HID_RAW_EP_SIZE, 0x81, 0x02,
    0x09, 0x63, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, HID_RAW_EP_SIZE, 0x91, 0x02,
    0xC0,
};

static const uint8_t USBD_HID_CfgDesc[HID_CUSTOM_CONFIG_DESC_SIZE] = {
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(HID_CUSTOM_CONFIG_DESC_SIZE), HIBYTE(HID_CUSTOM_CONFIG_DESC_SIZE),
    0x02, 0x01, 0x00, 0xA0, 0x32,

    0x09, USB_DESC_TYPE_INTERFACE,
    HID_KB_INTERFACE_NUM, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,

    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    LOBYTE(sizeof(HID_KB_ReportDesc)), HIBYTE(sizeof(HID_KB_ReportDesc)),

    0x07, USB_DESC_TYPE_ENDPOINT, HID_KB_EP_IN_ADDR, 0x03,
    LOBYTE(HID_KB_EP_IN_SIZE), HIBYTE(HID_KB_EP_IN_SIZE), HID_KB_POLL_INTERVAL,

    0x09, USB_DESC_TYPE_INTERFACE,
    HID_RAW_INTERFACE_NUM, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,

    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    LOBYTE(sizeof(HID_Raw_ReportDesc)), HIBYTE(sizeof(HID_Raw_ReportDesc)),

    0x07, USB_DESC_TYPE_ENDPOINT, HID_RAW_EP_IN_ADDR, 0x03,
    LOBYTE(HID_RAW_EP_SIZE), HIBYTE(HID_RAW_EP_SIZE), HID_RAW_POLL_INTERVAL,

    0x07, USB_DESC_TYPE_ENDPOINT, HID_RAW_EP_OUT_ADDR, 0x03,
    LOBYTE(HID_RAW_EP_SIZE), HIBYTE(HID_RAW_EP_SIZE), HID_RAW_POLL_INTERVAL,
};

static USBD_HID_Custom_HandleTypeDef s_hid_handle;
static uint8_t s_raw_rx_buf[HID_RAW_EP_SIZE];
static uint8_t s_raw_data_buf[HID_RAW_EP_SIZE];
static bool    s_raw_data_available = false;

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_HID_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_HID_GetCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);

USBD_ClassTypeDef USBD_HID_Custom = {
    USBD_HID_Init,
    USBD_HID_DeInit,
    USBD_HID_Setup,
    NULL,                   /* EP0_TxSent */
    USBD_HID_EP0_RxReady,  /* EP0_RxReady */
    USBD_HID_DataIn,
    USBD_HID_DataOut,
    NULL, NULL, NULL,
    USBD_HID_GetCfgDesc,
    USBD_HID_GetCfgDesc,
    USBD_HID_GetCfgDesc,
    USBD_HID_GetDeviceQualifierDesc,
};

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    memset(&s_hid_handle, 0, sizeof(s_hid_handle));
    s_hid_handle.protocol  = HID_PROTOCOL_REPORT;
    s_hid_handle.idle_rate = 0U;
    s_hid_handle.suspended = false;
    pdev->pClassData = &s_hid_handle;

    USBD_LL_OpenEP(pdev, HID_KB_EP_IN_ADDR, USBD_EP_TYPE_INTR, HID_KB_EP_IN_SIZE);
    pdev->ep_in[HID_KB_EP_IN_ADDR & 0xFU].is_used = 1U;

    USBD_LL_OpenEP(pdev, HID_RAW_EP_IN_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_in[HID_RAW_EP_IN_ADDR & 0xFU].is_used = 1U;

    USBD_LL_OpenEP(pdev, HID_RAW_EP_OUT_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_out[HID_RAW_EP_OUT_ADDR & 0xFU].is_used = 1U;

    USBD_LL_PrepareReceive(pdev, HID_RAW_EP_OUT_ADDR, s_raw_rx_buf, HID_RAW_EP_SIZE);

    s_hid_handle.kb_state  = HID_IDLE;
    s_hid_handle.raw_state = HID_IDLE;
    return USBD_OK;
}

static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    USBD_LL_CloseEP(pdev, HID_KB_EP_IN_ADDR);
    USBD_LL_CloseEP(pdev, HID_RAW_EP_IN_ADDR);
    USBD_LL_CloseEP(pdev, HID_RAW_EP_OUT_ADDR);
    pdev->ep_in[HID_KB_EP_IN_ADDR  & 0xFU].is_used = 0U;
    pdev->ep_in[HID_RAW_EP_IN_ADDR & 0xFU].is_used = 0U;
    pdev->ep_out[HID_RAW_EP_OUT_ADDR & 0xFU].is_used = 0U;
    pdev->pClassData = NULL;
    return USBD_OK;
}

static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    uint16_t len = 0U;
    uint8_t *pbuf = NULL;
    uint16_t status_info = 0U;
    USBD_StatusTypeDef ret = USBD_OK;

    if (hhid == NULL) return USBD_FAIL;

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
    case USB_REQ_TYPE_CLASS:
        switch (req->bRequest) {
        case HID_REQ_SET_PROTOCOL:
            hhid->protocol = (uint8_t)(req->wValue & 0x01U);
            break;
        case HID_REQ_GET_PROTOCOL:
            USBD_CtlSendData(pdev, &hhid->protocol, 1U);
            break;
        case HID_REQ_SET_IDLE:
            hhid->idle_rate = (uint8_t)(req->wValue >> 8);
            break;
        case HID_REQ_GET_IDLE:
            USBD_CtlSendData(pdev, &hhid->idle_rate, 1U);
            break;
        case HID_REQ_SET_REPORT:
            if ((req->wIndex & 0xFFU) == HID_KB_INTERFACE_NUM) {
                hhid->led_report_len = MIN(req->wLength, sizeof(hhid->led_report_buf));
                USBD_CtlPrepareRx(pdev, hhid->led_report_buf, hhid->led_report_len);
            }
            break;
        case HID_REQ_GET_REPORT:
            if ((req->wIndex & 0xFFU) == HID_KB_INTERFACE_NUM) {
                uint8_t report_type = (req->wValue >> 8) & 0xFFU;
                if (report_type == HID_REPORT_TYPE_OUTPUT) {
                    uint8_t led_val = LedManager_GetHIDLeds();
                    USBD_CtlSendData(pdev, &led_val, 1U);
                }
            }
            break;
        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }
        break;

    case USB_REQ_TYPE_STANDARD:
        switch (req->bRequest) {
        case USB_REQ_GET_STATUS:
            if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
            } else {
                USBD_CtlError(pdev, req); ret = USBD_FAIL;
            }
            break;
        case USB_REQ_GET_DESCRIPTOR:
            if ((req->wValue >> 8) == HID_REPORT_DESC) {
                if ((req->wIndex & 0xFFU) == HID_KB_INTERFACE_NUM) {
                    pbuf = (uint8_t *)HID_KB_ReportDesc; len = sizeof(HID_KB_ReportDesc);
                } else {
                    pbuf = (uint8_t *)HID_Raw_ReportDesc; len = sizeof(HID_Raw_ReportDesc);
                }
                len = MIN(len, req->wLength);
                USBD_CtlSendData(pdev, pbuf, len);
            } else if ((req->wValue >> 8) == HID_DESCRIPTOR_TYPE) {
                if ((req->wIndex & 0xFFU) == HID_KB_INTERFACE_NUM) {
                    pbuf = (uint8_t *)USBD_HID_CfgDesc + 18U;
                } else {
                    pbuf = (uint8_t *)USBD_HID_CfgDesc + 43U;
                }
                len = MIN(9U, req->wLength);
                USBD_CtlSendData(pdev, pbuf, len);
            } else {
                USBD_CtlError(pdev, req); ret = USBD_FAIL;
            }
            break;
        case USB_REQ_GET_INTERFACE:
            if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                uint8_t alt = 0U;
                USBD_CtlSendData(pdev, &alt, 1U);
            } else {
                USBD_CtlError(pdev, req); ret = USBD_FAIL;
            }
            break;
        case USB_REQ_SET_INTERFACE: break;
        case USB_REQ_CLEAR_FEATURE: break;
        default:
            USBD_CtlError(pdev, req); ret = USBD_FAIL; break;
        }
        break;
    default:
        USBD_CtlError(pdev, req); ret = USBD_FAIL; break;
    }
    return (uint8_t)ret;
}

static uint8_t USBD_HID_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL) return USBD_OK;

    if (hhid->led_report_len > 0U) {
        uint8_t led_bits;
        if (hhid->led_report_len >= 2U &&
            hhid->led_report_buf[0] == HID_REPORT_ID_KEYBOARD) {
            led_bits = hhid->led_report_buf[1];
        } else {
            led_bits = hhid->led_report_buf[0];
        }
        LedManager_SetHIDLeds(led_bits & 0x1FU);
        hhid->led_report_len = 0U;
    }
    return USBD_OK;
}

static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL) return USBD_FAIL;
    if (epnum == (HID_KB_EP_IN_ADDR & 0x0FU)) hhid->kb_state = HID_IDLE;
    else if (epnum == (HID_RAW_EP_IN_ADDR & 0x0FU)) hhid->raw_state = HID_IDLE;
    return USBD_OK;
}

static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if (epnum == (HID_RAW_EP_OUT_ADDR & 0x0FU)) {
        memcpy(s_raw_data_buf, s_raw_rx_buf, HID_RAW_EP_SIZE);
        s_raw_data_available = true;
        USBD_LL_PrepareReceive(pdev, HID_RAW_EP_OUT_ADDR, s_raw_rx_buf, HID_RAW_EP_SIZE);
    }
    return USBD_OK;
}

static uint8_t *USBD_HID_GetCfgDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
    return (uint8_t *)USBD_HID_CfgDesc;
}

static uint8_t USBD_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] = {
    USB_LEN_DEV_QUALIFIER_DESC, USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00,
};

static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);
    return USBD_HID_DeviceQualifierDesc;
}

/* ── Public API ───────────────────────────────────────────────── */

USBD_StatusTypeDef USBD_HID_SendKeyboardReport(USBD_HandleTypeDef *pdev, HID_KeyboardReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED || hhid->kb_state == HID_BUSY)
        return (hhid && hhid->kb_state == HID_BUSY) ? USBD_BUSY : USBD_FAIL;

    hhid->kb_state = HID_BUSY;
    if (hhid->protocol == HID_PROTOCOL_BOOT) {
        USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR, &report->modifiers, HID_KB_EP_BOOT_SIZE);
    } else {
        USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR, (uint8_t *)report, sizeof(HID_KeyboardReport_t));
    }
    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendNKROReport(USBD_HandleTypeDef *pdev, HID_NKROReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED || hhid->kb_state == HID_BUSY)
        return USBD_FAIL;
    hhid->kb_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR, (uint8_t *)report, sizeof(HID_NKROReport_t));
    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendRawReport(USBD_HandleTypeDef *pdev, uint8_t *data)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED || hhid->raw_state == HID_BUSY)
        return USBD_FAIL;
    hhid->raw_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_RAW_EP_IN_ADDR, data, HID_RAW_EP_SIZE);
    return USBD_OK;
}

bool USBD_HID_RawDataAvailable(void) { return s_raw_data_available; }

bool USBD_HID_GetRawData(uint8_t *buf)
{
    if (!s_raw_data_available) return false;
    memcpy(buf, s_raw_data_buf, HID_RAW_EP_SIZE);
    s_raw_data_available = false;
    return true;
}

bool USBD_HID_KeyboardReady(USBD_HandleTypeDef *pdev)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED) return false;
    return (hhid->kb_state == HID_IDLE);
}

bool USBD_HID_IsSuspended(USBD_HandleTypeDef *pdev)
{
    USBD_HID_Custom_HandleTypeDef *hhid = (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL) return false;
    return hhid->suspended;
}
