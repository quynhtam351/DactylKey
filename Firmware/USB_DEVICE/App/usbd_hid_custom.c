#include "usbd_hid_custom.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include <string.h>

/*
 * USB HID Custom Class
 * ════════════════════
 *
 * Implements a composite HID device with two interfaces:
 *   Interface 0: Boot-compatible Keyboard (6KRO)
 *   Interface 1: Raw HID (vendor-defined, 32-byte bidirectional)
 *
 * Boot Protocol Compliance:
 * Interface 0 declares bInterfaceSubClass=1 (Boot) and
 * bInterfaceProtocol=1 (Keyboard). This means BIOS/UEFI can use
 * this keyboard during POST via Boot Protocol.
 *
 * Boot Protocol (protocol=0):
 *   Report format: [modifier:1][reserved:1][keycodes:6] = 8 bytes
 *   No Report ID prefix. This is the format BIOS expects.
 *
 * Report Protocol (protocol=1, default):
 *   Report format: [report_id:1][modifier:1][reserved:1][keycodes:6] = 9 bytes
 *   Report ID allows multiplexing multiple report types on one endpoint.
 *
 * The host sends SET_PROTOCOL to switch between modes. We store the
 * protocol value and USBD_HID_SendKeyboardReport() uses it to select
 * the correct byte layout.
 *
 * HID Descriptor offsets in USBD_HID_CfgDesc:
 *   Interface 0 HID descriptor starts at byte 18
 *   Interface 1 HID descriptor starts at byte 43
 * These offsets are used in GET_DESCRIPTOR handling.
 */

/* ── HID Report Descriptors ───────────────────────────────────── */

static const uint8_t HID_KB_ReportDesc[] = {
    /* Usage Page: Generic Desktop */
    0x05, 0x01,
    /* Usage: Keyboard */
    0x09, 0x06,
    /* Collection: Application */
    0xA1, 0x01,
    /* Report ID 1 */
    0x85, HID_REPORT_ID_KEYBOARD,

    /* ── Modifier keys: 8 bits ───────────────────────────────── */
    /* Usage Page: Keyboard/Keypad */
    0x05, 0x07,
    /* Usage Minimum: Left Control (0xE0) */
    0x19, 0xE0,
    /* Usage Maximum: Right GUI (0xE7) */
    0x29, 0xE7,
    /* Logical Minimum: 0 */
    0x15, 0x00,
    /* Logical Maximum: 1 */
    0x25, 0x01,
    /* Report Size: 1 bit */
    0x75, 0x01,
    /* Report Count: 8 */
    0x95, 0x08,
    /* Input: Data, Variable, Absolute */
    0x81, 0x02,

    /* ── Reserved byte: 8 bits constant ─────────────────────── */
    /* Report Count: 1 */
    0x95, 0x01,
    /* Report Size: 8 bits */
    0x75, 0x08,
    /* Input: Constant, Variable, Absolute */
    0x81, 0x03,

    /* ── LED output: 5 bits + 3 padding ─────────────────────── */
    /* Usage Page: LEDs */
    0x05, 0x08,
    /* Usage Minimum: Num Lock (1) */
    0x19, 0x01,
    /* Usage Maximum: Kana (5) */
    0x29, 0x05,
    /* Report Count: 5 */
    0x95, 0x05,
    /* Report Size: 1 bit */
    0x75, 0x01,
    /* Output: Data, Variable, Absolute */
    0x91, 0x02,
    /* Report Count: 1 (padding) */
    0x95, 0x01,
    /* Report Size: 3 bits */
    0x75, 0x03,
    /* Output: Constant, Variable, Absolute */
    0x91, 0x03,

    /* ── Keycodes: 6 bytes ───────────────────────────────────── */
    /* Usage Page: Keyboard/Keypad */
    0x05, 0x07,
    /* Usage Minimum: 0x00 */
    0x19, 0x00,
    /* Usage Maximum: 0xE7 */
    0x29, 0xE7,
    /* Logical Minimum: 0 */
    0x15, 0x00,
    /* Logical Maximum: 231 (0xE7) */
    0x26, 0xE7, 0x00,
    /* Report Count: 6 */
    0x95, 0x06,
    /* Report Size: 8 bits */
    0x75, 0x08,
    /* Input: Data, Array (not Variable → allows ErrorRollOver) */
    0x81, 0x00,

    /* End Collection */
    0xC0,
};

