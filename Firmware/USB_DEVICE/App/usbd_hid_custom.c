#include "usbd_hid_custom.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include "led_manager.h"
#include <string.h>

static const uint8_t HID_KB_ReportDesc[] = {
    /* ── Keyboard Application Collection ── */
    0x05, 0x01,           /* Usage Page (Generic Desktop) */
    0x09, 0x06,           /* Usage (Keyboard) */
    0xA1, 0x01,           /* Collection (Application) */
    0x85, HID_REPORT_ID_KEYBOARD,  /* Report ID (1) */

    /* Modifier keys (8 bits) */
    0x05, 0x07,           /* Usage Page (Keyboard/Keypad) */
    0x19, 0xE0,           /* Usage Minimum (Left Control) */
    0x29, 0xE7,           /* Usage Maximum (Right GUI) */
    0x15, 0x00,           /* Logical Minimum (0) */
    0x25, 0x01,           /* Logical Maximum (1) */
    0x75, 0x01,           /* Report Size (1) */
    0x95, 0x08,           /* Report Count (8) */
    0x81, 0x02,           /* Input (Data, Variable, Absolute) */

    /* Reserved byte */
    0x95, 0x01,           /* Report Count (1) */
    0x75, 0x08,           /* Report Size (8) */
    0x81, 0x03,           /* Input (Constant, Variable, Absolute) */

    /* LED output report */
    0x05, 0x08,           /* Usage Page (LEDs) */
    0x19, 0x01,           /* Usage Minimum (Num Lock) */
    0x29, 0x05,           /* Usage Maximum (Kana) */
    0x95, 0x05,           /* Report Count (5) */
    0x75, 0x01,           /* Report Size (1) */
    0x91, 0x02,           /* Output (Data, Variable, Absolute) */
    0x95, 0x01,           /* Report Count (1) */
    0x75, 0x03,           /* Report Size (3) */
    0x91, 0x03,           /* Output (Constant, Variable, Absolute) */

    /* Keycodes (6KRO) */
    0x05, 0x07,           /* Usage Page (Keyboard/Keypad) */
    0x19, 0x00,           /* Usage Minimum (0) */
    0x29, 0xE7,           /* Usage Maximum (0xE7) */
    0x15, 0x00,           /* Logical Minimum (0) */
    0x26, 0xE7, 0x00,     /* Logical Maximum (0xE7) */
    0x95, 0x06,           /* Report Count (6) */
    0x75, 0x08,           /* Report Size (8) */
    0x81, 0x00,           /* Input (Data, Array, Absolute) */

    /* ── System Control (nested collection) ── */
    0x05, 0x01,           /* Usage Page (Generic Desktop) */
    0x09, 0x80,           /* Usage (System Control) */
    0xA1, 0x01,           /* Collection (Application) */
    0x85, HID_REPORT_ID_SYSTEM,  /* Report ID (3) */
    0x19, 0x81,           /* Usage Minimum (System Power Down) */
    0x29, 0x83,           /* Usage Maximum (System Wake Up) */
    0x15, 0x00,           /* Logical Minimum (0) */
    0x25, 0x01,           /* Logical Maximum (1) */
    0x75, 0x01,           /* Report Size (1) */
    0x95, 0x03,           /* Report Count (3) */
    0x81, 0x02,           /* Input (Data, Variable, Absolute) */
    0x95, 0x05,           /* Report Count (5) */
    0x75, 0x01,           /* Report Size (1) */
    0x81, 0x03,           /* Input (Constant, Variable, Absolute) */
    0xC0,                 /* End Collection (System Control) */

    0xC0,                 /* End Collection (Keyboard Application) */

    /* ── Consumer Control Collection ── */
    0x05, 0x0C,           /* Usage Page (Consumer) */
    0x09, 0x01,           /* Usage (Consumer Control) */
    0xA1, 0x01,           /* Collection (Application) */
    0x85, HID_REPORT_ID_CONSUMER,  /* Report ID (4) */

    0x15, 0x00,           /* Logical Minimum (0) */
    0x26, 0xFF, 0x02,     /* Logical Maximum (0x02FF = 767) */
    0x19, 0x00,           /* Usage Minimum (0) */
    0x2A, 0xFF, 0x02,     /* Usage Maximum (0x02FF) */
    0x75, 0x10,           /* Report Size (16 bits) */
    0x95, 0x01,           /* Report Count (1) */
    0x81, 0x00,           /* Input (Data, Array, Absolute) */

    0xC0,                 /* End Collection (Consumer Control) */
};

static const uint8_t HID_Raw_ReportDesc[] = {
    0x06, 0x60, 0xFF,
    0x09, 0x61,
    0xA1, 0x01,
    0x09, 0x62,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, HID_RAW_EP_SIZE,
    0x81, 0x02,
    0x09, 0x63,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, HID_RAW_EP_SIZE,
    0x91, 0x02,
    0xC0,
};

