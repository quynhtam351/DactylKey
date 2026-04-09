#include "usbd_hid_custom.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include <string.h>

/* =========================================================
 * HID REPORT DESCRIPTORS
 * ========================================================= */

/* Interface 0: Keyboard Report Descriptor
 * Hỗ trợ cả Boot Protocol (6KRO) và Report Protocol (NKRO)
 */
static const uint8_t HID_KB_ReportDesc[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, HID_REPORT_ID_KEYBOARD,

    // Modifiers
    0x05, 0x07,
    0x19, 0xE0,
    0x29, 0xE7,
    0x15, 0x00,
    0x25, 0x01,
    0x75, 0x01,
    0x95, 0x08,
    0x81, 0x02,

    // Reserved
    0x95, 0x01,
    0x75, 0x08,
    0x81, 0x03,

    // LEDs output (optional nhưng Windows expects this)
    0x05, 0x08,        // Usage Page (LEDs)
    0x19, 0x01,        // Usage Min (Num Lock)
    0x29, 0x05,        // Usage Max (Kana)
    0x95, 0x05,        // Report Count (5)
    0x75, 0x01,        // Report Size (1)
    0x91, 0x02,        // Output (Data, Var, Abs)
    0x95, 0x01,        // Report Count (1)
    0x75, 0x03,        // Report Size (3) — padding
    0x91, 0x03,        // Output (Const, Var, Abs)

    // Keycodes
    0x05, 0x07,
    0x19, 0x00,
    0x29, 0xE7,        // Usage Max = 0xE7 (Right GUI)
    0x15, 0x00,
    0x26, 0xE7, 0x00,  // Logical Max = 231 (2-byte format)
    0x95, 0x06,
    0x75, 0x08,
    0x81, 0x00,

    0xC0,
};

/* Interface 1: Raw HID Report Descriptor
 * Usage Page 0xFF60 là vendor-defined, tương thích VIA
 */
static const uint8_t HID_Raw_ReportDesc[] = {
    0x06, 0x60, 0xFF,   /* Usage Page: Vendor Defined (0xFF60) */
    0x09, 0x61,         /* Usage: Vendor Usage 0x61 */
    0xA1, 0x01,         /* Collection: Application */

    /* IN report: Firmware → Host (32 bytes) */
    0x09, 0x62,         /* Usage: Vendor Usage 0x62 */
    0x15, 0x00,         /* Logical Minimum: 0 */
    0x26, 0xFF, 0x00,   /* Logical Maximum: 255 */
    0x75, 0x08,         /* Report Size: 8 bits */
    0x95, HID_RAW_EP_SIZE, /* Report Count: 32 */
    0x81, 0x02,         /* Input: Data, Variable, Absolute */

    /* OUT report: Host → Firmware (32 bytes) */
    0x09, 0x63,         /* Usage: Vendor Usage 0x63 */
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, HID_RAW_EP_SIZE,
    0x91, 0x02,         /* Output: Data, Variable, Absolute */

    0xC0,               /* End Collection */
};

/* =========================================================
 * CONFIGURATION DESCRIPTOR
 * Mô tả toàn bộ cấu hình USB: 2 interfaces, 3 endpoints
 * ========================================================= */