static const uint8_t HID_Raw_ReportDesc[] = {
    /* Usage Page: Vendor Defined (0xFF60) */
    0x06, 0x60, 0xFF,
    /* Usage: 0x61 */
    0x09, 0x61,
    /* Collection: Application */
    0xA1, 0x01,

    /* Input (Device → Host): 32 bytes */
    0x09, 0x62,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, HID_RAW_EP_SIZE,
    0x81, 0x02,

    /* Output (Host → Device): 32 bytes */
    0x09, 0x63,
    0x15, 0x00,
    0x26, 0xFF, 0x00,
    0x75, 0x08,
    0x95, HID_RAW_EP_SIZE,
    0x91, 0x02,

    /* End Collection */
    0xC0,
};

/* ── Configuration Descriptor ─────────────────────────────────── */

/*
 * Total size: 66 bytes
 *   9  Config
 *   9  Interface 0 (Keyboard)
 *   9  HID Descriptor 0
 *   7  Endpoint 0x81
 *   9  Interface 1 (Raw HID)
 *   9  HID Descriptor 1
 *   7  Endpoint 0x82
 *   7  Endpoint 0x02
 *  ──
 *  66
 */
static const uint8_t USBD_HID_CfgDesc[HID_CUSTOM_CONFIG_DESC_SIZE] = {

    /* ── Configuration Descriptor (9 bytes, offset 0) ───────── */
    0x09,                                   /* bLength */
    USB_DESC_TYPE_CONFIGURATION,            /* bDescriptorType */
    LOBYTE(HID_CUSTOM_CONFIG_DESC_SIZE),    /* wTotalLength L */
    HIBYTE(HID_CUSTOM_CONFIG_DESC_SIZE),    /* wTotalLength H */
    0x02,                                   /* bNumInterfaces */
    0x01,                                   /* bConfigurationValue */
    0x00,                                   /* iConfiguration */
    0xA0,                                   /* bmAttributes: Bus Powered + Remote Wakeup */
    0x32,                                   /* bMaxPower: 100mA */

    /* ── Interface 0: HID Keyboard (9 bytes, offset 9) ──────── */
    0x09,                                   /* bLength */
    USB_DESC_TYPE_INTERFACE,                /* bDescriptorType */
    HID_KB_INTERFACE_NUM,                   /* bInterfaceNumber: 0 */
    0x00,                                   /* bAlternateSetting */
    0x01,                                   /* bNumEndpoints */
    0x03,                                   /* bInterfaceClass: HID */
    0x01,                                   /* bInterfaceSubClass: Boot */
    0x01,                                   /* bInterfaceProtocol: Keyboard */
    0x00,                                   /* iInterface */

    /* ── HID Descriptor 0 (9 bytes, offset 18) ──────────────── */
    0x09,                                   /* bLength */
    0x21,                                   /* bDescriptorType: HID */
    0x11, 0x01,                             /* bcdHID: 1.11 */
    0x00,                                   /* bCountryCode */
    0x01,                                   /* bNumDescriptors */
    0x22,                                   /* bDescriptorType: Report */
    LOBYTE(sizeof(HID_KB_ReportDesc)),      /* wDescriptorLength L */
    HIBYTE(sizeof(HID_KB_ReportDesc)),      /* wDescriptorLength H */

    /* ── Endpoint 0x81 IN (7 bytes, offset 27) ──────────────── */
    0x07,                                   /* bLength */
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType */
    HID_KB_EP_IN_ADDR,                      /* bEndpointAddress: 0x81 IN */
    0x03,                                   /* bmAttributes: Interrupt */
    LOBYTE(HID_KB_EP_IN_SIZE),              /* wMaxPacketSize L */
    HIBYTE(HID_KB_EP_IN_SIZE),              /* wMaxPacketSize H */
    HID_KB_POLL_INTERVAL,                   /* bInterval: 1ms */

    /* ── Interface 1: Raw HID (9 bytes, offset 34) ───────────── */
    0x09,                                   /* bLength */
    USB_DESC_TYPE_INTERFACE,                /* bDescriptorType */
    HID_RAW_INTERFACE_NUM,                  /* bInterfaceNumber: 1 */
    0x00,                                   /* bAlternateSetting */
    0x02,                                   /* bNumEndpoints */
    0x03,                                   /* bInterfaceClass: HID */
    0x00,                                   /* bInterfaceSubClass: None */
    0x00,                                   /* bInterfaceProtocol: None */
    0x00,                                   /* iInterface */

    /* ── HID Descriptor 1 (9 bytes, offset 43) ──────────────── */
    0x09,                                   /* bLength */
    0x21,                                   /* bDescriptorType: HID */
    0x11, 0x01,                             /* bcdHID: 1.11 */
    0x00,                                   /* bCountryCode */
    0x01,                                   /* bNumDescriptors */
    0x22,                                   /* bDescriptorType: Report */
    LOBYTE(sizeof(HID_Raw_ReportDesc)),     /* wDescriptorLength L */
    HIBYTE(sizeof(HID_Raw_ReportDesc)),     /* wDescriptorLength H */

    /* ── Endpoint 0x82 IN (7 bytes, offset 52) ──────────────── */
    0x07,                                   /* bLength */
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType */
    HID_RAW_EP_IN_ADDR,                     /* bEndpointAddress: 0x82 IN */
    0x03,                                   /* bmAttributes: Interrupt */
    LOBYTE(HID_RAW_EP_SIZE),                /* wMaxPacketSize L */
    HIBYTE(HID_RAW_EP_SIZE),                /* wMaxPacketSize H */
    HID_RAW_POLL_INTERVAL,                  /* bInterval: 1ms */

    /* ── Endpoint 0x02 OUT (7 bytes, offset 59) ─────────────── */
    0x07,                                   /* bLength */
    USB_DESC_TYPE_ENDPOINT,                 /* bDescriptorType */
    HID_RAW_EP_OUT_ADDR,                    /* bEndpointAddress: 0x02 OUT */
    0x03,                                   /* bmAttributes: Interrupt */
    LOBYTE(HID_RAW_EP_SIZE),                /* wMaxPacketSize L */
    HIBYTE(HID_RAW_EP_SIZE),                /* wMaxPacketSize H */
    HID_RAW_POLL_INTERVAL,                  /* bInterval: 1ms */
};