static const uint8_t USBD_HID_CfgDesc[HID_CUSTOM_CONFIG_DESC_SIZE] = {
    /* Configuration descriptor */
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(HID_CUSTOM_CONFIG_DESC_SIZE),
    HIBYTE(HID_CUSTOM_CONFIG_DESC_SIZE),
    0x02, 0x01, 0x00, 0xA0, 0x32,

    /* Interface 0: Keyboard HID */
    0x09, USB_DESC_TYPE_INTERFACE,
    HID_KB_INTERFACE_NUM, 0x00, 0x01, 0x03, 0x01, 0x01, 0x00,

    /* HID descriptor for Interface 0 */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    LOBYTE(sizeof(HID_KB_ReportDesc)),
    HIBYTE(sizeof(HID_KB_ReportDesc)),

    /* Endpoint IN for keyboard */
    0x07, USB_DESC_TYPE_ENDPOINT, HID_KB_EP_IN_ADDR, 0x03,
    LOBYTE(HID_KB_EP_IN_SIZE), HIBYTE(HID_KB_EP_IN_SIZE),
    HID_KB_POLL_INTERVAL,

    /* Interface 1: Raw HID */
    0x09, USB_DESC_TYPE_INTERFACE,
    HID_RAW_INTERFACE_NUM, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,

    /* HID descriptor for Interface 1 */
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
    LOBYTE(sizeof(HID_Raw_ReportDesc)),
    HIBYTE(sizeof(HID_Raw_ReportDesc)),

    /* Endpoint IN for raw */
    0x07, USB_DESC_TYPE_ENDPOINT, HID_RAW_EP_IN_ADDR, 0x03,
    LOBYTE(HID_RAW_EP_SIZE), HIBYTE(HID_RAW_EP_SIZE),
    HID_RAW_POLL_INTERVAL,

    /* Endpoint OUT for raw */
    0x07, USB_DESC_TYPE_ENDPOINT, HID_RAW_EP_OUT_ADDR, 0x03,
    LOBYTE(HID_RAW_EP_SIZE), HIBYTE(HID_RAW_EP_SIZE),
    HID_RAW_POLL_INTERVAL,
};

static USBD_HID_Custom_HandleTypeDef s_hid_handle;

static uint8_t       s_raw_rx_buf[HID_RAW_EP_SIZE];
static uint8_t       s_raw_data_buf[HID_RAW_EP_SIZE];
static volatile bool s_raw_data_available = false;

static uint8_t  USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t  USBD_HID_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t  USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_HID_GetCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);

USBD_ClassTypeDef USBD_HID_Custom = {
    USBD_HID_Init,
    USBD_HID_DeInit,
    USBD_HID_Setup,
    NULL,
    USBD_HID_EP0_RxReady,
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
    pdev->ep_in[HID_KB_EP_IN_ADDR & 0x0FU].is_used = 1U;

    USBD_LL_OpenEP(pdev, HID_RAW_EP_IN_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_in[HID_RAW_EP_IN_ADDR & 0x0FU].is_used = 1U;

    USBD_LL_OpenEP(pdev, HID_RAW_EP_OUT_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_out[HID_RAW_EP_OUT_ADDR & 0x0FU].is_used = 1U;

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
    pdev->ep_in[HID_KB_EP_IN_ADDR   & 0x0FU].is_used = 0U;
    pdev->ep_in[HID_RAW_EP_IN_ADDR  & 0x0FU].is_used = 0U;
    pdev->ep_out[HID_RAW_EP_OUT_ADDR & 0x0FU].is_used = 0U;
    pdev->pClassData = NULL;
    return USBD_OK;
}

static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    uint16_t len         = 0U;
    uint8_t *pbuf        = NULL;
    uint16_t status_info = 0U;
    USBD_StatusTypeDef ret = USBD_OK;

    if (hhid == NULL) return (uint8_t)USBD_FAIL;

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
                hhid->led_report_len =
                    (uint8_t)MIN(req->wLength, sizeof(hhid->led_report_buf));
                USBD_CtlPrepareRx(pdev, hhid->led_report_buf, hhid->led_report_len);
            }
            break;
        case HID_REQ_GET_REPORT:
            if ((req->wIndex & 0xFFU) == HID_KB_INTERFACE_NUM) {
                uint8_t report_type = (uint8_t)((req->wValue >> 8) & 0xFFU);
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
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;
        case USB_REQ_GET_DESCRIPTOR:
            if ((req->wValue >> 8) == HID_REPORT_DESC) {
                if ((req->wIndex & 0xFFU) == HID_KB_INTERFACE_NUM) {
                    pbuf = (uint8_t *)HID_KB_ReportDesc;
                    len  = (uint16_t)sizeof(HID_KB_ReportDesc);
                } else {
                    pbuf = (uint8_t *)HID_Raw_ReportDesc;
                    len  = (uint16_t)sizeof(HID_Raw_ReportDesc);
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
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;
        case USB_REQ_GET_INTERFACE:
            if (pdev->dev_state == USBD_STATE_CONFIGURED) {
                uint8_t alt = 0U;
                USBD_CtlSendData(pdev, &alt, 1U);
            } else {
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
            }
            break;
        case USB_REQ_SET_INTERFACE:  break;
        case USB_REQ_CLEAR_FEATURE:  break;
        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }
        break;

    default:
        USBD_CtlError(pdev, req);
        ret = USBD_FAIL;
        break;
    }

    return (uint8_t)ret;
}

static uint8_t USBD_HID_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
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
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL) return (uint8_t)USBD_FAIL;

    if (epnum == (HID_KB_EP_IN_ADDR & 0x0FU)) {
        hhid->kb_state = HID_IDLE;
    } else if (epnum == (HID_RAW_EP_IN_ADDR & 0x0FU)) {
        hhid->raw_state = HID_IDLE;
    }
    return USBD_OK;
}