static const uint8_t USBD_HID_CfgDesc[HID_CUSTOM_CONFIG_DESC_SIZE] = {
    /* ---- Configuration Descriptor (9 bytes) ---- */
    0x09,                           /* bLength */
    USB_DESC_TYPE_CONFIGURATION,    /* bDescriptorType: Configuration */
    LOBYTE(HID_CUSTOM_CONFIG_DESC_SIZE), /* wTotalLength low */
    HIBYTE(HID_CUSTOM_CONFIG_DESC_SIZE), /* wTotalLength high */
    0x02,                           /* bNumInterfaces: 2 */
    0x01,                           /* bConfigurationValue: 1 */
    0x00,                           /* iConfiguration: No string */
    0xA0,                           /* bmAttributes: Bus powered + Remote wakeup */
    0x32,                           /* MaxPower: 100mA (50 × 2mA) */

    /* ---- Interface 0: HID Keyboard (9 bytes) ---- */
    0x09,                           /* bLength */
    USB_DESC_TYPE_INTERFACE,        /* bDescriptorType: Interface */
    HID_KB_INTERFACE_NUM,           /* bInterfaceNumber: 0 */
    0x00,                           /* bAlternateSetting: 0 */
    0x01,                           /* bNumEndpoints: 1 (EP1 IN only) */
    0x03,                           /* bInterfaceClass: HID */
    0x01,                           /* bInterfaceSubClass: Boot Interface */
    0x01,                           /* bInterfaceProtocol: Keyboard */
    0x00,                           /* iInterface: No string */

    /* ---- HID Descriptor Interface 0 (9 bytes) ---- */
    0x09,                           /* bLength */
    0x21,                           /* bDescriptorType: HID */
    0x11, 0x01,                     /* bcdHID: HID version 1.11 */
    0x00,                           /* bCountryCode: Not localized */
    0x01,                           /* bNumDescriptors: 1 */
    0x22,                           /* bDescriptorType: Report */
    LOBYTE(sizeof(HID_KB_ReportDesc)),  /* wDescriptorLength low */
    HIBYTE(sizeof(HID_KB_ReportDesc)),  /* wDescriptorLength high */

    /* ---- Endpoint 1 IN: Keyboard (7 bytes) ---- */
    0x07,                           /* bLength */
    USB_DESC_TYPE_ENDPOINT,         /* bDescriptorType: Endpoint */
    HID_KB_EP_IN_ADDR,              /* bEndpointAddress: EP1 IN (0x81) */
    0x03,                           /* bmAttributes: Interrupt */
    LOBYTE(HID_KB_EP_IN_SIZE),      /* wMaxPacketSize low: 8 bytes */
    HIBYTE(HID_KB_EP_IN_SIZE),      /* wMaxPacketSize high */
    HID_KB_POLL_INTERVAL,           /* bInterval: 1ms */

    /* ---- Interface 1: HID Raw (9 bytes) ---- */
    0x09,
    USB_DESC_TYPE_INTERFACE,
    HID_RAW_INTERFACE_NUM,          /* bInterfaceNumber: 1 */
    0x00,
    0x02,                           /* bNumEndpoints: 2 (EP2 IN + EP2 OUT) */
    0x03,                           /* bInterfaceClass: HID */
    0x00,                           /* bInterfaceSubClass: None */
    0x00,                           /* bInterfaceProtocol: None */
    0x00,

    /* ---- HID Descriptor Interface 1 (9 bytes) ---- */
    0x09,
    0x21,
    0x11, 0x01,                     /* bcdHID: 1.11 */
    0x00,
    0x01,
    0x22,
    LOBYTE(sizeof(HID_Raw_ReportDesc)),
    HIBYTE(sizeof(HID_Raw_ReportDesc)),

    /* ---- Endpoint 2 IN: Raw HID TX (7 bytes) ---- */
    0x07,
    USB_DESC_TYPE_ENDPOINT,
    HID_RAW_EP_IN_ADDR,             /* EP2 IN (0x82) */
    0x03,                           /* Interrupt */
    LOBYTE(HID_RAW_EP_SIZE),        /* 32 bytes */
    HIBYTE(HID_RAW_EP_SIZE),
    HID_RAW_POLL_INTERVAL,          /* 1ms */

    /* ---- Endpoint 2 OUT: Raw HID RX (7 bytes) ---- */
    0x07,
    USB_DESC_TYPE_ENDPOINT,
    HID_RAW_EP_OUT_ADDR,            /* EP2 OUT (0x02) */
    0x03,                           /* Interrupt */
    LOBYTE(HID_RAW_EP_SIZE),        /* 32 bytes */
    HIBYTE(HID_RAW_EP_SIZE),
    HID_RAW_POLL_INTERVAL,          /* 1ms */
};

/* =========================================================
 * PRIVATE VARIABLES
 * ========================================================= */
static USBD_HID_Custom_HandleTypeDef s_hid_handle;

/* Raw HID receive ring buffer (double buffer đơn giản) */
static uint8_t  s_raw_rx_buf[HID_RAW_EP_SIZE];
static uint8_t  s_raw_data_buf[HID_RAW_EP_SIZE];
static bool     s_raw_data_available = false;