/* ── Static state ─────────────────────────────────────────────── */

static USBD_HID_Custom_HandleTypeDef s_hid_handle;

static uint8_t s_raw_rx_buf[HID_RAW_EP_SIZE];
static uint8_t s_raw_data_buf[HID_RAW_EP_SIZE];
static bool    s_raw_data_available = false;

/* ── Private function prototypes ──────────────────────────────── */

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t USBD_HID_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t USBD_HID_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_HID_GetCfgDesc(uint16_t *length);
static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length);

/* ── Class object ─────────────────────────────────────────────── */

USBD_ClassTypeDef USBD_HID_Custom = {
    USBD_HID_Init,
    USBD_HID_DeInit,
    USBD_HID_Setup,
    NULL,   /* EP0_TxSent */
    NULL,   /* EP0_RxReady */
    USBD_HID_DataIn,
    USBD_HID_DataOut,
    NULL,   /* SOF */
    NULL,   /* IsoINIncomplete */
    NULL,   /* IsoOUTIncomplete */
    USBD_HID_GetCfgDesc,
    USBD_HID_GetCfgDesc,
    USBD_HID_GetCfgDesc,
    USBD_HID_GetDeviceQualifierDesc,
};

/* ── Class callbacks ──────────────────────────────────────────── */

static uint8_t USBD_HID_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;

    memset(&s_hid_handle, 0, sizeof(USBD_HID_Custom_HandleTypeDef));

    /*
     * Default to Report Protocol (1).
     * Host will send SET_PROTOCOL(0) if it wants Boot Protocol.
     */
    s_hid_handle.protocol  = HID_PROTOCOL_REPORT;
    s_hid_handle.idle_rate = 0U;
    pdev->pClassData = &s_hid_handle;

    /* Open keyboard IN endpoint */
    USBD_LL_OpenEP(pdev, HID_KB_EP_IN_ADDR, USBD_EP_TYPE_INTR, HID_KB_EP_IN_SIZE);
    pdev->ep_in[HID_KB_EP_IN_ADDR & 0xFU].is_used = 1U;

    /* Open Raw HID IN endpoint */
    USBD_LL_OpenEP(pdev, HID_RAW_EP_IN_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_in[HID_RAW_EP_IN_ADDR & 0xFU].is_used = 1U;

    /* Open Raw HID OUT endpoint */
    USBD_LL_OpenEP(pdev, HID_RAW_EP_OUT_ADDR, USBD_EP_TYPE_INTR, HID_RAW_EP_SIZE);
    pdev->ep_out[HID_RAW_EP_OUT_ADDR & 0xFU].is_used = 1U;

    /* Prepare OUT endpoint to receive */
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

static uint8_t USBD_HID_Setup(USBD_HandleTypeDef *pdev,
                               USBD_SetupReqTypedef *req)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    uint16_t len   = 0U;
    uint8_t *pbuf  = NULL;
    uint16_t status_info = 0U;
    USBD_StatusTypeDef ret = USBD_OK;

    if (hhid == NULL) return USBD_FAIL;

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {

    /* ── HID Class requests ───────────────────────────────────── */
    case USB_REQ_TYPE_CLASS:
        switch (req->bRequest) {

        case HID_REQ_SET_PROTOCOL:
            /*
             * Host switches protocol:
             *   0 = Boot Protocol  (BIOS, no Report ID, 8 bytes)
             *   1 = Report Protocol (OS, with Report ID, 9 bytes)
             *
             * Store as uint8_t. USBD_HID_SendKeyboardReport() will
             * read this to select the correct report format.
             */
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

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }
        break;

    /* ── Standard requests ────────────────────────────────────── */
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
                /* Select report descriptor by interface number */
                if ((req->wIndex & 0xFFU) == HID_KB_INTERFACE_NUM) {
                    pbuf = (uint8_t *)HID_KB_ReportDesc;
                    len  = sizeof(HID_KB_ReportDesc);
                } else {
                    pbuf = (uint8_t *)HID_Raw_ReportDesc;
                    len  = sizeof(HID_Raw_ReportDesc);
                }
                len = MIN(len, req->wLength);
                USBD_CtlSendData(pdev, pbuf, len);

            } else if ((req->wValue >> 8) == HID_DESCRIPTOR_TYPE) {
                /*
                 * HID descriptor offsets in USBD_HID_CfgDesc:
                 *   Interface 0 HID descriptor: offset 18
                 *   Interface 1 HID descriptor: offset 43
                 */
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

        case USB_REQ_SET_INTERFACE:
            /* Acknowledged, no action needed for single alt setting */
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
        s_raw_data_available = true;

        /* Re-arm the OUT endpoint for the next packet */
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
    0x00,
    0x00,
    0x00,
    0x40,
    0x01,
    0x00,
};

static uint8_t *USBD_HID_GetDeviceQualifierDesc(uint16_t *length)
{
    *length = (uint16_t)sizeof(USBD_HID_DeviceQualifierDesc);
    return USBD_HID_DeviceQualifierDesc;
}

/* ═══════════════════════════════════════════════════════════════ */
/*                        PUBLIC API                               */
/* ═══════════════════════════════════════════════════════════════ */

/*
 * USBD_HID_SendKeyboardReport
 * ────────────────────────────
 * Sends a keyboard report using the correct format for the current
 * HID protocol negotiated with the host:
 *
 * Report Protocol (default, protocol=1):
 *   Transmits all 9 bytes: [report_id][modifiers][reserved][keys×6]
 *   Host HID driver uses report_id to identify the report type.
 *
 * Boot Protocol (BIOS mode, protocol=0):
 *   Transmits 8 bytes, SKIPPING report_id: [modifiers][reserved][keys×6]
 *   BIOS expects exactly this format per HID Boot spec.
 *   The host has no HID driver in this mode, so report_id would confuse it.
 */
USBD_StatusTypeDef USBD_HID_SendKeyboardReport(USBD_HandleTypeDef *pdev,
                                                HID_KeyboardReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    if (hhid == NULL)                          return USBD_FAIL;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->kb_state == HID_BUSY)            return USBD_BUSY;

    hhid->kb_state = HID_BUSY;

    if (hhid->protocol == HID_PROTOCOL_BOOT) {
        /*
         * Boot Protocol: 8 bytes, no Report ID.
         * &report->modifiers points to the byte immediately after
         * report_id in the packed struct.
         */
        USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR,
                         &report->modifiers,
                         HID_KB_EP_BOOT_SIZE);
    } else {
        /*
         * Report Protocol: 9 bytes including Report ID.
         */
        USBD_LL_Transmit(pdev, HID_KB_EP_IN_ADDR,
                         (uint8_t *)report,
                         sizeof(HID_KeyboardReport_t));
    }

    return USBD_OK;
}

USBD_StatusTypeDef USBD_HID_SendNKROReport(USBD_HandleTypeDef *pdev,
                                            HID_NKROReport_t *report)
{
    USBD_HID_Custom_HandleTypeDef *hhid =
        (USBD_HID_Custom_HandleTypeDef *)pdev->pClassData;

    if (hhid == NULL)                          return USBD_FAIL;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->kb_state == HID_BUSY)            return USBD_BUSY;

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

    if (hhid == NULL)                          return USBD_FAIL;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return USBD_FAIL;
    if (hhid->raw_state == HID_BUSY)           return USBD_BUSY;

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

    if (hhid == NULL)                          return false;
    if (pdev->dev_state != USBD_STATE_CONFIGURED) return false;

    return (hhid->kb_state == HID_IDLE);
}
