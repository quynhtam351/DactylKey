#ifndef __USBD_HID_CUSTOM_H
#define __USBD_HID_CUSTOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"
#include <stdint.h>
#include <stdbool.h>

#define HID_REQ_GET_REPORT      0x01U
#define HID_REQ_GET_IDLE        0x02U
#define HID_REQ_GET_PROTOCOL    0x03U
#define HID_REQ_SET_REPORT      0x09U
#define HID_REQ_SET_IDLE        0x0AU
#define HID_REQ_SET_PROTOCOL    0x0BU

#define HID_DESCRIPTOR_TYPE     0x21U
#define HID_REPORT_DESC         0x22U
#define HID_PHYSICAL_DESC       0x23U

#define HID_PROTOCOL_BOOT       0x00U
#define HID_PROTOCOL_REPORT     0x01U

#define HID_REPORT_TYPE_INPUT   0x01U
#define HID_REPORT_TYPE_OUTPUT  0x02U
#define HID_REPORT_TYPE_FEATURE 0x03U

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

#define HID_KB_EP_IN_ADDR               0x81U
#define HID_KB_EP_IN_SIZE               9U
#define HID_KB_EP_BOOT_SIZE             8U

#define HID_RAW_EP_IN_ADDR              0x82U
#define HID_RAW_EP_OUT_ADDR             0x02U
#define HID_RAW_EP_SIZE                 32U

#define HID_KB_POLL_INTERVAL            1U
#define HID_RAW_POLL_INTERVAL           1U

#define HID_KB_INTERFACE_NUM            0U
#define HID_RAW_INTERFACE_NUM           1U

#define HID_REPORT_ID_KEYBOARD          0x01U
#define HID_REPORT_ID_NKRO             0x02U

typedef struct {
    uint8_t report_id;
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keycodes[6];
} __attribute__((packed)) HID_KeyboardReport_t;

typedef struct {
    uint8_t report_id;
    uint8_t modifiers;
    uint8_t bitmap[15];
} __attribute__((packed)) HID_NKROReport_t;

typedef struct {
    uint8_t command;
    uint8_t data[31];
} __attribute__((packed)) HID_RawPacket_t;

typedef enum {
    HID_IDLE    = 0x00U,
    HID_BUSY    = 0x01U
} HID_StateTypeDef;

typedef struct {
    HID_StateTypeDef kb_state;
    HID_StateTypeDef raw_state;
    uint8_t          raw_rx_buf[HID_RAW_EP_SIZE];

    uint8_t          protocol;
    uint8_t          idle_rate;
    bool             kb_report_pending;

    /* LED Output report buffer for EP0 data phase */
    uint8_t          led_report_buf[2];
    uint8_t          led_report_len;

    /*
     * USB suspend flag.
     * Set in SuspendCallback, cleared in ResumeCallback.
     * Used by main loop to reduce power and trigger remote wakeup.
     */
    volatile bool    suspended;
} USBD_HID_Custom_HandleTypeDef;

#define HID_CUSTOM_CONFIG_DESC_SIZE     66U

extern USBD_ClassTypeDef USBD_HID_Custom;

USBD_StatusTypeDef USBD_HID_SendKeyboardReport(USBD_HandleTypeDef *pdev,
                                                HID_KeyboardReport_t *report);

USBD_StatusTypeDef USBD_HID_SendNKROReport(USBD_HandleTypeDef *pdev,
                                            HID_NKROReport_t *report);

USBD_StatusTypeDef USBD_HID_SendRawReport(USBD_HandleTypeDef *pdev,
                                           uint8_t *data);

bool USBD_HID_RawDataAvailable(void);
bool USBD_HID_GetRawData(uint8_t *buf);
bool USBD_HID_KeyboardReady(USBD_HandleTypeDef *pdev);
bool USBD_HID_IsSuspended(USBD_HandleTypeDef *pdev);

#ifdef __cplusplus
}
#endif

#endif