/* =========================================================
 * PRIVATE FUNCTION PROTOTYPES
 * ========================================================= */
static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_HID_GetCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);

/* =========================================================
 * CLASS DRIVER INTERFACE TABLE
 * Đăng ký với USB Device stack
 * ========================================================= */
USBD_ClassTypeDef USBD_HID_Custom = {
    USBD_HID_Init,
    USBD_HID_DeInit,
    USBD_HID_Setup,
    NULL,                               /* EP0_TxSent - không dùng */
    NULL,                               /* EP0_RxReady - không dùng */
    USBD_HID_DataIn,
    USBD_HID_DataOut,
    NULL,                               /* SOF - không dùng */
    NULL,                               /* IsoINIncomplete */
    NULL,                               /* IsoOUTIncomplete */
    USBD_HID_GetCfgDesc,
    USBD_HID_GetCfgDesc,                /* Same for High Speed */
    USBD_HID_GetCfgDesc,                /* Same for Other Speed */
    USBD_HID_GetDeviceQualifierDesc,
};

/* =========================================================
 * CLASS DRIVER IMPLEMENTATION
 * ========================================================= */

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;

    /* Reset handle */
    memset(&s_hid_handle, 0, sizeof(USBD_HID_Custom_HandleTypeDef));
    s_hid_handle.protocol  = 1U; /* Report Protocol mặc định */
    s_hid_handle.idle_rate = 0U; /* 0 = chỉ gửi khi có thay đổi */
    pdev->pClassData = &s_hid_handle;

    /* Mở EP1 IN: Keyboard (8 bytes, Interrupt) */
    USBD_LL_OpenEP(pdev, HID_KB_EP_IN_ADDR, USBD_EP_TYPE_INTR, HID_KB_EP_IN_SIZE);
    pdev->ep_in[HID_KB_EP_IN_ADDR & 0xFU].is_used = 1U;

    /* Mở EP2 IN: Raw HID TX (32 bytes, Interrupt) */
    USBD_LL_OpenEP(pdev, HID_RAW_EP_IN_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_in[HID_RAW_EP_IN_ADDR & 0xFU].is_used = 1U;

    /* Mở EP2 OUT: Raw HID RX (32 bytes, Interrupt) */
    USBD_LL_OpenEP(pdev, HID_RAW_EP_OUT_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_out[HID_RAW_EP_OUT_ADDR & 0xFU].is_used = 1U;

    /* Bắt đầu nhận Raw HID data từ host */
    USBD_LL_PrepareReceive(pdev, HID_RAW_EP_OUT_ADDR,
                           s_raw_rx_buf, HID_RAW_EP_SIZE);

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
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    uint16_t len   = 0U;
    uint8_t *pbuf  = NULL;
    uint16_t status_info = 0U;
    USBD_StatusTypeDef ret = USBD_OK;

    if (hhid == NULL) return USBD_FAIL;

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {

    /* ---- Class-specific requests ---- */
    case USB_REQ_TYPE_CLASS:
        switch (req->bRequest) {

        case HID_REQ_SET_PROTOCOL:
            hhid->protocol = (uint8_t)req->wValue;
            break;

        case HID_REQ_GET_PROTOCOL:
            USBD_CtlSendData(pdev, (uint8_t *)&hhid->protocol, 1U);
            break;

        case HID_REQ_SET_IDLE:
            hhid->idle_rate = (uint8_t)(req->wValue >> 8);
            break;

        case HID_REQ_GET_IDLE:
            USBD_CtlSendData(pdev, (uint8_t *)&hhid->idle_rate, 1U);
            break;

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }
        break;

    /* ---- Standard requests ---- */
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
            /* Host yêu cầu HID Report Descriptor */
            if (req->wValue >> 8 == HID_REPORT_DESC) {
                /* Phân biệt Interface 0 (KB) và Interface 1 (Raw) */
                if ((req->wIndex & 0xFF) == HID_KB_INTERFACE_NUM) {
                    pbuf = (uint8_t *)HID_KB_ReportDesc;
                    len  = sizeof(HID_KB_ReportDesc);
                } else {
                    pbuf = (uint8_t *)HID_Raw_ReportDesc;
                    len  = sizeof(HID_Raw_ReportDesc);
                }
                len = MIN(len, req->wLength);
                USBD_CtlSendData(pdev, pbuf, len);
            }
            /* Host yêu cầu HID Descriptor */
            else if (req->wValue >> 8 == HID_DESCRIPTOR_TYPE) {
                /* Trả về phần HID descriptor trong Config descriptor */
                if ((req->wIndex & 0xFF) == HID_KB_INTERFACE_NUM) {
                    pbuf = (uint8_t *)USBD_HID_CfgDesc + 18U; /* Offset đến HID desc IF0 */
                } else {
                    pbuf = (uint8_t *)USBD_HID_CfgDesc + 43U; /* Offset đến HID desc IF1 */
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

        case USB_REQ_SET_INTERFACE:
            /* Không dùng alternate setting */
            break;

        case USB_REQ_CLEAR_FEATURE:
            break;

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

static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    if (hhid == NULL) return USBD_FAIL;

    /* EP1 IN: Keyboard report đã gửi xong */
    if (epnum == (HID_KB_EP_IN_ADDR & 0x0FU)) {
        hhid->kb_state = HID_IDLE;
    }
    /* EP2 IN: Raw HID response đã gửi xong */
    else if (epnum == (HID_RAW_EP_IN_ADDR & 0x0FU)) {
        hhid->raw_state = HID_IDLE;
    }

    return USBD_OK;
}

static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    /* EP2 OUT: Nhận được Raw HID packet từ host */
    if (epnum == HID_RAW_EP_OUT_ADDR) {
        /* Copy data vào buffer và set flag */
        memcpy(s_raw_data_buf, s_raw_rx_buf, HID_RAW_EP_SIZE);
        s_raw_data_available = true;

        /* Chuẩn bị nhận packet tiếp theo */
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

/* Device Qualifier Descriptor (cho High Speed devices) */
static uint8_t USBD_HID_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] = {
    USB_LEN_DEV_QUALIFIER_DESC,         /* bLength */
    USB_DESC_TYPE_DEVICE_QUALIFIER,     /* bDescriptorType */
    0x00, 0x02,                         /* bcdUSB: 2.0 */
    0x00,                               /* bDeviceClass */
    0x00,                               /* bDeviceSubClass */
    0x00,                               /* bDeviceProtocol */
    0x40,                               /* bMaxPacketSize0: 64 */
    0x01,                               /* bNumConfigurations: 1 */
    0x00,                               /* bReserved */
};

static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);
    return USBD_HID_DeviceQualifierDesc;
}

/* =========================================================
 * PUBLIC API IMPLEMENTATION
 * ========================================================= */

USBD_StatusTypeDef USBD_HID_SendKeyboardReport(USBD_HandleTypeDef *pdev,
                                                HID_KeyboardReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    if (hhid == NULL) return USBD_FAIL;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->kb_state == HID_BUSY) return USBD_BUSY;

    hhid->kb_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR,
                     (uint8_t *)report, sizeof(HID_KeyboardReport_t));

    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendNKROReport(USBD_HandleTypeDef *pdev,
                                            HID_NKROReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    if (hhid == NULL) return USBD_FAIL;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
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

    if (hhid == NULL) return USBD_FAIL;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->raw_state == HID_BUSY) return USBD_BUSY;

    hhid->raw_state = HID_BUSY;
    USBD_LL_Transmit(pdev, HID_RAW_EP_IN_ADDR, data, HID_RAW_EP_SIZE);

    return USBD_OK;
}

bool USBD_HID_RawDataAvailable(void)
{
    return s_raw_data_available;
}

bool USBD_HID_GetRawData(uint8_t *buf)
{
    if (!s_raw_data_available) return false;

    memcpy(buf, s_raw_data_buf, HID_RAW_EP_SIZE);
    s_raw_data_available = false;

    return true;
}

bool USBD_HID_KeyboardReady(USBD_HandleTypeDef *pdev)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    if (hhid == NULL) return false;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return false;

    return (hhid->kb_state == HID_IDLE);
}