static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if (epnum == (HID_RAW_EP_OUT_ADDR & 0x0FU)) {
        memcpy(s_raw_data_buf, s_raw_rx_buf, HID_RAW_EP_SIZE);
        __DMB();
        s_raw_data_available = true;
        USBD_LL_PrepareReceive(pdev, HID_RAW_EP_OUT_ADDR,
                               s_raw_rx_buf, HID_RAW_EP_SIZE);
    }
    return USBD_OK;
}

static uint8_t *USBD_HID_GetCfgDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USBD_HID_CfgDesc);
    return (uint8_t *)USBD_HID_CfgDesc;
}

static uint8_t USBD_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    0x40,
    0x01,
    0x00,
};

static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);
    return USBD_HID_DeviceQualifierDesc;
}

USBD_StatusTypeDef USBD_HID_SendKeyboardReport(USBD_HandleTypeDef *pdev,
                                                HID_KeyboardReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->kb_state == HID_BUSY) return USBD_BUSY;

    hhid->kb_state = HID_BUSY;
    if (hhid->protocol == HID_PROTOCOL_BOOT) {
        USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR,
                         &report->modifiers, HID_KB_EP_BOOT_SIZE);
    } else {
        USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR,
                         (uint8_t *)report, sizeof(HID_KeyboardReport_t));
    }
    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendSystemReport(USBD_HandleTypeDef *pdev,
                                              uint8_t usage_bitmap)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->kb_state == HID_BUSY) return USBD_BUSY;

    static uint8_t sys_buf[2][2];
    static uint8_t sys_active = 0U;

    uint8_t tx_idx = sys_active;
    sys_active ^= 1U;

    sys_buf[tx_idx][0] = HID_REPORT_ID_SYSTEM;
    sys_buf[tx_idx][1] = usage_bitmap;

    hhid->kb_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR, sys_buf[tx_idx], 2U);
    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendConsumerReport(USBD_HandleTypeDef *pdev,
                                                uint16_t usage_id)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED) {
        return USBD_FAIL;
    }
    if (hhid->kb_state == HID_BUSY) {
        return USBD_BUSY;
    }

    static HID_ConsumerReport_t s_cons_buf[2];
    static uint8_t              s_cons_idx = 0U;

    uint8_t tx_idx = s_cons_idx;
    s_cons_idx ^= 1U;

    s_cons_buf[tx_idx].report_id = HID_REPORT_ID_CONSUMER;
    s_cons_buf[tx_idx].usage_id  = usage_id;

    hhid->kb_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR,
                     (uint8_t *)&s_cons_buf[tx_idx],
                     sizeof(HID_ConsumerReport_t));
    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendNKROReport(USBD_HandleTypeDef *pdev,
                                            HID_NKROReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->kb_state == HID_BUSY) return USBD_BUSY;

    hhid->kb_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR,
                     (uint8_t *)report, sizeof(HID_NKROReport_t));
    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendRawReport(USBD_HandleTypeDef *pdev,
                                           uint8_t *data)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->raw_state == HID_BUSY) return USBD_BUSY;

    hhid->raw_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_RAW_EP_IN_ADDR, data, HID_RAW_EP_SIZE);
    return USBD_OK;
}

bool USBD_HID_RawDataAvailable(void)
{
    return (bool)s_raw_data_available;
}

bool USBD_HID_GetRawData(uint8_t *buf)
{
    if (buf == NULL) return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (!s_raw_data_available) {
        __set_PRIMASK(primask);
        return false;
    }

    memcpy(buf, s_raw_data_buf, HID_RAW_EP_SIZE);
    s_raw_data_available = false;

    __set_PRIMASK(primask);
    return true;
}

bool USBD_HID_KeyboardReady(USBD_HandleTypeDef *pdev)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL || pdev->dev_state != USBD_STATE_CONFIGURED) return false;
    return (hhid->kb_state == HID_IDLE);
}

bool USBD_HID_IsSuspended(USBD_HandleTypeDef *pdev)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;
    if (hhid == NULL) return false;
    return hhid->suspended;
}
