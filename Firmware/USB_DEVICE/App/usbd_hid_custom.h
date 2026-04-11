#ifndef __USBD_HID_CUSTOM_H
#define __USBD_HID_CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"
#include <stdint.h>
#include <stdbool.h>

/* ── HID Class-specific requests ──────────────────────────────── */

#define HID_REQ_GET_REPORT      0x01U
#define HID_REQ_GET_IDLE        0x02U
#define HID_REQ_GET_PROTOCOL    0x03U
#define HID_REQ_SET_REPORT      0x09U
#define HID_REQ_SET_IDLE        0x0AU
#define HID_REQ_SET_PROTOCOL    0x0BU

/* ── HID descriptor types ─────────────────────────────────────── */

#define HID_DESCRIPTOR_TYPE     0x21U
#define HID_REPORT_DESC         0x22U
#define HID_PHYSICAL_DESC       0x23U

/* ── HID protocol values (SET_PROTOCOL / GET_PROTOCOL) ───────── */

#define HID_PROTOCOL_BOOT       0x00U   /* BIOS/UEFI boot protocol */
#define HID_PROTOCOL_REPORT     0x01U   /* Normal report protocol  */

/* ── USB request type masks ───────────────────────────────────── */

#ifndef USB_REQ_TYPE_STANDARD
#define USB_REQ_TYPE_STANDARD   0x00U
#endif
#ifndef USB_REQ_TYPE_CLASS
#define USB_REQ_TYPE_CLASS      0x20U
#endif
#ifndef USB_REQ_TYPE_VENDOR
#define USB_REQ_TYPE_VENDOR     0x40U
#endif
#ifndef USB_REQ_TYPE_MASK
#define USB_REQ_TYPE_MASK       0x60U
#endif

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

/* ── Endpoint addresses and sizes ─────────────────────────────── */

#define HID_KB_EP_IN_ADDR               0x81U
#define HID_KB_EP_IN_SIZE               9U      /* Report protocol: 9 bytes */
#define HID_KB_EP_BOOT_SIZE             8U      /* Boot protocol: 8 bytes   */

#define HID_RAW_EP_IN_ADDR              0x82U
#define HID_RAW_EP_OUT_ADDR             0x02U
#define HID_RAW_EP_SIZE                 32U

/* ── Poll intervals ────────────────────────────────────────────── */

#define HID_KB_POLL_INTERVAL            1U
#define HID_RAW_POLL_INTERVAL           1U

/* ── Interface numbers ────────────────────────────────────────── */

#define HID_KB_INTERFACE_NUM            0U
#define HID_RAW_INTERFACE_NUM           1U

/* ── Report IDs ────────────────────────────────────────────────── */

#define HID_REPORT_ID_KEYBOARD          0x01U
#define HID_REPORT_ID_NKRO              0x02U

/* ── Report structures ────────────────────────────────────────── */

/*
 * Standard 6KRO keyboard report (Report Protocol).
 * Total: 9 bytes including report_id.
 *
 * Boot Protocol variant omits report_id → 8 bytes.
 * See USBD_HID_SendKeyboardReport() for protocol-aware sending.
 */
typedef struct {
    uint8_t report_id;      /* Always HID_REPORT_ID_KEYBOARD (0x01) */
    uint8_t modifiers;      /* Modifier bitmask */
    uint8_t reserved;       /* Always 0x00 */
    uint8_t keycodes[6];    /* HID usage IDs, 0x00 = no key */
} __attribute__((packed)) HID_KeyboardReport_t;

/*
 * Boot Protocol report layout (no report_id prefix):
 *   [modifiers:1][reserved:1][keycodes:6] = 8 bytes
 * We reuse HID_KeyboardReport_t and skip the first byte when sending
 * in boot protocol mode. See USBD_HID_SendKeyboardReport().
 */

typedef struct {
    uint8_t report_id;
    uint8_t modifiers;
    uint8_t bitmap[15];
} __attribute__((packed)) HID_NKROReport_t;

typedef struct {
    uint8_t command;
    uint8_t data[31];
} __attribute__((packed)) HID_RawPacket_t;

/* ── HID endpoint state ────────────────────────────────────────── */

typedef enum {
    HID_IDLE    = 0x00U,
    HID_BUSY    = 0x01U
} HID_StateTypeDef;

/* ── HID class handle ──────────────────────────────────────────── */

typedef struct {
    HID_StateTypeDef kb_state;
    HID_StateTypeDef raw_state;
    uint8_t          raw_rx_buf[HID_RAW_EP_SIZE];

    /*
     * protocol: 0 = Boot Protocol, 1 = Report Protocol (default).
     * Set by SET_PROTOCOL request from host.
     * BIOS/UEFI typically sets protocol=0 during POST.
     * OS will set protocol=1 after loading HID driver.
     */
    uint8_t          protocol;

    uint8_t          idle_rate;
    bool             kb_report_pending;
} USBD_HID_Custom_HandleTypeDef;

/* ── Configuration descriptor size ────────────────────────────── */

#define HID_CUSTOM_CONFIG_DESC_SIZE     66U

/* ── Class object ──────────────────────────────────────────────── */

extern USBD_ClassTypeDef USBD_HID_Custom;

/* ── Public API ────────────────────────────────────────────────── */

/*
 * Send a keyboard report.
 * Automatically selects Boot Protocol (8 bytes, no report ID) or
 * Report Protocol (9 bytes, with report ID) based on the current
 * HID protocol negotiated with the host.
 */
USBD_StatusTypeDef USBD_HID_SendKeyboardReport(USBD_HandleTypeDef *pdev,
                                                HID_KeyboardReport_t *report);

USBD_StatusTypeDef USBD_HID_SendNKROReport(USBD_HandleTypeDef *pdev,
                                            HID_NKROReport_t *report);

USBD_StatusTypeDef USBD_HID_SendRawReport(USBD_HandleTypeDef *pdev,
                                           uint8_t *data);

bool USBD_HID_RawDataAvailable(void);

bool USBD_HID_GetRawData(uint8_t *buf);

bool USBD_HID_KeyboardReady(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif
